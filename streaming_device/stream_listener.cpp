// Nonolith Connect
// https://github.com/nonolith/connect
// Listeners - streaming data requests
// Released under the terms of the GNU GPLv3+
// (C) 2012 Nonolith Labs, LLC
// Authors:
//   Kevin Mehall <km@kevinmehall.net>

#include "stream_listener.hpp"
#include <boost/foreach.hpp>
#include <iostream>
#include <memory>

StreamListener::StreamListener():
	id(0),
	index(0),
	outIndex(0),
	triggerType(NONE),
	triggered(false),
	triggerRepeat(false),
	triggerLevel(0),
	triggerStream(0),
	triggerHoldoff(0),
	triggerOffset(0),
	triggerForce(0),
	triggerForceIndex(0),
	triggerSubsampleError(0){}

listener_ptr makeStreamListener(StreamingDevice* dev, ClientConn* client, JSONNode &n){
	std::auto_ptr<WSStreamListener> listener(new WSStreamListener());

	listener->id = jsonIntProp(n, "id");
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
		
		string type = jsonStringProp(trigger, "type", "in");
		if (type == "in"){
			listener->triggerType = INSTREAM;
			listener->triggerLevel = jsonFloatProp(trigger, "level");
			listener->triggerStream = dev->findStream(
				jsonStringProp(trigger, "channel"),
				jsonStringProp(trigger, "stream"));
		}else if (type == "out"){
			listener->triggerType = OUTSOURCE;
			listener->triggerChannel = dev->channelById(jsonStringProp(trigger, "channel"));
			if (!listener->triggerChannel) throw ErrorStringException("Trigger channel not found");
		}else{
			throw ErrorStringException("Invalid trigger type");
		}
			
		listener->triggerRepeat = jsonBoolProp(trigger, "repeat", true);
		listener->triggerHoldoff = jsonIntProp(trigger, "holdoff", 0);
		listener->triggerOffset = jsonIntProp(trigger, "offset", 0);
		
		if (listener->triggerOffset<0 && -listener->triggerOffset >= listener->triggerHoldoff){
			// Prevent big negative offsets that could cause infinite loops
			listener->triggerHoldoff = -listener->triggerOffset;
		}
		listener->triggerForce = jsonIntProp(trigger, "force", 0);
		listener->triggerForceIndex = listener->index + listener->triggerForce;
	}
		
		
	return listener_ptr(listener.release());
}

unsigned StreamListener::howManySamples(){
	if (triggerType != NONE && !triggered && !findTrigger())
		// Waiting for a trigger and haven't found it yet
		return 0;
				
	if (index + decimateFactor >= device->capture_i)
		// The data for our next output sample hasn't been collected yet
		return 0;

	// Calulate number of decimateFactor-sized chunks are available
	unsigned nchunks = (device->capture_i - index)/decimateFactor;
	
	// But if it's more than the remaining number of output samples, clamp it
	if (count > 0 && (count - outIndex) < nchunks)
		nchunks = count - outIndex;	

	return nchunks;
}

bool WSStreamListener::handleNewData(){
	unsigned nchunks = howManySamples();
	if (!nchunks) return true;
	
	JSONNode n(JSON_NODE);

	n.push_back(JSONNode("id", id));
	n.push_back(JSONNode("idx", outIndex));
	
	if (outIndex == 0){
		if (triggerForce && index > triggerForceIndex){
			n.push_back(JSONNode("triggerForced", true));
		}
		if (triggered){
			n.push_back(JSONNode("subsample", triggerSubsampleError));
		}
		n.push_back(JSONNode("sampleIndex", index));
	}
	
	JSONNode streams_data(JSON_ARRAY);
	streams_data.set_name("data");
	
	BOOST_FOREACH(Stream* stream, streams){
		JSONNode a(JSON_ARRAY);
		
		for (unsigned chunk = 0; chunk < nchunks; chunk++){
			a.push_back(JSONNode("", 
				device->resample(*stream, index+chunk*decimateFactor, decimateFactor)
			));
		}
		
		streams_data.push_back(a);
	}
	
	n.push_back(streams_data);
	
	index += nchunks * decimateFactor;
	outIndex += nchunks;

	bool done = (count>0 && (int) outIndex >= count);
	
	if (done && !triggerRepeat){
		n.push_back(JSONNode("done", true));
	}

	n.push_back(JSONNode("_action", "update"));
	client->sendJSON(n);
	
	if (done && triggerRepeat){
		//std::cout << "Trigger sweep end "<<index<<" "<<outIndex<<std::endl;
		outIndex = 0;
		triggered = false;
		index += triggerHoldoff;
		triggerForceIndex = index + triggerForce;
		return handleNewData(); // In case there's another packet in waiting
	}
	
	return !done;
}

bool StreamListener::findTrigger(){
	triggerSubsampleError = 0;
	if (triggerType == INSTREAM){
		bool state = device->get(*triggerStream, index) > triggerLevel;
		while (++index < device->capture_i){
			bool newState = device->get(*triggerStream, index) > triggerLevel;
		
			if (newState == true && state == false){
				//std::cout << "Trigger at " << index << " " <<device->get(*triggerStream, index-1) << " " << device->get(*triggerStream, index) << std::endl;
				index += triggerOffset;
				triggered = true;
				return true;
			}
			state = newState;
		}
	}else if (triggerType == OUTSOURCE){
		double zero = triggerChannel->source->getPhaseZeroAfterSample(index);
		unsigned tIndex = triggerForceIndex;
		if (!triggerForce || zero <= triggerForceIndex){
			tIndex = round(zero) + triggerOffset;
			triggerSubsampleError = zero - round(zero);
		}

		if (device->capture_i >= tIndex){
			index = tIndex;
			triggered = true;
			return true;
		}
	}
	
	if (triggerForce && index > triggerForceIndex){
		//std::cout << "Forced trigger at " << index << std::endl;
		triggered = true;
		return true;
	}
	
	return false;
}
