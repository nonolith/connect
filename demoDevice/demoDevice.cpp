#include "demoDevice.hpp"
#include <boost/bind.hpp>
#include <iostream>
#include <math.h>

const float demoSampleTime = 0.010;

DemoDevice::DemoDevice():
	channel("channel", "Channel A"),
	channel_v("v", "Voltage", "V", -2, 2, "measure", demoSampleTime/10, 0),
	channel_i("i", "Current", "mA", -2, 2, "source", demoSampleTime/10, 1),
	count(0),
	sample_timer(io){

	channels.push_back(&channel);
	channel.streams.push_back(&channel_v);
	channel.streams.push_back(&channel_i);
}

DemoDevice::~DemoDevice(){
	pause_capture();
}

void DemoDevice::on_prepare_capture(){
	samples = ceil(captureLength/channel_v.sampleTime);
	std::cout << "Starting for " << samples << " samples" <<std::endl;
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
	sample_timer.expires_from_now(boost::posix_time::milliseconds(demoSampleTime*1000));
	sample_timer.async_wait(boost::bind(
		&DemoDevice::sample,this,
		boost::asio::placeholders::error
	));
}

void DemoDevice::sample(const boost::system::error_code& e){
	if (e) return;
	setTimer();

	for (int i=0; i<10; i++){
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
	
		channel_v.put(a);	
		channel_i.put(b);
	}

	dataReceived.notify();
	if (count >= samples) done_capture();
}
