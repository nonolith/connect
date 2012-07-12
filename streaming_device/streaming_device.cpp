// Nonolith Connect
// https://github.com/nonolith/connect
// Streaming device base class
// Released under the terms of the GNU GPLv3+
// (C) 2012 Nonolith Labs, LLC
// Authors:
//   Kevin Mehall <km@kevinmehall.net>

#include <iostream>
#include <boost/foreach.hpp>

#include "streaming_device.hpp"
#include "stream_listener.hpp"

//// Serialization functions

JSONNode Stream::toJSON(){
	Stream *s = this;
	JSONNode n(JSON_NODE);
	n.set_name(s->id);
	n.push_back(JSONNode("id", s->id));
	n.push_back(JSONNode("displayName", s->displayName));
	n.push_back(JSONNode("units", s->units));
	n.push_back(JSONNode("min", s->min));
	n.push_back(JSONNode("max", s->max));
	n.push_back(JSONNode("outputMode", s->outputMode));
	n.push_back(JSONNode("gain", s->getGain()));
	n.push_back(JSONNode("uncertainty", s->uncertainty));
	return n;
}

JSONNode Channel::toJSON(){
	Channel *channel = this;
	JSONNode n(JSON_NODE);
	n.set_name(channel->id);
	n.push_back(JSONNode("id", channel->id));
	n.push_back(JSONNode("displayName", channel->displayName));
	
	if (source){
		JSONNode output(JSON_NODE);
		output.set_name("output");
		source->describeJSON(output);
		n.push_back(output);
	}

	JSONNode streams(JSON_NODE);
	streams.set_name("streams");
	BOOST_FOREACH (Stream* i, channel->streams){
		streams.push_back(i->toJSON());
	}
	n.push_back(streams);

	return n;
}

JSONNode StreamingDevice::stateToJSON(bool configOnly){
	JSONNode n;
	if (!configOnly){
		n = toJSON();
	}
	
	std::cout << "sampleTime " << sampleTime << std::endl;
	
	n.push_back(JSONNode("sampleTime", sampleTime));
	n.push_back(JSONNode("minSampleTime", minSampleTime));
	n.push_back(JSONNode("mode", devMode));
	n.push_back(JSONNode("samples", captureSamples));
	n.push_back(JSONNode("length", captureLength));
	n.push_back(JSONNode("continuous", captureContinuous));
	n.push_back(JSONNode("raw", rawMode));
	n.push_back(JSONNode("currentLimit", currentLimit));
	
	if  (configOnly) return n;
	
	n.push_back(JSONNode("captureState", captureState));
	n.push_back(JSONNode("captureDone", captureDone));
	
	
	JSONNode channels(JSON_NODE);
	channels.set_name("channels");
	BOOST_FOREACH (Channel* c, this->channels){
		channels.push_back(c->toJSON());
	}
	n.push_back(channels);

	return n;
}

void StreamingDevice::addListener(listener_ptr l){
	if (l->handleNewData()){
		listeners.insert(l);
	}
}

listener_ptr StreamingDevice::findListener(ClientConn* c, unsigned id){
	BOOST_FOREACH(listener_ptr w, listeners){
		if (w->isFromClient(c) && w->id == id) return w;
	}
	return listener_ptr();
}

void StreamingDevice::cancelListen(listener_ptr c){
	listener_set_t::iterator it = listeners.find(c);
	if (it != listeners.end()){
		listeners.erase(it);
	}
}

void StreamingDevice::clearAllListeners(){
	listeners.clear();
}

void StreamingDevice::resetAllListeners(){
	BOOST_FOREACH(listener_ptr w, listeners){
		w->reset();
	}
}

