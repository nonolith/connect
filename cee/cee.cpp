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

const int CEE_timer_clock = 4e6; // 4 MHz
const double CEE_default_sample_time = 1/10000.0;
const double CEE_current_gain_scale = 100000;
const uint32_t CEE_default_current_gain = 45*.07*CEE_current_gain_scale;

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

CEE_device::CEE_device(libusb_device *dev, libusb_device_descriptor &desc):
	StreamingDevice(CEE_default_sample_time),
	USB_device(dev, desc),
	channel_a("a", "A"),
	channel_b("b", "B"),
	
	//          id   name         unit  min    max    omode uncert.  gain
	channel_a_v("v", "Voltage A", "V",  V_min, V_max, 1,  V_max/2048, 1),
	channel_a_i("i", "Current A", "mA", 0,     0,     2,  1,          2),
	channel_b_v("v", "Voltage B", "V",  V_min, V_max, 1,  V_max/2048, 1),
	channel_b_i("i", "Current B", "mA", 0,     0,     2,  1,          2)
	{
	cerr << "Found a CEE: \n    Serial: "<< serial << endl;
	
	char buf[64];
	int r;
	r = controlTransfer(0xC0, 0x00, 0, 0, (uint8_t*)buf, 64);
	if (r >= 0){
		_hwversion = string(buf, strnlen(buf, r));
	}

	r = controlTransfer(0xC0, 0x00, 0, 1, (uint8_t*)buf, 64);
	if (r >= 0){
		_fwversion = string(buf, strnlen(buf, r));
	}

	CEE_version_descriptor version_info;
	bool have_version_info = 0;

	if (_fwversion >= "1.2"){
		r = controlTransfer(0xC0, 0x00, 0, 0xff, (uint8_t*)&version_info, sizeof(version_info));
		have_version_info = (r>=0);

		r = controlTransfer(0xC0, 0x00, 0, 2, (uint8_t*)buf, 64);
		if (r >= 0){
			_gitversion = string(buf, strnlen(buf, r));
		}
	}

	if (have_version_info){
		min_per = version_info.min_per;
		if (version_info.per_ns != 250){
			std::cerr << "    Error: alternate timer clock " << version_info.per_ns << " is not supported in this release." << std::endl;
		}

	}else{
		min_per = 100;
	}

	minSampleTime = min_per / (double) CEE_timer_clock;

	std::cout << "    Hardware: " << _hwversion << std::endl;
	std::cout << "    Firmware version: " << _fwversion << " (" << _gitversion << ")" << std::endl;
	std::cout << "    Supported sample rate: " << CEE_timer_clock / min_per / 1000.0 << "ksps" << std::endl;
 	
	// Reset the state
	controlTransfer(0x40, CMD_CONFIG_CAPTURE, 0, DEVMODE_OFF, 0, 0);

	// Reset the gains
	controlTransfer(0x40, CMD_CONFIG_GAIN, (0x01<<2), 0, 0, 0);
	controlTransfer(0x40, CMD_CONFIG_GAIN, (0x00<<2), 1, 0, 0);
	controlTransfer(0x40, CMD_CONFIG_GAIN, (0x00<<2), 2, 0, 0);
	controlTransfer(0x40, CMD_CONFIG_GAIN, (0x01<<2), 3, 0, 0);

	readCalibration();
	
	configure(0, CEE_default_sample_time, ceil(12.0/CEE_default_sample_time), true, false);
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
		cerr << "    Reading calibration data failed " << r << endl;
		memset(&cal, 0xff, sizeof(cal));
		
		cal.offset_a_v = cal.offset_a_i = cal.offset_b_v = cal.offset_b_i = 0;
		cal.dac400_a = cal.dac400_b = cal.dac200_a = cal.dac200_b = 0x6B7;
	}else{
		memcpy((uint8_t*)&cal, buf, sizeof(cal));
	}
	
	setCurrentLimit((cal.flags&EEPROM_FLAG_USB_POWER)?defaultCurrentLimit:2000);
	
	if (cal.current_gain_a == (uint32_t) -1){
		cal.current_gain_a = CEE_default_current_gain;
	}
	
	if (cal.current_gain_b == (uint32_t) -1){
		cal.current_gain_b = CEE_default_current_gain;
	}
	
	cerr << "    Current gain " << cal.current_gain_a << " " << cal.current_gain_b << endl;
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
		cal.current_gain_a = jsonIntProp(n, "current_gain_a", (uint32_t) -1);
		cal.current_gain_b = jsonIntProp(n, "current_gain_b", (uint32_t) -1);
		cal.flags = jsonIntProp(n, "flags", 0xff);
		cal.magic = EEPROM_VALID_MAGIC;
		
		int r = controlTransfer(0x40, 0xE1, 0, 0, (uint8_t *)&cal, sizeof(cal));
		
		cerr << "Wrote calibration, " << r << std::endl;
		
		JSONNode reply(JSON_NODE);
		reply.push_back(JSONNode("_action", "return"));
		reply.push_back(JSONNode("id", jsonIntProp(n, "id", 0)));
		reply.push_back(JSONNode("status", r));
		client.sendJSON(reply);
		return true;
		
	}else if (cmd == "readCalibration"){
		JSONNode reply(JSON_NODE);
		reply.push_back(JSONNode("_action", "return"));
		reply.push_back(JSONNode("id", jsonIntProp(n, "id", 0)));
		
		reply.push_back(JSONNode("offset_a_v", cal.offset_a_v));
		reply.push_back(JSONNode("offset_a_i", cal.offset_a_i));
		reply.push_back(JSONNode("offset_b_v", cal.offset_b_v));
		reply.push_back(JSONNode("offset_b_i", cal.offset_b_i));
		reply.push_back(JSONNode("dac200_a", cal.dac200_a));
		reply.push_back(JSONNode("dac200_b", cal.dac200_b));
		reply.push_back(JSONNode("dac400_a", cal.dac400_a));
		reply.push_back(JSONNode("dac400_b", cal.dac400_b));
		reply.push_back(JSONNode("current_gain_a", cal.current_gain_a));
		reply.push_back(JSONNode("current_gain_b", cal.current_gain_b));
		reply.push_back(JSONNode("flags", cal.flags));
		
		client.sendJSON(reply);	
		return true;
			
	}else if (cmd == "tempCalibration"){
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

JSONNode CEE_device::gpio(bool set, uint8_t dir, uint8_t out){
	uint8_t buf[4];
	JSONNode j;
	int r = controlTransfer(0xC0, set?0x21:0x20, out, dir, buf, 4);
	j.push_back(JSONNode("status", r));
	j.push_back(JSONNode("in", buf[0]));
	j.push_back(JSONNode("dir", buf[1]));
	j.push_back(JSONNode("out", buf[2]));
	return j;
}

void CEE_device::handleRESTGPIOCallback(websocketpp::session_ptr client, string postdata){
	try{
		std::map<string, string> map;
		parse_query(postdata, map);
		uint8_t dir = map_get_num(map, "dir", 0) & 0xFF;
		uint8_t out = map_get_num(map, "out", 0) & 0xFF;
		JSONNode j = gpio(true, dir, out);
		respondJSON(client, j);
	}catch(std::exception& e){
		respondError(client, e);
	}
}

bool CEE_device::handleREST(UrlPath path, websocketpp::session_ptr client){
	if (!path.leaf() && path.matches("gpio")){
		if (client->get_method() == "POST"){
			client->read_http_post_body(
				boost::bind(
					&CEE_device::handleRESTGPIOCallback,
					boost::static_pointer_cast<CEE_device>(shared_from_this()),
					client,  _1));
		}else{
			JSONNode j = gpio(false);
			respondJSON(client, j);
		}
		return true;
	}else{
		return StreamingDevice::handleREST(path, client);
	}
}

void CEE_device::configure(int mode, double _sampleTime, unsigned samples, bool continuous, bool raw){
	pause_capture();
	
	// Clean up previous configuration
	delete channel_a.source;
	delete channel_b.source;
	channels.clear();
	channel_a.streams.clear();
	channel_b.streams.clear();
	
	// Store state
	xmega_per = round(_sampleTime * (double) CEE_timer_clock);
	if (xmega_per < min_per) xmega_per = min_per;
	sampleTime = xmega_per / (double) CEE_timer_clock; // convert back to get the actual sample time;
	
	captureSamples = samples;
	captureContinuous = continuous;
	devMode = mode;
	rawMode = raw;
	captureLength = captureSamples * sampleTime;
	
	ntransfers = 4;
	packets_per_transfer = ceil(BUFFER_TIME / (sampleTime * 10) / ntransfers);
	
	capture_i = capture_o = 0;
	
	std::cerr << "CEE prepare "<< xmega_per << " " << ntransfers <<  " " << packets_per_transfer << " " << captureSamples << " " << currentLimit << std::endl;
	
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
			
			double effectiveLimitA = 2.5/(cal.current_gain_a/CEE_current_gain_scale)/channel_a_i.normalGain*1000;
			if (effectiveLimitA > currentLimit) effectiveLimitA = currentLimit;
			
			double effectiveLimitB = 2.5/(cal.current_gain_b/CEE_current_gain_scale)/channel_b_i.normalGain*1000;
			if (effectiveLimitB > currentLimit) effectiveLimitB = currentLimit;
			
			cerr << "    Current axis " << effectiveLimitA << " " <<effectiveLimitB << endl;
			
			channel_a_v.min = channel_b_v.min = V_min;
			channel_a_v.max = channel_b_v.max = V_max;
			channel_a_i.min = -effectiveLimitA;
			channel_b_i.min = -effectiveLimitB;
			channel_a_i.max =  effectiveLimitA;
			channel_b_i.max =  effectiveLimitB;
		}
		
		channel_a_v.allocate(captureSamples);
		channel_a_i.allocate(captureSamples);
		channel_b_v.allocate(captureSamples);
		channel_b_i.allocate(captureSamples);
	}
	
	notifyConfig();
}

