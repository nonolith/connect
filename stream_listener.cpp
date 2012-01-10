#include "stream_listener.hpp"
#include <boost/foreach.hpp>
#include <iostream>
#include <memory>

StreamListener *makeStreamListener(StreamingDevice* dev, ClientConn* client, JSONNode &n){
	std::auto_ptr<StreamListener> listener(new StreamListener());

	listener->id = ListenerId(client, n.at("id").as_int());
	listener->device = dev;
	listener->client = client;
	
	listener->decimateFactor = n.at("decimateFactor").as_int();

	int start = jsonIntProp(n, "start", -1);
	if (start < 0){ // Negative indexes are relative to latest sample
		start = (dev->buffer_max()) + start + 1;
	}
	
	if (start < 0) listener->index = 0;
	else listener->index = start;
	
	listener->count = jsonIntProp(n, "count");
	
	JSONNode j_streams = n.at("streams");
	for(JSONNode::iterator i=j_streams.begin(); i!=j_streams.end(); i++){
		string channel = i->at("channel").as_string();
		string streamName = i->at("stream").as_string();
		Stream* stream = dev->findStream(channel, streamName);
		listener->streams.push_back(stream);
	}
		
	return listener.release();
}


bool StreamListener::handleNewData(){
	bool keep = true;
	if (isDataAvailable()){
		JSONNode n(JSON_NODE);

		n.push_back(JSONNode("id", id.second));
		n.push_back(JSONNode("idx", outIndex));
		
		if (outIndex == 0)
			n.push_back(JSONNode("sampleIndex", index));
		
		JSONNode streams_data(JSON_ARRAY);
		streams_data.set_name("data");
		
		unsigned nchunks = (device->capture_i - index)/decimateFactor;
		
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
	
		if (isComplete()){
			n.push_back(JSONNode("done", true));
			keep = false;
		}

		n.push_back(JSONNode("_action", "update"));
		client->sendJSON(n);
	}
	return keep;
}
