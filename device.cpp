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