void CEE_device::setCurrentLimit(unsigned mode){
	unsigned ilimit_cal_a, ilimit_cal_b;

	if (mode == 200){
		ilimit_cal_a = cal.dac200_a;
		ilimit_cal_b = cal.dac200_b;
	}else if(mode == 400){
		ilimit_cal_a = cal.dac400_a;
		ilimit_cal_b = cal.dac400_b;
	}else if (mode == 2000){
		ilimit_cal_a = ilimit_cal_b = 0;
	}else{
		std::cerr << "Invalid current limit " << mode << std::endl;
		return;
	}
	
	controlTransfer(0xC0, CMD_ISET_DAC, ilimit_cal_a, ilimit_cal_b, NULL, 0);	
	
	currentLimit = mode;
}

void CEE_device::on_reset_capture(){
	boost::mutex::scoped_lock lock(outputMutex);
	incount = outcount = 0;
	if (channel_a.source) channel_a.source->startSample=0;
	if (channel_b.source) channel_b.source->startSample=0;
}

void CEE_device::on_start_capture(){
	claimInterface();
	boost::mutex::scoped_lock lock(transfersMutex);
	
	// Turn on the device
	controlTransfer(0x40, CMD_CONFIG_CAPTURE, xmega_per, DEVMODE_2SMU, 0, 0);
	
	// Ignore the effect of output samples we sent before pausing
	capture_o = capture_i;
	
	firstPacket = true;
	
	for (int i=0; i<ntransfers; i++){
		in_transfers[i] = libusb_alloc_transfer(0);
		const int isize = sizeof(IN_packet)*packets_per_transfer;
		unsigned char* buf = (unsigned char*) malloc(isize);
		libusb_fill_bulk_transfer(in_transfers[i], handle, EP_BULK_IN, buf, isize, in_transfer_callback, this, 500);
		in_transfers[i]->flags |= LIBUSB_TRANSFER_FREE_BUFFER;
		libusb_submit_transfer(in_transfers[i]);
		
		out_transfers[i] = libusb_alloc_transfer(0);
		const int osize = sizeof(OUT_packet)*packets_per_transfer;
		buf = (unsigned char *) malloc(osize);
		fillOutTransfer(buf);
		outcount++;
		libusb_fill_bulk_transfer(out_transfers[i], handle, EP_BULK_OUT, buf, osize, out_transfer_callback, this, 500);
		out_transfers[i]->flags |= LIBUSB_TRANSFER_FREE_BUFFER;
		libusb_submit_transfer(out_transfers[i]);
	}
}

