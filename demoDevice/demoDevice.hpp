#pragma once

#include "../dataserver.hpp"

class DemoDevice: public Device{
	public: 
	DemoDevice();
	virtual ~DemoDevice();

	virtual const string model(){return "com.nonolithlabs.demo";}
	virtual const string hwversion(){return "0";}
	virtual const string fwversion(){return "0";}
	virtual const string serialno(){return "00000000";}

	Channel channel;
	InputStream channel_v;
	InputStream channel_i;

	OutputStream channel_out;

private:
	virtual void on_prepare_capture();
	virtual void on_start_capture();
	virtual void on_pause_capture();

	unsigned count;
	unsigned samples;
	boost::asio::deadline_timer sample_timer;
	void sample(const boost::system::error_code&);
	void setTimer();
};