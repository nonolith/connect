#include "demoDevice.hpp"

DemoDevice::DemoDevice():
	channel_v("av", "Voltage A", "V", "measure"),
	channel_i("ai", "Current A", "I", "source"),
	channel_out("o"){

	output_channels.push_back(&channel_out);
	input_channels.push_back(&channel_v);
	input_channels.push_back(&channel_i);
}

DemoDevice::~DemoDevice(){
	
}

void DemoDevice::start_streaming(unsigned samples){
	
}

void DemoDevice::stop_streaming(){}