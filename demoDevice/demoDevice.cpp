#include "demoDevice.hpp"
#include <boost/bind.hpp>
#include <iostream>
#include <math.h>

const float demoSampleTime = 0.001;

DemoDevice::DemoDevice():
	Device(demoSampleTime),
	channel("channel", "Channel A"),
	channel_v("v", "Voltage", "V",     0,   5, 1),
	channel_i("i", "Current", "mA", -200, 200, 2),
	sample_timer(io){

	channels.push_back(&channel);
	channel.streams.push_back(&channel_v);
	channel.streams.push_back(&channel_i);
}

DemoDevice::~DemoDevice(){
	pause_capture();
}

void DemoDevice::on_prepare_capture(){
	captureSamples = ceil(captureLength/sampleTime);
	std::cout << "Starting for " << captureSamples << " samples "<< captureContinuous <<std::endl;
	channel_v.allocate(captureSamples);
	channel_i.allocate(captureSamples);
}

void DemoDevice::on_start_capture(){
	setTimer();
}

void DemoDevice::on_pause_capture(){
	sample_timer.cancel();
}

void DemoDevice::setTimer(){
	sample_timer.expires_from_now(boost::posix_time::milliseconds(demoSampleTime*10000));
	sample_timer.async_wait(boost::bind(
		&DemoDevice::sample,this,
		boost::asio::placeholders::error
	));
}

void DemoDevice::sample(const boost::system::error_code& e){
	if (e) return;
	setTimer();

	for (int i=0; i<10; i++){	
		unsigned mode = 0;
		float val = 0;
		if (channel.source){
			mode = channel.source->mode;
			val = channel.source->nextValue(capture_i/sampleTime);
		}
	
		float a=0, b=0;
		
		if (mode == 1){
			a = val;
			b = 100*val;
		}else if (mode == 2){
			a = 100.0/val;
			b = val;
		}
	
		put(channel_v, a+sin(capture_i*0.01)*0.1);
		put(channel_i, b+sin(capture_i*0.01)*0.1);
		sampleDone();
	}
	packetDone();
}
