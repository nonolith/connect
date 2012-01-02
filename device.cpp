#include <boost/foreach.hpp>
#include "dataserver.hpp"
#include <iostream>

void Device::reset_capture(){
	captureDone = false;
	capture_i = 0;
	capture_o = 0;
	on_reset_capture();
	notifyCaptureReset();
}

void Device::start_capture(){
	if (!captureState){
		if (captureDone) reset_capture();
		captureState = true;
		std::cerr << "start capture" <<std::endl;
		on_start_capture();
		notifyCaptureState();
	}
}

void Device::pause_capture(){
	if (captureState){
		captureState = false;
		std::cerr << "pause capture" <<std::endl;
		on_pause_capture();
		notifyCaptureState();
	}
}

void Device::done_capture(){
	captureDone = true;
	std::cerr << "done capture" <<std::endl;
	if (captureState){
		captureState = false;
		on_pause_capture();
	}
	notifyCaptureState();
}

void Device::notifyCaptureState(){
	BOOST_FOREACH(DeviceEventListener *l, listeners){
		l->on_capture_state_changed();
	}
}

void Device::notifyCaptureReset(){
	BOOST_FOREACH(DeviceEventListener *l, listeners){
		l->on_capture_reset();
	}
}

void Device::notifyConfig(){
	BOOST_FOREACH(DeviceEventListener *l, listeners){
		l->on_config();
	}
}

void Device::notifyDisconnect(){
	BOOST_FOREACH(DeviceEventListener *l, listeners){
		l->on_disconnect();
	}
}

void Device::packetDone(){
	BOOST_FOREACH(DeviceEventListener *l, listeners){
		l->on_data_received();
	}
	if (!captureContinuous && capture_i >= captureSamples){
		done_capture();
	}
}

void Device::setOutput(Channel* channel, OutputSource* source){
	if (channel->source){
		delete channel->source;
	}
	channel->source=source;
	channel->source->startSample = capture_o;
	
	notifyOutputChanged(channel, source);
}

void Device::notifyOutputChanged(Channel *channel, OutputSource *source){
	BOOST_FOREACH(DeviceEventListener *l, listeners){
		l->on_output_changed(channel, source);
	}
}

device_ptr getDeviceById(string id){
	BOOST_FOREACH(device_ptr d, devices){
		if (d->getId() == id) return d;
	}
	return device_ptr();
}

Channel* Device::channelById(const string& id){
	BOOST_FOREACH(Channel* c, channels){
		if (c->id == id) return c;
	}
	return 0;
}

Stream* Channel::streamById(const string& id){
	BOOST_FOREACH(Stream *i, streams){
		if (i->id == id) return i;
	}
	return 0;
}

struct ErrorStringException : public std::exception{
   string s;
   ErrorStringException (string ss) throw() : s(ss) {}
   virtual const char* what() const throw() { return s.c_str(); }
   virtual ~ErrorStringException() throw() {}
};

Stream* findStream(const string& deviceId, const string& channelId, const string& streamId){
	device_ptr d = getDeviceById(deviceId);
	if (!d){
		throw ErrorStringException("Device not found");
	}
	return d->findStream(channelId, streamId);
}

Stream* Device::findStream(const string& channelId, const string& streamId){
	Channel* c = channelById(channelId);
	if (!c){
		throw ErrorStringException("Channel not found");
	}
	Stream *s = c->streamById(streamId);
	if (!s){
		throw ErrorStringException("Stream not found");
	}
	return s;
}

bool Stream::allocate(unsigned size){
	if (data){
		free(data);
	}
	data = (float *) malloc(size*sizeof(float));
	return !!data;
}
