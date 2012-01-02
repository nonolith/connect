// USB data streaming
// http://nonolithlabs.com/cee
// (C) 2011 Kevin Mehall / Nonolith Labs <km@kevinmehall.net>
// Released under the terms of the GNU GPLv3+

#include <stdlib.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <libusb/libusb.h>
#include <sys/timeb.h>
#include <boost/bind.hpp>
using namespace std;

#include "cee.hpp"

#define EP_BULK_IN 0x81
#define EP_BULK_OUT 0x02

#define CMD_CONFIG_CAPTURE 0x80
#define DEVMODE_OFF  0
#define DEVMODE_2SMU 1

long millis(){
	struct timeb tp;
	ftime(&tp);
	return tp.millitm;
}

extern "C" void LIBUSB_CALL in_transfer_callback(libusb_transfer *t);
extern "C" void LIBUSB_CALL out_transfer_callback(libusb_transfer *t);

const int CEE_sample_per = 160;
const int CEE_timer_clock = 4e6; // 4 MHz
const float CEE_sample_time = CEE_sample_per / (float) CEE_timer_clock; // 40us
const float CEE_I_gain = 45*.07;

const float V_min = 0;
const float V_max = 5.0;
const float I_min = -200;
const float I_max = 200;


CEE_device::CEE_device(libusb_device *dev, libusb_device_descriptor &desc):
	Device(CEE_sample_time),
	channel_a("a", "A"),
	channel_b("b", "B"),
	channel_a_v("v", "Voltage A", "V",  V_min, V_max,  1),
	channel_a_i("i", "Current A", "mA", I_min, I_max,  2),
	channel_b_v("v", "Voltage B", "V",  V_min, V_max,  1),
	channel_b_i("i", "Current B", "mA", I_min, I_max,  2)
	{

	int r = libusb_open(dev, &handle);
	if (r != 0){
		cerr << "Could not open device"<<endl;
		return;
	}
	
	r = libusb_claim_interface(handle, 0);
	if (r != 0){
		cerr << "Could not claim interface"<<endl;
		return;
	}

	libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, (unsigned char *) serial, 32);
	cerr << "Found a CEE: "<< serial << endl;
	
	configure(0, CEE_sample_time, 10.0/CEE_sample_time, true, false);
}

CEE_device::~CEE_device(){
	pause_capture();
	libusb_close(handle);
	delete channel_a.source;
	delete channel_b.source;
}

int CEE_device::controlTransfer(uint8_t bmRequestType,
                                uint8_t bRequest,
                                uint16_t wValue,
                                uint16_t wIndex,
                                uint8_t* data,
                                uint16_t wLength){
	return libusb_control_transfer(handle, bmRequestType, bRequest, wValue, wIndex, data, wLength, 100);
};

void CEE_device::configure(int mode, float sampleTime, unsigned samples, bool continuous, bool raw){
	pause_capture();
	
	// Clean up previous configuration
	delete channel_a.source;
	delete channel_b.source;
	channels.clear();
	channel_a.streams.clear();
	channel_b.streams.clear();
	
	// Store state
	captureSamples = samples;
	captureContinuous = continuous;
	devMode = mode;
	rawMode = raw;
	captureLength = captureSamples * CEE_sample_time; //TODO: note sampleTime is currently ignored.
	
	std::cerr << "CEE prepare "<< captureSamples <<" " << captureLength<<"/"<<CEE_sample_time<< std::endl;
	
	// Configure
	if (devMode == 0){
		channels.push_back(&channel_a);
		channel_a.source = new ConstantOutputSource(0, 0);
		channel_a.streams.push_back(&channel_a_v);
		channel_a.streams.push_back(&channel_a_i);
		
		channels.push_back(&channel_b);
		channel_b.source = new ConstantOutputSource(0, 0);
		channel_b.streams.push_back(&channel_b_v);
		channel_b.streams.push_back(&channel_b_i);
		
		if (rawMode){
			channel_a_v.units = channel_b_v.units = string("LSB");
			channel_a_i.units = channel_b_i.units = string("LSB");
			
			channel_a_v.min = channel_b_v.min = -100;
			channel_a_v.max = channel_b_v.max = 2047;
			channel_a_i.min = channel_b_i.min = -2048;
			channel_a_i.max = channel_b_i.max = 2047;
		}else{
			channel_a_v.units = channel_b_v.units = string("V");
			channel_a_i.units = channel_b_i.units = string("mA");
			
			channel_a_v.min = channel_b_v.min = V_min;
			channel_a_v.max = channel_b_v.max = V_max;
			channel_a_i.min = channel_b_i.min = I_min;
			channel_a_i.max = channel_b_i.max = I_max;
		}
		
		channel_a_v.allocate(captureSamples);
		channel_a_i.allocate(captureSamples);
		channel_b_v.allocate(captureSamples);
		channel_b_i.allocate(captureSamples);
	}
	
	notifyConfig();
	reset_capture();
}