void CEE_device::on_pause_capture(){
	boost::mutex::scoped_lock lock(transfersMutex);

	//std::cerr << "on_pause_capture " << capture_i << " " << capture_o <<std::endl;
	
	controlTransfer(0x40, CMD_CONFIG_CAPTURE, 0, DEVMODE_OFF, 0, 0);
	
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

void CEE_device::setInternalGain(Channel *channel, Stream* stream, int gain){
	uint8_t streamval = 0, gainval=0;
	
	if (stream == &channel_a_i){
		streamval = 0;
	}else if(stream == &channel_a_v){
		streamval = 1;
	}else if(stream == &channel_b_v){
		streamval = 2;
	}else if(stream == &channel_b_i){
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

void CEE_device::handleInTransfer(unsigned char *buffer){
	float v_factor = 5.0/2048.0;
	float i_factor_a = 2.5/2048.0/(cal.current_gain_a/CEE_current_gain_scale)*1000.0;
	float i_factor_b = 2.5/2048.0/(cal.current_gain_b/CEE_current_gain_scale)*1000.0;
	if (rawMode) v_factor = i_factor_a = i_factor_b = 1;
			
	for (int p=0; p<packets_per_transfer; p++){
		IN_packet *pkt = &((IN_packet*)buffer)[p];
	
		if ((pkt->flags & FLAG_PACKET_DROPPED) && !firstPacket){
			std::cerr << "Warning: dropped packet" << std::endl;
			JSONNode j;
			j.push_back(JSONNode("_action", "packetDrop"));
			broadcastJSON(j);
		}

		firstPacket = false;
	
		for (int i=0; i<10; i++){
			put(channel_a_v, (cal.offset_a_v + pkt->data[i].av())*v_factor/channel_a_v.gain);
			if ((pkt->mode_a & 0x3) != DISABLED){
				put(channel_a_i, (cal.offset_a_i + pkt->data[i].ai())*i_factor_a/channel_a_i.gain);
			}else{
				put(channel_a_i, 0);
			}
			put(channel_b_v, (cal.offset_b_v + pkt->data[i].bv())*v_factor/channel_b_v.gain);
			if ((pkt->mode_b & 0x3) != DISABLED){
				put(channel_b_i, (cal.offset_b_i + pkt->data[i].bi())*i_factor_b/channel_b_i.gain);
			}else{
				put(channel_b_i, 0);
			}
			sampleDone();
		}
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
		channel->source->startSample = capture_o + 1;
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


uint16_t CEE_device::encode_out(CEE_chanmode mode, float val, uint32_t igain){
	if (rawMode){
		return constrain(val, 0, 4095);
	}else{
		int v = 0;
		if (mode == SVMI){
			val = constrain(val, V_min, V_max);
			v = 4095*val/5.0;
		}else if (mode == SIMV){
			val = constrain(val, -currentLimit, currentLimit);
			v = 4095*(1.25+(igain/CEE_current_gain_scale)*val/1000.0)/2.5;
		}
		if (v > 4095) v=4095;
		if (v < 0) v = 0;
		return v;
	}
	return 0;
}

void CEE_device::fillOutTransfer(unsigned char* buf){
	boost::mutex::scoped_lock lock(outputMutex);
	
	uint8_t mode_a = channel_a.source->mode;
	uint8_t mode_b = channel_b.source->mode;
	
	if (channel_a.source && channel_b.source){
		for (int p=0; p<packets_per_transfer; p++){
			OUT_packet *pkt = &((OUT_packet *)buf)[p];

			pkt->mode_a = mode_a;
			pkt->mode_b = mode_b;

			for (int i=0; i<10; i++){
				pkt->data[i].pack(
					encode_out((CEE_chanmode)mode_a, channel_a.source->getValue(capture_o, sampleTime), cal.current_gain_a),
					encode_out((CEE_chanmode)mode_b, channel_b.source->getValue(capture_o, sampleTime), cal.current_gain_b)
				);
				capture_o++;
			}	
		}
	}else{
		memset(buf, 0, sizeof(OUT_packet)*packets_per_transfer);
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
		io.post(boost::bind(&CEE_device::handleInTransfer, dev, t->buffer));
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
extern "C" void LIBUSB_CALL out_transfer_callback(libusb_transfer *t){
	if (!t->user_data){
		//cerr << "Freeing out packet "<< t << " " << t->status << endl;
		libusb_free_transfer(t); // user_data was zeroed out when device was deleted
		return;
	}

	CEE_device *dev = (CEE_device *) t->user_data;

	if (t->status == LIBUSB_TRANSFER_COMPLETED){
		if (DISABLE_SELF_STOP || dev->captureContinuous || dev->outcount*OUT_SAMPLES_PER_PACKET < dev->captureSamples){
			dev->fillOutTransfer(t->buffer);
			dev->outcount++;
			libusb_submit_transfer(t);
		}
		//cerr << outcount << " " << millis() << " " << t << " sent " << t->actual_length << endl;
	}else{
		cerr << "OTransfer error "<< t->status << " " << t << endl;
		destroy_transfer(dev, dev->out_transfers, t);
	}
}
