#include "demoDevice.hpp"
#include <boost/bind.hpp>
#include <iostream>

DemoDevice::DemoDevice():
	channel("channel", "Channel A"),
	channel_v("v", "Voltage", "V", "measure", 1.0/1000, -1, 0.050),
	channel_i("i", "Current", "I", "source", 1.0/1000, -1, 0.050),
	channel_out("o"),
	count(0),
	sample_timer(io){

	channels.push_back(&channel);
	channel.outputs.push_back(&channel_out);
	channel.inputs.push_back(&channel_v);
	channel.inputs.push_back(&channel_i);
}

DemoDevice::~DemoDevice(){
	pause_capture();
}

void DemoDevice::on_prepare_capture(){
	samples = captureLength/channel_v.sampleTime;
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

	channel_v.put(sin(count/10.0)*1000.0+1000.0);
	channel_v.data_received.notify();
	channel_i.put(sin(count/8.0)*1000.0+1000.0);
	channel_i.data_received.notify();

	if (count >= samples) done_capture();
}