void CEE_device::on_reset_capture(){
	boost::mutex::scoped_lock lock(outputMutex);
	incount = outcount = 0;
	if (channel_a.source) channel_a.source->startSample=0;
	if (channel_b.source) channel_b.source->startSample=0;
}

void CEE_device::on_start_capture(){
	boost::mutex::scoped_lock lock(transfersMutex);
	
	// Turn on the device
	controlTransfer(0x40, CMD_CONFIG_CAPTURE, CEE_sample_per, DEVMODE_2SMU, 0, 0);
	
	// Ignore the effect of output samples we sent before pausing
	capture_o = capture_i; 
	
	for (int i=0; i<N_TRANSFERS; i++){
		in_transfers[i] = libusb_alloc_transfer(0);
		unsigned char* buf = (unsigned char*) malloc(sizeof(IN_packet));
		libusb_fill_bulk_transfer(in_transfers[i], handle, EP_BULK_IN, buf, 64, in_transfer_callback, this, 5000);
		in_transfers[i]->flags |= LIBUSB_TRANSFER_FREE_BUFFER;
		libusb_submit_transfer(in_transfers[i]);
		
		out_transfers[i] = libusb_alloc_transfer(0);
		buf = (unsigned char *) malloc(sizeof(OUT_packet));
		fill_out_packet(buf);
		outcount++;
		libusb_fill_bulk_transfer(out_transfers[i], handle, EP_BULK_OUT, buf, 32, out_transfer_callback, this, 5000);
		out_transfers[i]->flags |= LIBUSB_TRANSFER_FREE_BUFFER;
		libusb_submit_transfer(out_transfers[i]);
	}
}

void CEE_device::on_pause_capture(){
	boost::mutex::scoped_lock lock(transfersMutex);

	std::cerr << "on_pause_capture " << capture_i << " " << capture_o <<std::endl;
	
	controlTransfer(0x40, CMD_CONFIG_CAPTURE, CEE_sample_per, DEVMODE_OFF, 0, 0);
	
	for (int i=0; i<N_TRANSFERS; i++){
		if (in_transfers[i]){
			// zero user_data tells the callback to free on complete. this obj may be dead by then
			in_transfers[i]->user_data=0;

			libusb_cancel_transfer(in_transfers[i]);
			in_transfers[i] = 0;
		}
		
		if (out_transfers[i]){
			// zero user_data tells the callback to free on complete. this obj may be dead by then
			out_transfers[i]->user_data=0;
			
			libusb_cancel_transfer(out_transfers[i]);
			out_transfers[i] = 0;
		}
	}
}

void CEE_device::handle_in_packet(unsigned char *buffer){
	IN_packet *pkt = (IN_packet*) buffer;
	for (int i=0; i<10; i++){
		float v_factor = 5.0/2048.0;
		float i_factor = 2.5/2048.0/CEE_I_gain*1000.0;
		
		if (rawMode) v_factor = i_factor = 1;
	
		put(channel_a_v, pkt->data[i].av()*v_factor);
		put(channel_a_i, pkt->data[i].ai()*i_factor);
		put(channel_b_v, pkt->data[i].bv()*v_factor);
		put(channel_b_i, pkt->data[i].bi()*i_factor);
		sampleDone();
	}

	free(buffer);
	packetDone();
	
	checkOutputEffective(channel_a);
	checkOutputEffective(channel_b);
}

void CEE_device::setOutput(Channel* channel, OutputSource* source){
	{
		boost::mutex::scoped_lock lock(outputMutex);
		if (channel->source){
			delete channel->source;
		}
		channel->source=source;
		channel->source->startSample = capture_o;
	}
	notifyOutputChanged(channel, source);
}

