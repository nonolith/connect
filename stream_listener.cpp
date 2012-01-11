#include "stream_listener.hpp"
#include <boost/foreach.hpp>
#include <iostream>
#include <memory>

StreamListener *makeStreamListener(StreamingDevice* dev, ClientConn* client, JSONNode &n){
	std::auto_ptr<StreamListener> listener(new StreamListener());

	listener->id = ListenerId(client, jsonIntProp(n, "id"));
	listener->device = dev;
	listener->client = client;
	
	listener->decimateFactor = jsonIntProp(n, "decimateFactor", 1);
	
	// Prevent divide by 0
	if (listener->decimateFactor == 0) listener->decimateFactor = 1; 

	int start = jsonIntProp(n, "start", -1);
	if (start < 0){ // Negative indexes are relative to latest sample
		start = (dev->buffer_max()) + start + 1;
	}
	
	if (start < 0) listener->index = 0;
	else listener->index = start;
	
	listener->outIndex = 0;
	
	listener->count = jsonIntProp(n, "count");
	
	JSONNode j_streams = n.at("streams");
	for(JSONNode::iterator i=j_streams.begin(); i!=j_streams.end(); i++){
		listener->streams.push_back(
			dev->findStream(
				jsonStringProp(*i, "channel"),
				jsonStringProp(*i, "stream")));
	}
	
	JSONNode::iterator t = n.find("trigger");
	if (t != n.end() && (t->type()) == JSON_NODE){
		JSONNode &trigger = *t;	 
		listener->triggerMode = 1;
		listener->triggered = false;
		listener->triggerLevel = jsonFloatProp(trigger, "level");
		listener->triggerStream = dev->findStream(
			jsonStringProp(trigger, "channel"),
			jsonStringProp(trigger, "stream"));
		listener->triggerHoldoff = jsonIntProp(trigger, "holdoff", 0);
		listener->triggerOffset = jsonIntProp(trigger, "offset", 0);
		
		if (listener->triggerOffset<0 && -listener->triggerOffset >= listener->triggerHoldoff){
			// Prevent big negative offsets that could cause infinite loops
			listener->triggerHoldoff = -listener->triggerOffset;
		}
		
	}else{
		listener->triggerMode = 0;
		listener->triggered = false;
		listener->triggerLevel = 0;
		listener->triggerStream = 0;
		listener->triggerHoldoff = 0;
		listener->triggerOffset = 0;
	}
		
	return listener.release();
}


bool StreamListener::handleNewData(){
	if (triggerMode && !triggered && !findTrigger())
		// Waiting for a trigger and haven't found it yet
		return true;
				
	if (index + decimateFactor >= device->capture_i)
		// The data for our next output sample hasn't been collected yet
		return true;
	
	JSONNode n(JSON_NODE);

	n.push_back(JSONNode("id", id.second));
	n.push_back(JSONNode("idx", outIndex));
	
	if (outIndex == 0){
		n.push_back(JSONNode("sampleIndex", index));
	}
	
	JSONNode streams_data(JSON_ARRAY);
	streams_data.set_name("data");
	
	// Calulate number of decimateFactor-sized chunks are available
	unsigned nchunks = (device->capture_i - index)/decimateFactor;
	
	// But if it's more than the remaining number of output samples, clamp it
	if (count > 0 && (count - outIndex) < nchunks)
		nchunks = count - outIndex;	
	
	BOOST_FOREACH(Stream* stream, streams){
		JSONNode a(JSON_ARRAY);
		
		unsigned i = index;
		
		for (unsigned chunk = 0; chunk < nchunks; chunk++){
			float total = 0;
			for (unsigned chunk_i=0; chunk_i < decimateFactor; chunk_i++){
				total += device->get(*stream, i++);
			}
			a.push_back(JSONNode("", total/decimateFactor));
		}
		
		streams_data.push_back(a);
	}
	
	n.push_back(streams_data);
	
	index += nchunks * decimateFactor;
	outIndex += nchunks;

	bool done = (count>0 && (int) outIndex >= count);
	
	if (done && !triggerMode){
		n.push_back(JSONNode("done", true));
	}

	n.push_back(JSONNode("_action", "update"));
	client->sendJSON(n);
	
	if (done && triggerMode){
		//std::cout << "Trigger sweep end "<<index<<" "<<outIndex<<std::endl;
		outIndex = 0;
		triggered = false;
		index += triggerHoldoff;
		return handleNewData(); // In case there's another packet in waiting
	}
	
	return !done;
}

bool StreamListener::findTrigger(){
	bool state = device->get(*triggerStream, index) > triggerLevel;
	while (++index < device->capture_i){
		bool newState = device->get(*triggerStream, index) > triggerLevel;
		
		if (newState == true && state == false){
			std::cout << "Trigger at " << index << " " <<device->get(*triggerStream, index-1) << " " << device->get(*triggerStream, index) << std::endl;
			index += triggerOffset;
			triggered = true;
			return true;
		}
		state = newState;
	}
	return false;
}
