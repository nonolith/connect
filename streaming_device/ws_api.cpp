#include "streaming_device.hpp"
#include "stream_listener.hpp"

bool StreamingDevice::processMessage(ClientConn& client, string& cmd, JSONNode& n){
	if (cmd == "listen"){
		cancelListen(findListener(&client, jsonIntProp(n, "id")));
		addListener(makeStreamListener(this, &client, n));
	
	}else if (cmd == "cancelListen"){
		cancelListen(findListener(&client, jsonIntProp(n, "id")));
	
	}else if (cmd == "configure"){
		int      mode =       jsonIntProp(n,   "mode");
		unsigned samples =    jsonIntProp(n,   "samples");
		double    sampleTime = jsonFloatProp(n, "sampleTime");
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

		double gain = jsonFloatProp(n, "gain", 1);
		
		setGain(channel, stream, gain);
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
	
	std::cout << "Client disconnected. " << listeners.size() << std::endl;
	
	listener_set_t::iterator it;
	for (it=listeners.begin(); it!=listeners.end();){
		// Increment before deleting as that invalidates the iterator
		listener_set_t::iterator currentIt = it++;
		listener_ptr w = *currentIt;
		
		if (w->isFromClient(client)){
			listeners.erase(currentIt);
		}
	}
}



