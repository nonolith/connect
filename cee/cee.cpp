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
#define CMD_CONFIG_GAIN 0x65
#define CMD_ISET_DAC 0x15
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
	StreamingDevice(CEE_sample_time),
	USB_device(dev, desc),
	channel_a("a", "A"),
	channel_b("b", "B"),
	channel_a_v("v", "Voltage A", "V",  V_min, V_max,  1, V_max/2048),
	channel_a_i("i", "Current A", "mA", I_min, I_max,  2, 1), // I_max*2/4096, reduced for 9919 bug
	channel_b_v("v", "Voltage B", "V",  V_min, V_max,  1, V_max/2048),
	channel_b_i("i", "Current B", "mA", I_min, I_max,  2, 1)
	{
	cerr << "Found a CEE: "<< serial << endl;
	
	uint8_t buf[64];
	int r;
	r = controlTransfer(0xC0, 0x00, 0, 0, buf, 64);
	if (r >= 0){
		_hwversion = string((char*)buf, r);
	}
	
	r = controlTransfer(0xC0, 0x00, 0, 1, buf, 64);
	if (r >= 0){
		_fwversion = string((char*)buf, r);
	}
	
	readCalibration();
	
	configure(0, CEE_sample_time, ceil(12.0/CEE_sample_time), true, false);
}

CEE_device::~CEE_device(){
	pause_capture();
	delete channel_a.source;
	delete channel_b.source;
}

void CEE_device::readCalibration(){
	union{
		uint8_t buf[64];
		uint32_t magic;
	};
	int r = controlTransfer(0xC0, 0xE0, 0, 0, buf, 64);
	if (!r || magic != EEPROM_VALID_MAGIC){
		cerr << "Reading calibration data failed " << r << endl;
		memset(&cal, 0, sizeof(cal));
	}else{
		memcpy((uint8_t*)&cal, buf, sizeof(cal));
	}
}

bool CEE_device::processMessage(ClientConn& client, string& cmd, JSONNode& n){
	if (cmd == "writeCalibration"){
		cal.offset_a_v = jsonIntProp(n, "offset_a_v");
		cal.offset_a_i = jsonIntProp(n, "offset_a_i");
		cal.offset_b_v = jsonIntProp(n, "offset_b_v");
		cal.offset_b_i = jsonIntProp(n, "offset_b_i");
		cal.dac200_a = jsonIntProp(n, "dac200_a");
		cal.dac200_b = jsonIntProp(n, "dac200_b");
		cal.dac400_a = jsonIntProp(n, "dac400_a");
		cal.dac400_b = jsonIntProp(n, "dac400_b");
		cal.magic = EEPROM_VALID_MAGIC;
		
		int r = controlTransfer(0x40, 0xE1, 0, 0, (uint8_t *)&cal, sizeof(cal));
		
		cerr << "Wrote calibration, " << r << std::endl;
		
		JSONNode reply(JSON_NODE);
		reply.push_back(JSONNode("_action", "return"));
		reply.push_back(JSONNode("id", jsonIntProp(n, "id", 0)));
		reply.push_back(JSONNode("status", r));
		client.sendJSON(reply);
		return true;
		
	}else if (cmd == "tempCalibration"){
		memset(&cal, 0, sizeof(cal));
		cal.offset_a_v = jsonIntProp(n, "offset_a_v", 0);
		cal.offset_a_i = jsonIntProp(n, "offset_a_i", 0);
		cal.offset_b_v = jsonIntProp(n, "offset_b_v", 0);
		cal.offset_b_i = jsonIntProp(n, "offset_b_i", 0);
		cerr << "Applied temporary calibration" << std::endl;
		return true;
		
	}else{
		return USB_device::processMessage(client,cmd,n)
			|| StreamingDevice::processMessage(client,cmd,n);
	}
}

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
		
		// Write current-limit DAC
		if (cal.magic == EEPROM_VALID_MAGIC){
			controlTransfer(0xC0, CMD_ISET_DAC, cal.dac200_a, cal.dac200_b, NULL, 0);
		}
	}
	
	notifyConfig();
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

void CEE_device::setGain(Channel *channel, Stream* stream, int gain){
	uint8_t streamval = 0, gainval=0;
	
	int effectiveGain = gain;
	
	if (stream == &channel_a_i){
		streamval = 0;
		effectiveGain *= 2;
	}else if(stream == &channel_a_v){
		streamval = 1;
	}else if(stream == &channel_b_v){
		streamval = 2;
	}else if(stream == &channel_b_i){
		effectiveGain *= 2;
		streamval = 3;
	}else return;
	
	switch(gain){
		case 1:
			gainval = (0x00<<2);
			break;
		case 2:
			gainval = (0x01<<2);  /* 2x gain */
			break;
		case 4:
    		gainval = (0x02<<2);  /* 4x gain */
    		break;
    	case 8:
    		gainval = (0x03<<2);  /* 8x gain */
    		break;
    	case 16:
    		gainval = (0x04<<2);  /* 16x gain */
    		break;
    	case 32:
    		gainval = (0x05<<2);  /* 32x gain */
    		break;
    	case 64:
    		gainval = (0x06<<2);  /* 64x gain */
    		break;
    	default:
    		return;
	}
	
	stream->gain = gain;
	
	if (captureState){
		on_pause_capture();
	}
	
	controlTransfer(0x40, CMD_CONFIG_GAIN, gainval, streamval, 0, 0);
	
	if (captureState){
		on_start_capture();
	}
	
	std::cerr << "Set gain " << channel->id << " " << stream->id << " "
	    << gain << " " << (int) streamval << " " << (int) gainval << std::endl;
	notifyGainChanged(channel, stream, gain);
}

void CEE_device::handle_in_packet(unsigned char *buffer){
	IN_packet *pkt = (IN_packet*) buffer;
	
	if (pkt->flags & FLAG_PACKET_DROPPED){
		std::cerr << "Warning: dropped packet" << std::endl;
	}
	
	for (int i=0; i<10; i++){
		float v_factor = 5.0/2048.0;
		float i_factor = 2.5/2048.0/CEE_I_gain*1000.0/2;
		
		if (rawMode) v_factor = i_factor = 1;
		
		put(channel_a_v, (cal.offset_a_v + pkt->data[i].av())*v_factor/channel_a_v.gain);
		if ((pkt->mode_a & 0x3) != DISABLED){
			put(channel_a_i, (cal.offset_a_i + pkt->data[i].ai())*i_factor/channel_a_i.gain);
		}else{
			put(channel_a_i, 0);
		}
		put(channel_b_v, (cal.offset_b_v + pkt->data[i].bv())*v_factor/channel_b_v.gain);
		if ((pkt->mode_b & 0x3) != DISABLED){
			put(channel_b_i, (cal.offset_b_i + pkt->data[i].bi())*i_factor/channel_b_i.gain);
		}else{
			put(channel_b_i, 0);
		}
		sampleDone();
	}

	free(buffer);
	packetDone();
	checkOutputEffective(channel_a);
	checkOutputEffective(channel_b);
}

void CEE_device::setOutput(Channel* channel, OutputSource* source){
	{boost::mutex::scoped_lock lock(outputMutex);
		
		source->initialize(capture_o, channel->source);
		
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
	
	uint8_t mode_a = channel_a.source->mode;;
	uint8_t mode_b = channel_b.source->mode;
	
	if (channel_a.source && channel_b.source){
		OUT_packet *pkt = (OUT_packet *)buf;

		pkt->mode_a = mode_a;
		pkt->mode_b = mode_b;

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
