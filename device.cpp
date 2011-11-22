#include <boost/foreach.hpp>
#include "dataserver.hpp"


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
	device_ptr d = getDeviceById(deviceId);
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

CaptureState captureState = CAPTURE_INACTIVE;
float captureLength = 0;

string captureStateToString(CaptureState s){
	switch (s){
		case CAPTURE_INACTIVE:
			return "inactive";
		case CAPTURE_READY:
			return "ready";
		case CAPTURE_ACTIVE:
			return "active";
		case CAPTURE_PAUSED:
			return "paused";
		case CAPTURE_DONE:
			return "done";
		default:
			return "";
	}
}

void prepareCapture(float seconds){
	if (captureState == CAPTURE_ACTIVE){
		pauseCapture();
	}

	captureLength = seconds;

	BOOST_FOREACH (device_ptr d, devices){
		d->prepare_capture(seconds);
	}
	captureState = CAPTURE_READY;
	capture_state_changed.notify();	
}

void startCapture(){
	if (captureState != CAPTURE_READY || captureState != CAPTURE_PAUSED) return;
	BOOST_FOREACH (device_ptr d, devices){
		d->start_capture();
	}
	captureState = CAPTURE_ACTIVE;
	capture_state_changed.notify();
}

void pauseCapture(){
	if (captureState != CAPTURE_ACTIVE) return;
	BOOST_FOREACH (device_ptr d, devices){
		d->pause_capture();
	}
	captureState = CAPTURE_PAUSED;
	capture_state_changed.notify();
}
