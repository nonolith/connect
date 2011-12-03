#include "demoDevice.hpp"
#include <boost/bind.hpp>
#include <iostream>
#include <math.h>

DemoDevice::DemoDevice():
	channel("channel", "Channel A"),
	channel_v("v", "Voltage", "V", -2, 2, "measure", 0.050, 0),
	channel_i("i", "Current", "mA", -2, 2, "source", 0.050, 1),
	count(0),
	sample_timer(io){

	channels.push_back(&channel);
	channel.inputs.push_back(&channel_v);
	channel.inputs.push_back(&channel_i);
}

DemoDevice::~DemoDevice(){
	pause_capture();
}

void DemoDevice::on_prepare_capture(){
	samples = ceil(captureLength/channel_v.sampleTime);
	channel_v.allocate(samples);
	channel_i.allocate(samples);
	count = 0;
}

void DemoDevice::on_start_capture(){
	setTimer();
}

void DemoDevice::on_pause_capture(){
	sample_timer.cancel();
}

void DemoDevice::setTimer(){
	sample_timer.expires_from_now(boost::posix_time::milliseconds(50));
	sample_timer.async_wait(boost::bind(
		&DemoDevice::sample,this,
		boost::asio::placeholders::error
	));
}

void DemoDevice::sample(const boost::system::error_code& e){
	if (e) return;
	setTimer();

	count++;

	unsigned mode = 0;
	float val = 0;
	if (channel.source){
		mode = channel.source->mode;
		val = channel.source->nextValue(count/channel_v.sampleTime);
	}

	float a, b;

	if (mode == 0){
		a = val;
		b = 100*val;
	}else{
		a = 100.0/val;
		b = val;
	}

	//std::cout << "Sample: "<< a << " " << b << " " << mode << "  " << val << std::endl;

	channel_v.put(a);
	channel_v.data_received.notify();
	channel_i.put(b);
	channel_i.data_received.notify();

	if (count >= samples) done_capture();
}