#pragma once

#include "../dataserver.hpp"

class DemoDevice: public Device{
	public: 
	DemoDevice();
	virtual ~DemoDevice();
	
	/// Start streaming for the specified number of samples
	virtual void start_streaming(unsigned samples);
	
	/// Cancel streaming
	virtual void stop_streaming();

	virtual const string model(){return "com.nonolithlabs.demo";}
	virtual const string hwversion(){return "0";}
	virtual const string fwversion(){return "0";}
	virtual const string serialno(){return "00000000";}

	InputChannel channel_v;
	InputChannel channel_i;

	OutputChannel channel_out;
};