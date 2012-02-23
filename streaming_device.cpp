// Nonolith Connect
// https://github.com/nonolith/connect
// Streaming device base class
// Released under the terms of the GNU GPLv3+
// (C) 2012 Nonolith Labs, LLC
// Authors:
//   Kevin Mehall <km@kevinmehall.net>

#include <iostream>
#include <boost/foreach.hpp>

#include "dataserver.hpp"
#include "streaming_device.hpp"
#include "stream_listener.hpp"

OutputSource *makeSource(JSONNode& description);

bool StreamingDevice::processMessage(ClientConn& client, string& cmd, JSONNode& n){
	if (cmd == "listen"){
		addListener(makeStreamListener(this, &client, n));
	
	}else if (cmd == "cancelListen"){
		ListenerId id(&client, jsonIntProp(n, "id"));
		cancelListen(id);
	
	}else if (cmd == "configure"){
		int      mode =       jsonIntProp(n,   "mode");
		unsigned samples =    jsonIntProp(n,   "samples");
		float    sampleTime = jsonFloatProp(n, "sampleTime");
		bool     continuous = jsonBoolProp(n,  "continuous", false);
		bool     raw =        jsonBoolProp(n,  "raw", false);
		configure(mode, sampleTime, samples, continuous, raw);
	
	}else if (cmd == "startCapture"){
		start_capture();
	
	}else if (cmd == "pauseCapture"){
		pause_capture();
	
	}else if (cmd == "set"){
		Channel *channel = channelById(jsonStringProp(n, "channel"));
		if (!channel) throw ErrorStringException("Channel not found");
		setOutput(channel, makeSource(n));
		
	}else if (cmd == "setGain"){
		Channel *channel = channelById(jsonStringProp(n, "channel"));
		if (!channel) throw ErrorStringException("Channel not found");
		Stream *stream = findStream(
				jsonStringProp(n, "channel"),
				jsonStringProp(n, "stream"));

		unsigned gain = jsonIntProp(n, "gain", 1);
		
		setGain(channel, stream, gain);
	}else{
		return false;
	}
	return true;
}

void RESTOutputRespond(websocketpp::session_ptr client, Channel *channel){
	JSONNode n;
	channel->source->describeJSON(n);
	respondJSON(client, n);
}

void handleRESTOutputCallback(websocketpp::session_ptr client, StreamingDevice* device, device_ptr ptr, Channel* channel, string postdata){
	std::cerr<< "Got POST data:" << postdata << std::endl;
	JSONNode n = libjson::parse(postdata);
	device->setOutput(channel, makeSource(n));
	RESTOutputRespond(client, channel);
}

bool handleRESTOutput(UrlPath path, websocketpp::session_ptr client, StreamingDevice* device, device_ptr ptr, Channel* channel){
	if (!channel->source) return false;
	
	if (client->get_method() == "POST"){
		client->read_http_post_body(
			boost::bind(&handleRESTOutputCallback, client, device, ptr, channel,_1));
	}else{
		RESTOutputRespond(client, channel);
	}

	return true;
}

bool handleRESTInput(UrlPath path, websocketpp::session_ptr client, StreamingDevice* device, device_ptr ptr, Channel* channel){
	JSONNode j;
	respondJSON(client, j);
	return true;
}

bool StreamingDevice::handleREST(UrlPath path, websocketpp::session_ptr client){
	if (path.leaf()){
		JSONNode jstate = stateToJSON();
		respondJSON(client, jstate);
		return true;
	}else{
		Channel *channel = channelById(path.get());
		if (!channel) return false;
		
		UrlPath spath = path.sub();
		if (spath.leaf()) return false;
		
		if (spath.matches("output")){
			return handleRESTOutput(spath.sub(), client, this, shared_from_this(), channel);
		}else if (spath.matches("input")){
			return handleRESTInput(spath.sub(), client, this, shared_from_this(), channel);
		}	
		
		return false;
	}
}

void StreamingDevice::onClientAttach(ClientConn* client){
	Device::onClientAttach(client);
	
	JSONNode n(JSON_NODE);
	n.push_back(JSONNode("_action", "deviceConfig"));

	JSONNode jstate = stateToJSON();
	jstate.set_name("device");
	n.push_back(jstate);
	
	client->sendJSON(n);
}

void StreamingDevice::onClientDetach(ClientConn* client){
	Device::onClientDetach(client);
	
	std::cout << "Client disconnected. " << listeners.size() << std::endl;
	
	listener_map_t::iterator it;
	for (it=listeners.begin(); it!=listeners.end();){
		// Increment before deleting as that invalidates the iterator
		listener_map_t::iterator currentIt = it++;
		StreamListener* w = currentIt->second;
		
		if (w->client == client){
			delete w;
			listeners.erase(currentIt);
		}
	}
	
	std::cout << "L " << listeners.size() << std::endl;
}

void StreamingDevice::addListener(StreamListener *l){
	cancelListen(l->id);
	
	if (l->handleNewData()){
		listeners.insert(listener_map_t::value_type(l->id, l));
	}else{
		delete l;
	}
}

void StreamingDevice::cancelListen(ListenerId id){
	listener_map_t::iterator it = listeners.find(id);
	if (it != listeners.end()){
		delete it->second;
		listeners.erase(it);
	}
}

void StreamingDevice::clearAllListeners(){
	BOOST_FOREACH(listener_map_t::value_type &p, listeners){
		delete p.second;
	}
	listeners.clear();
}

void StreamingDevice::resetAllListeners(){
	BOOST_FOREACH(listener_map_t::value_type &p, listeners){
		p.second->reset();
	}
}

void StreamingDevice::handleNewData(){
	listener_map_t::iterator it;
	for (it=listeners.begin(); it!=listeners.end();){
		// Increment before (potentially) deleting the watch, as that invalidates the iterator
		listener_map_t::iterator currentIt = it++;
		StreamListener* w = currentIt->second;
		
		if (!w->handleNewData()){
			delete w;
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
		std::cerr << "start capture" <<std::endl;
		on_start_capture();
		notifyCaptureState();
	}
}

void StreamingDevice::pause_capture(){
	if (captureState){
		captureState = false;
		std::cerr << "pause capture" <<std::endl;
		on_pause_capture();
		notifyCaptureState();
	}
}

void StreamingDevice::done_capture(){
	captureDone = true;
	std::cerr << "done capture" <<std::endl;
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
	n.push_back(JSONNode("gain", gain));
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

bool Stream::allocate(unsigned size){
	if (data){
		free(data);
	}
	data = (float *) malloc(size*sizeof(float));
	return !!data;
}
