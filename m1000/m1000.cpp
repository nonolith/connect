// Nonolith Connect
// https://github.com/nonolith/connect
// USB Data streaming for CEE
// Released under the terms of the GNU GPLv3+
// (C) 2012 Nonolith Labs, LLC
// Authors:
//   Kevin Mehall <kevin@nonolithlabs.com>
//   Ian Daniher <ian@nonolithlabs.com>

#include <stdlib.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <sys/timeb.h>
#include <boost/bind.hpp>
using namespace std;

#include "m1000.hpp"
#include <libusb/libusb.h>

#define EP_BULK_IN 0x81
#define EP_BULK_OUT 0x02

const unsigned chunk_size = 256;
#define OUT_SAMPLES_PER_PACKET chunk_size
#define IN_SAMPLES_PER_PACKET chunk_size

extern "C" void LIBUSB_CALL m1000_in_transfer_callback(libusb_transfer *t);
extern "C" void LIBUSB_CALL m1000_out_transfer_callback(libusb_transfer *t);

const double m1000_default_sample_time = 1/48000.0;

const float V_min = 0;
const float V_max = 5.0;
const float I_min = -200;
const float I_max = 200;
const int defaultCurrentLimit = 200;

#ifdef _WIN32
const double BUFFER_TIME = 0.050;
#else
const double BUFFER_TIME = 0.020;
#endif

typedef struct IN_packet{
	uint16_t data_a_v[256];
	uint16_t data_a_i[256];
	uint16_t data_b_v[256];
	uint16_t data_b_i[256];
} IN_packet;

typedef struct OUT_packet{
	uint16_t data_a[256];
	uint16_t data_b[256];
} OUT_packet;

M1000_device::M1000_device(libusb_device *dev, libusb_device_descriptor &desc):
	StreamingDevice(m1000_default_sample_time),
	USB_device(dev, desc),
	channel_a("a", "A"),
	channel_b("b", "B"),
	
	//          id   name         unit  min    max    omode uncert.  gain
	channel_a_v("v", "Voltage A", "V",  V_min, V_max, 1,  V_max/2048, 1),
	channel_a_i("i", "Current A", "mA", 0,     0,     2,  1,          1),
	channel_b_v("v", "Voltage B", "V",  V_min, V_max, 1,  V_max/2048, 1),
	channel_b_i("i", "Current B", "mA", 0,     0,     2,  1,          1)
	{
	cerr << "Found M1000: \n    Serial: "<< serial << endl;
	
	configure(0, m1000_default_sample_time, ceil(12.0/m1000_default_sample_time), true, false);
}

M1000_device::~M1000_device(){
	pause_capture();
	delete channel_a.source;
	delete channel_b.source;
}


bool M1000_device::processMessage(ClientConn& client, string& cmd, JSONNode& n){
		return USB_device::processMessage(client,cmd,n)
			|| StreamingDevice::processMessage(client,cmd,n);
}

