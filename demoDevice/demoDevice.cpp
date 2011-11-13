#include "demoDevice.hpp"
#include <boost/bind.hpp>
#include <iostream>

DemoDevice::DemoDevice():
	channel("channel", "Channel"),
	channel_v("av", "Voltage A", "V", "measure", 0),
	channel_i("ai", "Current A", "I", "source", 1),
	channel_out("o"),
	sample_timer(io){

	channels.push_back(&channel);
	channel.outputs.push_back(&channel_out);
	channel.inputs.push_back(&channel_v);
	channel.inputs.push_back(&channel_i);
}

DemoDevice::~DemoDevice(){
	stop_streaming();
}

void DemoDevice::start_streaming(unsigned samples){
	channel_v.allocate(samples);
	channel_i.allocate(samples);

	setTimer();
}

void DemoDevice::stop_streaming(){
	sample_timer.cancel();
}

void DemoDevice::setTimer(){
	sample_timer.expires_from_now(boost::posix_time::milliseconds(100));
	sample_timer.async_wait(boost::bind(
		&DemoDevice::sample,this,
		boost::asio::placeholders::error
	));
}

void DemoDevice::sample(const boost::system::error_code& e){
	if (e) return;
	setTimer();
	std::cerr << "Demo sampling" << std::endl;

	channel_v.put(1234);
	channel_v.data_received.notify();
	channel_i.put(4567);
	channel_i.data_received.notify();
}