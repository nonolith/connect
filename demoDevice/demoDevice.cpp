#include "demoDevice.hpp"
#include <boost/bind.hpp>
#include <iostream>
#include <math.h>

const float demoSampleTime = 0.010;

DemoDevice::DemoDevice():
	channel("channel", "Channel A"),
	channel_v("v", "Voltage", "V",     0,   5, "measure", demoSampleTime/10, 1),
	channel_i("i", "Current", "mA", -200, 200, "source",  demoSampleTime/10, 2),
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
	std::cout << "Starting for " << samples << " samples "<< captureContinuous <<std::endl;
	channel_v.allocate(samples, captureContinuous);
	channel_i.allocate(samples, captureContinuous);
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
	
		float a=0, b=0;
		
		if (mode == 1){
			a = val;
			b = 100*val;
		}else if (mode == 2){
			a = 100.0/val;
			b = val;
		}
	
		channel_v.put(a+sin(count*0.01)*0.1);
		channel_i.put(b+sin(count*0.01)*0.1);
	}

	dataReceived.notify();
	if (!captureContinuous && count >= samples) done_capture();
}
