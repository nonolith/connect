#include "streaming_device.hpp"
#include "stream_listener.hpp"

//// device/channel/output resource

void RESTOutputRespond(websocketpp::session_ptr client, Channel *channel){
	JSONNode n;
	channel->source->describeJSON(n);
	respondJSON(client, n);
}

void handleRESTOutputCallback(websocketpp::session_ptr client, StreamingDevice* device, device_ptr ptr, Channel* channel, string postdata){
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

//// device/channel/input resource

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

