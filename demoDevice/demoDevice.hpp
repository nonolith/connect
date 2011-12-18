#pragma once

#include "../dataserver.hpp"

class DemoDevice: public Device{
	public: 
	DemoDevice();
	virtual ~DemoDevice();

	virtual const string model(){return "com.nonolithlabs.demo";}
	virtual const string hwVersion(){return "0";}
	virtual const string fwVersion(){return "0";}
	virtual const string serialno(){return "00000000";}

	Channel channel;
	Stream channel_v;
	Stream channel_i;

private:
	virtual void on_prepare_capture();
	virtual void on_reset_capture();
	virtual void on_start_capture();
	virtual void on_pause_capture();

	boost::asio::deadline_timer sample_timer;
	void sample(const boost::system::error_code&);
	void setTimer();
};
