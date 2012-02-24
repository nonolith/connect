#include "streaming_device.hpp"
#include "stream_listener.hpp"

//// device/channel/output resource

void StreamingDevice::RESTOutputRespond(websocketpp::session_ptr client, Channel *channel){
	JSONNode n;
	channel->source->describeJSON(n);
	respondJSON(client, n);
}

void StreamingDevice::handleRESTOutputCallback(websocketpp::session_ptr client, Channel* channel, string postdata){
	JSONNode n = libjson::parse(postdata);
	setOutput(channel, makeSource(n));
	RESTOutputRespond(client, channel);
}

bool StreamingDevice::handleRESTOutput(UrlPath path, websocketpp::session_ptr client, Channel* channel){
	if (client->get_method() == "POST"){
		client->read_http_post_body(
			boost::bind(
				&StreamingDevice::handleRESTOutputCallback,
				boost::static_pointer_cast<StreamingDevice>(shared_from_this()),
				client, channel, _1));
	}else{
		if (!channel->source) return false;
		RESTOutputRespond(client, channel);
	}

	return true;
}

//// device/channel/input resource

struct RESTListener: public StreamListener{
	websocketpp::session_ptr client;
	boost::shared_ptr<StreamingDevice> device;
	
	virtual bool handleNewData(){
		unsigned nchunks = howManySamples();
		if (!nchunks) return true;
	}
};

bool StreamingDevice::handleRESTInput(UrlPath path, websocketpp::session_ptr client, Channel* channel){
	client->start_http(200, "datadatadata\n");
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
			return handleRESTOutput(spath.sub(), client, channel);
		}else if (spath.matches("input")){
			return handleRESTInput(spath.sub(), client, channel);
		}	
		
		return false;
	}
}