inline void CEE_device::checkOutputEffective(Channel& channel){
	if (!channel.source->effective && capture_i > channel.source->startSample){
		channel.source->effective = true;
		notifyOutputChanged(&channel, channel.source);
	}
}

inline float constrain(float val, float lo, float hi){
	if (val > hi) val = hi;
	if (val < lo) val = lo;
	return val;
}


uint16_t CEE_device::encode_out(CEE_chanmode mode, float val){
	if (rawMode){
		return constrain(val, 0, 4095);
	}else{
		if (mode == SVMI){
			val = constrain(val, V_min, V_max);
			return 4095*val/5.0;
		}else if (mode == SIMV){
			val = constrain(val, I_min, I_max);
			return 4095*(1.25+CEE_I_gain*val/1000.0)/2.5;
		}
	}
	return 0;
}

void CEE_device::fill_out_packet(unsigned char* buf){
	boost::mutex::scoped_lock lock(outputMutex);
	if (channel_a.source && channel_b.source){
		unsigned mode_a = channel_a.source->mode;
		unsigned mode_b = channel_b.source->mode;

		OUT_packet *pkt = (OUT_packet *)buf;

		pkt->flags = (mode_b<<4) | mode_a;

		for (int i=0; i<10; i++){
			pkt->data[i].pack(
				encode_out((CEE_chanmode)mode_a, channel_a.source->getValue(capture_o, CEE_sample_time)),
				encode_out((CEE_chanmode)mode_b, channel_b.source->getValue(capture_o, CEE_sample_time))
			);
			capture_o++;
		}	
	}else{
		memset(buf, 0, 32);
	}
	
}

void destroy_transfer(CEE_device *dev, libusb_transfer** list, libusb_transfer* t){
	boost::mutex::scoped_lock lock(dev->transfersMutex);
	t->user_data = 0;
	for (int i=0; i<N_TRANSFERS; i++){
		if (list[i] == t){
			list[i] = 0;
		}
	}
	//cerr << "Freeing lpacket "<< t << " " << t->status << endl;
	libusb_free_transfer(t);
}


#define DISABLE_SELF_STOP 1

/// Runs in USB thread
extern "C" void LIBUSB_CALL in_transfer_callback(libusb_transfer *t){
	if (!t->user_data){
		//cerr << "Freeing in packet "<< t << " " << t->status << endl;
		libusb_free_transfer(t); // user_data was zeroed out when device was deleted
		return;
	}

	CEE_device *dev = (CEE_device *) t->user_data;

	if (t->status == LIBUSB_TRANSFER_COMPLETED){
		//cerr <<  millis() << " " << t << " complete " << t->actual_length << endl;
		io.post(boost::bind(&CEE_device::handle_in_packet, dev, t->buffer));
		t->buffer = (unsigned char*) malloc(sizeof(IN_packet));

		if (DISABLE_SELF_STOP || dev->captureContinuous || dev->incount*IN_SAMPLES_PER_PACKET < dev->captureSamples){
			dev->incount++;
			libusb_submit_transfer(t);
		}else{
			// don't submit more transfers, but wait for all the transfers to complete
			cerr << "Queued last in packet" << endl;
			destroy_transfer(dev, dev->in_transfers, t);
		}
	}else{
		cerr << "ITransfer error "<< t->status << " " << t << endl;
		//TODO: notify main thread of error
		destroy_transfer(dev, dev->in_transfers, t);
	}
}


/// Runs in USB thread
extern "C" void LIBUSB_CALL out_transfer_callback(libusb_transfer *t){
	if (!t->user_data){
		//cerr << "Freeing out packet "<< t << " " << t->status << endl;
		libusb_free_transfer(t); // user_data was zeroed out when device was deleted
		return;
	}

	CEE_device *dev = (CEE_device *) t->user_data;

	if (t->status == LIBUSB_TRANSFER_COMPLETED){
		if (DISABLE_SELF_STOP || dev->captureContinuous || dev->outcount*OUT_SAMPLES_PER_PACKET < dev->captureSamples){
			dev->fill_out_packet(t->buffer);
			dev->outcount++;
			libusb_submit_transfer(t);
		}
		//cerr << outcount << " " << millis() << " " << t << " sent " << t->actual_length << endl;
	}else{
		cerr << "OTransfer error "<< t->status << " " << t << endl;
		destroy_transfer(dev, dev->out_transfers, t);
	}
}
