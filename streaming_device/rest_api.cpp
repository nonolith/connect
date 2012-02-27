#include "streaming_device.hpp"
#include "stream_listener.hpp"
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <sstream>
#include <boost/foreach.hpp>

//// device/channel/output resource

void StreamingDevice::RESTOutputRespond(websocketpp::session_ptr client, Channel *channel){
	JSONNode n;
	channel->source->describeJSON(n);
	respondJSON(client, n);
}

void StreamingDevice::handleRESTOutputCallback(websocketpp::session_ptr client, Channel* channel, string postdata){
	try{
		if (postdata[0] == '{'){
			JSONNode n = libjson::parse(postdata);
			setOutput(channel, makeSource(n));
		}else{
			std::map<string, string> map;
			parse_query(postdata, map);
			float value = boost::lexical_cast<float>(map_get(map, "value", "0"));
			string mode = map_get(map, "mode", "0");
			boost::algorithm::to_lower(mode);
			unsigned modeval = 0;
			if (mode == "0" || mode == "disabled"){ // Note: cee-specific
				modeval = 0;
			}else if (mode == "1" || mode == "svmi"){
				modeval = 1;
			}else if (mode == "2" || mode == "simv"){
				modeval = 2;
			}
			setOutput(channel, makeConstantSource(modeval, value));
		}
		RESTOutputRespond(client, channel);
	}catch(std::exception e){
		std::cerr << "Exception while processing request: " << e.what() <<std::endl;
		client->start_http(402);
	}
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
	bool csv;
	
	virtual bool handleNewData(){
		if (client->is_closed()){
			std::cout << "socket is closed" << std::endl;
			return false;
		}
	
		unsigned nchunks = howManySamples();
		if (!nchunks) return true;
		
		std::ostringstream o(std::ostringstream::out);
		
		for (unsigned chunk = 0; chunk < nchunks; chunk++){
			bool first = true;
			BOOST_FOREACH(Stream* s, streams){
				if (!first){
					o << ", ";
				}else{
					first = false;
				}
				o << device->resample(*s, index+chunk*decimateFactor, decimateFactor);
			}
			o << "\n";
		}
	
		index += nchunks * decimateFactor;
		outIndex += nchunks;

		bool done = (count>0 && (int) outIndex >= count);
		
		client->http_write(o.str(), done);
		return !done;
	}
};

bool StreamingDevice::handleRESTInput(UrlPath path, websocketpp::session_ptr client, Channel* channel){
	
	boost::shared_ptr<RESTListener> l = boost::shared_ptr<RESTListener>(new RESTListener());
	
	l->client = client;
	l->device = this;
	l->streams = channel->streams;
	l->csv = 1;
	
	float resample_s = boost::lexical_cast<float>(path.param("resample", "0.01"));
	l->decimateFactor = round(resample_s / sampleTime);
	
	// Prevent divide by 0
	if (l->decimateFactor == 0) l->decimateFactor = 1;

	int start = boost::lexical_cast<int>(path.param("start", "-1"));
	if (start < 0){ // Negative indexes are relative to latest sample
		start = (buffer_max()) + start + 1;
	}
	if (start < 0) l->index = 0;
	else l->index = start;
	
	l->count = boost::lexical_cast<unsigned>(path.param("count", "0"));
	
	addListener(l);
	client->start_http(200, "#csv header\n", false);
	return true;
}

/// Device resource

void StreamingDevice::RESTDeviceRespond(websocketpp::session_ptr client){
	JSONNode jstate = stateToJSON();
	respondJSON(client, jstate);
}

void StreamingDevice::handleRESTDeviceCallback(websocketpp::session_ptr client, string postdata){
	try{
		if (postdata[0] == '{'){
			//TODO: json
		}else{
			std::map<string, string> map;
			parse_query(postdata, map);
			
			string captureState = map_get(map, "capture");
			if (captureState == "true" || captureState == "on"){
				start_capture();
			}else if (captureState == "false" || captureState == "off"){
				pause_capture();
			}

		}
		RESTDeviceRespond(client);
	}catch(std::exception e){
		std::cerr << "Exception while processing request: " << e.what() <<std::endl;
		client->start_http(402);
	}
}

/// Dispatch

bool StreamingDevice::handleREST(UrlPath path, websocketpp::session_ptr client){
	if (path.leaf()){
		if (client->get_method() == "POST"){
			client->read_http_post_body(
				boost::bind(
					&StreamingDevice::handleRESTDeviceCallback,
					boost::static_pointer_cast<StreamingDevice>(shared_from_this()),
					client,  _1));
		}else{
			RESTDeviceRespond(client);
		}
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

