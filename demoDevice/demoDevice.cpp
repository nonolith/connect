#include "demoDevice.hpp"

DemoDevice::DemoDevice():
	channel("channel", "Channel"),
	channel_v("av", "Voltage A", "V", "measure", 0),
	channel_i("ai", "Current A", "I", "source", 1),
	channel_out("o"){

	channels.push_back(&channel);
	channel.outputs.push_back(&channel_out);
	channel.inputs.push_back(&channel_v);
	channel.inputs.push_back(&channel_i);
}

DemoDevice::~DemoDevice(){
	
}

void DemoDevice::start_streaming(unsigned samples){
	
}

void DemoDevice::stop_streaming(){}