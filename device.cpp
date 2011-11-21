#include <boost/foreach.hpp>
#include "dataserver.hpp"


device_ptr getDeviceBySerial(string id){
	BOOST_FOREACH(device_ptr d, devices){
		if (d->serialno() == id) return d;
	}
	return device_ptr();
}

Channel* Device::channelById(const string& id){
	BOOST_FOREACH(Channel* c, channels){
		if (c->id == id) return c;
	}
	return 0;
}

InputStream* Channel::inputById(const string& id){
	BOOST_FOREACH(InputStream *i, inputs){
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

InputStream* findStream(const string& deviceId, const string& channelId, const string& streamId){
	device_ptr d = getDeviceBySerial(deviceId);
	if (!d){
		throw ErrorStringException("Device not found");
	}
	Channel* c = d->channelById(channelId);
	if (!c){
		throw ErrorStringException("Channel not found");
	}
	InputStream *s = c->inputById(streamId);
	if (!s){
		throw ErrorStringException("Stream not found");
	}
	return s;
}

OutputStream* Channel::outputById(const string& id){
	BOOST_FOREACH(OutputStream *i, outputs){
		if (i->id == id) return i;
	}
	return 0;
}


void InputStream::allocate(unsigned size){
	buffer_size = size;
	buffer_fill_point = 0;
	if (data){
		free(data);
	}
	data = (uint16_t*) malloc(buffer_size*sizeof(uint16_t));
}

void InputStream::put(uint16_t p){
	if (buffer_fill_point < buffer_size){
		data[buffer_fill_point++]=p;
	}
}

void startStreaming(){
	BOOST_FOREACH (device_ptr d, devices){
		d->start_streaming(1000);
	}
	streaming_state_changed.notify();
}

void stopStreaming(){
	BOOST_FOREACH (device_ptr d, devices){
		d->stop_streaming();
	}
	streaming_state_changed.notify();
}