void StreamingDevice::handleNewData(){
	listener_set_t::iterator it;
	for (it=listeners.begin(); it!=listeners.end();){
		// Increment before (potentially) deleting the watch, as that invalidates the iterator
		listener_set_t::iterator currentIt = it++;
		listener_ptr w = *currentIt;
		
		if (!w->handleNewData()){
			listeners.erase(currentIt);
		}
	}
}
	
	
void StreamingDevice::reset_capture(){
	captureDone = false;
	capture_i = 0;
	capture_o = 0;
	on_reset_capture();
	notifyCaptureReset();
}

void StreamingDevice::start_capture(){
	if (!captureState){
		if (captureDone) reset_capture();
		captureState = true;
		std::cerr << "Start capture" <<std::endl;
		on_start_capture();
		notifyCaptureState();
	}
}

void StreamingDevice::pause_capture(){
	if (captureState){
		captureState = false;
		std::cerr << "Pause capture" <<std::endl;
		on_pause_capture();
		notifyCaptureState();
	}
}

void StreamingDevice::done_capture(){
	captureDone = true;
	std::cerr << "Done capture" <<std::endl;
	if (captureState){
		captureState = false;
		on_pause_capture();
	}
	notifyCaptureState();
}

void StreamingDevice::notifyCaptureState(){
	JSONNode n(JSON_NODE);
	n.push_back(JSONNode("_action", "captureState"));
	n.push_back(JSONNode("state", captureState));
	n.push_back(JSONNode("done", captureDone));
	broadcastJSON(n);
}

void StreamingDevice::notifyCaptureReset(){
	resetAllListeners();
	JSONNode n(JSON_NODE);
	n.push_back(JSONNode("_action", "captureReset"));
	broadcastJSON(n);
}

void StreamingDevice::notifyConfig(){
	clearAllListeners();
	
	JSONNode n(JSON_NODE);
	n.push_back(JSONNode("_action", "deviceConfig"));

	JSONNode d = stateToJSON();
	d.set_name("device");
	n.push_back(d);

	broadcastJSON(n);
}

void StreamingDevice::packetDone(){
	handleNewData();
	
	if (!captureContinuous && capture_i >= captureSamples){
		done_capture();
	}
}

void StreamingDevice::setOutput(Channel* channel, OutputSource* source){
	source->initialize(capture_o, channel->source);
	
	if (channel->source){
		delete channel->source;
	}
	channel->source=source;
	channel->source->startSample = capture_o;
	
	notifyOutputChanged(channel, source);
}

void StreamingDevice::notifyOutputChanged(Channel *channel, OutputSource *source){
	JSONNode n(JSON_NODE);
	n.push_back(JSONNode("_action", "outputChanged"));
	n.push_back(JSONNode("channel", channel->id));
	source->describeJSON(n);
	broadcastJSON(n);
}

void StreamingDevice::notifyGainChanged(Channel* channel, Stream* stream, int gain){
	JSONNode n(JSON_NODE);
	n.push_back(JSONNode("_action", "gainChanged"));
	n.push_back(JSONNode("channel", channel->id));
	n.push_back(JSONNode("stream", stream->id));
	n.push_back(JSONNode("gain", stream->getGain()));
	broadcastJSON(n);
}

Channel* StreamingDevice::channelById(const string& id){
	BOOST_FOREACH(Channel* c, channels){
		if (c->id == id) return c;
	}
	return 0;
}

Stream* Channel::streamById(const string& id){
	BOOST_FOREACH(Stream *i, streams){
		if (i->id == id) return i;
	}
	return 0;
}

Stream* StreamingDevice::findStream(const string& channelId, const string& streamId){
	Channel* c = channelById(channelId);
	if (!c){
		throw ErrorStringException("Channel not found");
	}
	Stream *s = c->streamById(streamId);
	if (!s){
		throw ErrorStringException("Stream not found");
	}
	return s;
}

void StreamingDevice::onDisconnect(){
	Device::onDisconnect();
	clearAllListeners();
}

bool Stream::allocate(unsigned size){
	if (data){
		free(data);
	}
	data = (float *) malloc(size*sizeof(float));
	return !!data;
}