void M1000_device::configure(int mode, double _sampleTime, unsigned samples, bool continuous, bool raw){
	pause_capture();
	
	// Clean up previous configuration
	delete channel_a.source;
	delete channel_b.source;
	channels.clear();
	channel_a.streams.clear();
	channel_b.streams.clear();
	
	sampleTime = m1000_default_sample_time;
	
	captureSamples = samples;
	captureContinuous = continuous;
	devMode = mode;
	rawMode = raw;
	captureLength = captureSamples * sampleTime;
	
	ntransfers = 4;
	packets_per_transfer = 1;
	
	capture_i = capture_o = 0;
		
	// Configure
	if (devMode == 0){
		channels.push_back(&channel_a);
		channel_a.source = makeConstantSource(0, 0);
		channel_a.streams.push_back(&channel_a_v);
		channel_a.streams.push_back(&channel_a_i);
		
		channels.push_back(&channel_b);
		channel_b.source = makeConstantSource(0, 0);
		channel_b.streams.push_back(&channel_b_v);
		channel_b.streams.push_back(&channel_b_i);
		
		if (rawMode){
			channel_a_v.units = channel_b_v.units = string("LSB");
			channel_a_i.units = channel_b_i.units = string("LSB");
			
			channel_a_v.min = channel_b_v.min = 0;
			channel_a_v.max = channel_b_v.max = 65535;
			channel_a_i.min = channel_b_i.min = 0;
			channel_a_i.max = channel_b_i.max = 65535;
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
}


void M1000_device::on_reset_capture(){
	boost::mutex::scoped_lock lock(outputMutex);
	incount = outcount = 0;
	if (channel_a.source) channel_a.source->startSample=0;
	if (channel_b.source) channel_b.source->startSample=0;
}

void M1000_device::on_start_capture(){
	claimInterface();
	boost::mutex::scoped_lock lock(transfersMutex);
	
	libusb_set_interface_alt_setting(handle, 0, 1);
	
	uint8_t buf[4];
	// set pots for sane simv
	controlTransfer(0x40|0x80, 0x1B, 0x0707, 'a', buf, 4, 100);
	// set adcs for bipolar sequenced mode
	controlTransfer(0x40|0x80, 0xCA, 0xF1C0, 0xF5C0, buf, 1, 100);
	controlTransfer(0x40|0x80, 0xCB, 0xF1C0, 0xF5C0, buf, 1, 100);
	controlTransfer(0x40|0x80, 0xCD, 0x0000, 0x0100, buf, 1, 100);
	// set timer for 1us keepoff, 20us period
	controlTransfer(0x40|0x80, 0xC5, 0x0004, 0x001E, buf, 1, 100);

	// Ignore the effect of output samples we sent before pausing
	capture_o = capture_i;
	
	firstPacket = true;
	
	for (int i=0; i<ntransfers; i++){
		in_transfers[i] = libusb_alloc_transfer(0);
		const int isize = sizeof(IN_packet)*packets_per_transfer;
		unsigned char* buf = (unsigned char*) malloc(isize);
		libusb_fill_bulk_transfer(in_transfers[i], handle, EP_BULK_IN, buf, isize, m1000_in_transfer_callback, this, 500);
		in_transfers[i]->flags |= LIBUSB_TRANSFER_FREE_BUFFER;
		int r = libusb_submit_transfer(in_transfers[i]);
		
		if (r) cerr << "libusb_submit_transfer i: " << r << endl;
		
		out_transfers[i] = libusb_alloc_transfer(0);
		const int osize = sizeof(OUT_packet)*packets_per_transfer;
		buf = (unsigned char *) malloc(osize);
		fillOutTransfer(buf);
		outcount++;
		libusb_fill_bulk_transfer(out_transfers[i], handle, EP_BULK_OUT, buf, osize, m1000_out_transfer_callback, this, 500);
		out_transfers[i]->flags |= LIBUSB_TRANSFER_FREE_BUFFER;
		r = libusb_submit_transfer(out_transfers[i]);
		if (r) cerr << "libusb_submit_transfer o: " << r << endl;
		
	}
}

void M1000_device::on_pause_capture(){
	boost::mutex::scoped_lock lock(transfersMutex);

	//std::cerr << "on_pause_capture " << capture_i << " " << capture_o <<std::endl;
	
	uint8_t buf[4];
	controlTransfer(0x40|0x80, 0xC5, 0x0000, 0x0000, buf, 1, 100);
	
	for (int i=0; i<ntransfers; i++){
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
	
	capture_o = capture_i;

	releaseInterface();
}

void M1000_device::handleInTransfer(unsigned char *buffer){
			
	for (int p=0; p<packets_per_transfer; p++){
		uint16_t* buf = (uint16_t*) buffer + p*sizeof(IN_packet);

		firstPacket = false;
	
		for (int i=0; i<chunk_size; i++){
			put(channel_a_v,  be16toh(buf[i+chunk_size*0]) / 65535.0 * 5.0);
			put(channel_a_i, (be16toh(buf[i+chunk_size*1]) / 65535.0 - 0.5) * 400.0 );
			put(channel_b_v,  be16toh(buf[i+chunk_size*2]) / 65535.0 * 5.0);
			put(channel_b_i, (be16toh(buf[i+chunk_size*3]) / 65535.0 - 0.5) * 400.0 );
			
			sampleDone();
		}
	}

	free(buffer);
	packetDone();
	checkOutputEffective(channel_a);
	checkOutputEffective(channel_b);
}

void M1000_device::setOutput(Channel* channel, OutputSource* source){
	{boost::mutex::scoped_lock lock(outputMutex);
		
		source->initialize(capture_o, channel->source);
		
		if (channel->source){
			delete channel->source;
		}
		channel->source=source;
		channel->source->startSample = capture_o + 1;
	}
	notifyOutputChanged(channel, source);
}

inline void M1000_device::checkOutputEffective(Channel& channel){
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


uint16_t M1000_device::encode_out(Chanmode mode, float val, uint32_t igain){
	if (rawMode){
		return constrain(val, 0, 65535);
	}else{
		int v = 0;
		if (mode == SVMI){
			val = constrain(val, V_min, V_max);
			v = 65535*val/5.0;
		}else if (mode == SIMV){
			val = constrain(val, -currentLimit, currentLimit);
			v = 65535*((val + 200.0)/400.0);
		}
		if (v > 65535) v=65535;
		if (v < 0) v = 0;
		return v;
	}
	return 0;
}

void M1000_device::fillOutTransfer(unsigned char* buf){
	boost::mutex::scoped_lock lock(outputMutex);
	
	uint8_t mode_a = channel_a.source->mode;
	uint8_t mode_b = channel_b.source->mode;
	
	if (channel_a.source && channel_b.source){
		for (int p=0; p<packets_per_transfer; p++){
			OUT_packet *pkt = &((OUT_packet *)buf)[p];

			for (int i=0; i<chunk_size; i++){
				pkt->data_a[i] = htobe16(encode_out((Chanmode)mode_a, channel_a.source->getValue(capture_o, sampleTime), 1));
				pkt->data_b[i] = htobe16(encode_out((Chanmode)mode_b, channel_b.source->getValue(capture_o, sampleTime), 1));
				capture_o++;
			}	
		}
	}else{
		memset(buf, 0, sizeof(OUT_packet)*packets_per_transfer);
	}
}

void destroy_transfer(M1000_device *dev, libusb_transfer** list, libusb_transfer* t){
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
extern "C" void LIBUSB_CALL m1000_in_transfer_callback(libusb_transfer *t){
	if (!t->user_data){
		//cerr << "Freeing in packet "<< t << " " << t->status << endl;
		libusb_free_transfer(t); // user_data was zeroed out when device was deleted
		return;
	}

	M1000_device *dev = (M1000_device *) t->user_data;

	if (t->status == LIBUSB_TRANSFER_COMPLETED){
		io.post(boost::bind(&M1000_device::handleInTransfer, dev, t->buffer));
		t->buffer = (unsigned char*) malloc(sizeof(IN_packet) * dev->packets_per_transfer);

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
extern "C" void LIBUSB_CALL m1000_out_transfer_callback(libusb_transfer *t){
	if (!t->user_data){
		//cerr << "Freeing out packet "<< t << " " << t->status << endl;
		libusb_free_transfer(t); // user_data was zeroed out when device was deleted
		return;
	}

	M1000_device *dev = (M1000_device *) t->user_data;

	if (t->status == LIBUSB_TRANSFER_COMPLETED){
		if (DISABLE_SELF_STOP || dev->captureContinuous || dev->outcount*OUT_SAMPLES_PER_PACKET < dev->captureSamples){
			dev->fillOutTransfer(t->buffer);
			dev->outcount++;
			libusb_submit_transfer(t);
		}
		//cerr << dev->outcount << " " << t << " sent " << t->actual_length << endl;
	}else{
		cerr << "OTransfer error "<< t->status << " " << t << endl;
		destroy_transfer(dev, dev->out_transfers, t);
	}
}
