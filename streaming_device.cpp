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
		ListenerId id(&client, n.at("id").as_int());
		cancelListen(id);
	
	}else if (cmd == "configure"){
		int mode = n.at("mode").as_int();
		unsigned samples = n.at("samples").as_int();
		float sampleTime = n.at("sampleTime").as_float();
	
		bool continuous = false;
		if (n.find("continuous") != n.end()){
			continuous = n.at("continuous").as_bool();
		}
	
		bool raw = false;
		if (n.find("raw") != n.end()){
			raw = n.at("raw").as_bool();
		}
		configure(mode, sampleTime, samples, continuous, raw);
	
	}else if (cmd == "startCapture"){
		start_capture();
	
	}else if (cmd == "pauseCapture"){
		pause_capture();
	
	}else if (cmd == "set"){
		string cId = n.at("channel").as_string();
		Channel *channel = channelById(cId);
		if (!channel) throw ErrorStringException("Stream not found");
		setOutput(channel, makeSource(n));
	
	}else if (cmd == "controlTransfer"){
		//TODO: this is device-specific. Should it go elsewhere?
		string id = n.at("id").as_string();
		uint8_t bmRequestType = n.at("bmRequestType").as_int();
		uint8_t bRequest = n.at("bRequest").as_int();
		uint16_t wValue = n.at("wValue").as_int();
		uint16_t wIndex = n.at("wIndex").as_int();
		uint16_t wLength = n.at("wLength").as_int();
	
		uint8_t data[wLength];
	
		bool isIn = bmRequestType & 0x80;
	
		if (!isIn){
			//TODO: also handle array input
			string datastr = n.at("data").as_string();
			datastr.copy((char*)data, wLength);
		}
	
		int ret = controlTransfer(bmRequestType, bRequest, wValue, wIndex, data, wLength);
	
		JSONNode reply(JSON_NODE);
		reply.push_back(JSONNode("_action", "controlTransferReturn"));
		reply.push_back(JSONNode("status", ret));
		reply.push_back(JSONNode("id", id));
	
		if (isIn && ret>=0){
			JSONNode data_arr(JSON_ARRAY);
			for (int i=0; i<ret && i<wLength; i++){
				data_arr.push_back(JSONNode("", data[i]));
			}
			data_arr.set_name("data");
			reply.push_back(data_arr);
		}
	
		client.sendJSON(reply);
	}else{
		return false;
	}
	return true;
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
	JSONNode n(JSON_NODE);
	n.push_back(JSONNode("_action", "deviceConfig"));

	JSONNode d = toJSON();
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
