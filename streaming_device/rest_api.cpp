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
			float value = map_get_num(map, "value", 0.0);
			string mode = map_get(map, "mode", "0");
			boost::algorithm::to_lower(mode);
			unsigned modeval = 0;
			if (mode == "0" || mode == "disabled" || mode == "d"){ // Note: cee-specific
				modeval = 0;
			}else if (mode == "1" || mode == "svmi" || mode == "v"){
				modeval = 1;
			}else if (mode == "2" || mode == "simv" || mode == "i"){
				modeval = 2;
			}
			
			string source = map_get(map, "wave", "constant");
			OutputSource* sourceObj;
			
			if (source == "constant"){
				sourceObj = makeConstantSource(modeval, value);
			}else if (source == "adv_square"){
				float value1 = map_get_num(map, "value1", 0.0);
				float value2 = map_get_num(map, "value2", 0.0);
				int time1 = map_get_num(map, "time1", 0.5)/sampleTime;
				if (time1 <= 0) time1 = 1;
				int time2 = map_get_num(map, "time2", 0.5)/sampleTime;
				if (time2 <= 0) time2 = 1;
				int phase = map_get_num(map, "phase", 0)/sampleTime;
				sourceObj = makeAdvSquare(modeval, value1, value2, time1, time2, phase);
				
			}else{
				float amplitude = map_get_num(map, "amplitude", 0.0);
				double freq = map_get_num(map, "frequency", 1.0);
				if (freq <= 0){freq = 0.001;}
				double period = 1/sampleTime/freq;
				double phase = map_get_num(map, "phase", 1.0)/sampleTime;
				bool relPhase = (map_get(map, "relPhase", "1") == "1");
		
				sourceObj = makeSource(modeval, source, value, amplitude, period, phase, relPhase);
			}
			
			setOutput(channel, sourceObj);
		}
		RESTOutputRespond(client, channel);
	}catch(std::exception& e){
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
	
	virtual bool handleNewData(){
		if (client->is_closed()){
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
	
	virtual ~RESTListener(){
		if (client && !client->is_closed()) client->http_write("", true);
	}
};

bool StreamingDevice::handleRESTInput(UrlPath path, websocketpp::session_ptr client, Channel* channel){
	if (client->get_method() == "POST"){
		client->read_http_post_body(
			boost::bind(
				&StreamingDevice::handleRESTInputPOSTCallback,
				boost::static_pointer_cast<StreamingDevice>(shared_from_this()),
				client, channel, _1));
	}else{
		boost::shared_ptr<RESTListener> l = boost::shared_ptr<RESTListener>(new RESTListener());
	
		l->client = client;
		l->device = this;
		l->streams = channel->streams;
	
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
	
		l->count = boost::lexical_cast<unsigned>(path.param("count", "1"));
		bool header = (path.param("header", "1") == "1");
	
		std::ostringstream o(std::ostringstream::out);
	
		if (header){
			bool first = true;
			BOOST_FOREACH(Stream* s, l->streams){
				if (!first){
					o << ",";
				}else first = false;
				o << s->displayName << " (" << s->units << ")" ;
			}
			o<<"\n";
		}
	
		client->start_http(200, o.str(), false);
	
		addListener(l);
	}
	return true;
}

void StreamingDevice::handleRESTInputPOSTCallback(websocketpp::session_ptr client, Channel* channel, string postdata){
	try{
		std::map<string, string> map;
		parse_query(postdata, map);
		
		JSONNode g;
		BOOST_FOREACH(Stream* s, channel->streams){
			double gain = map_get_num(map, "gain_"+s->id, 0.0);
			if (gain != 0.0) setGain(channel, s, gain);
			g.push_back(JSONNode(s->id, s->getGain()));
		}
		
		JSONNode r;
		g.set_name("gain");
		r.push_back(g);
		
		respondJSON(client, r);
			
	}catch(std::exception& e){
		std::cerr << "Exception while processing request: " << e.what() <<std::endl;
		client->start_http(402);
	}
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
			if (captureState == "true" || captureState == "on" || captureState == "1"){
				start_capture();
			}else if (captureState == "false" || captureState == "off" || captureState == "0"){
				pause_capture();
			}

		}
		RESTDeviceRespond(client);
	}catch(std::exception& e){
		std::cerr << "Exception while processing request: " << e.what() <<std::endl;
		client->start_http(402);
	}
}

/// Channel resource

void handleRESTChannel(websocketpp::session_ptr client, Channel* channel){
	JSONNode n = channel->toJSON();
	respondJSON(client, n);
}

/// Configuration resource

void StreamingDevice::RESTConfigurationRespond(websocketpp::session_ptr client){
	JSONNode jstate = stateToJSON(true);
	respondJSON(client, jstate);
}

void StreamingDevice::handleRESTConfigurationCallback(websocketpp::session_ptr client, string postdata){
	try{
		if (postdata[0] == '{'){
			//TODO: json
		}else{
			std::map<string, string> map;
			parse_query(postdata, map);
			
			unsigned samples = map_get_num(map, "samples", captureSamples);
			
			double _sampleTime = map_get_num(map, "sampleTime", sampleTime);
			if (_sampleTime <= 0) _sampleTime = sampleTime;
			if (_sampleTime > 0.001) _sampleTime = 0.001;
			
			unsigned current = map_get_num(map, "currentLimit", 0);
			setCurrentLimit(current);
			
			configure(devMode, _sampleTime, samples, captureContinuous, rawMode);
			
		}
		RESTConfigurationRespond(client);
	}catch(std::exception& e){
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
		if (path.matches("configuration")){
			if (client->get_method() == "POST"){
				client->read_http_post_body(
					boost::bind(
						&StreamingDevice::handleRESTConfigurationCallback,
						boost::static_pointer_cast<StreamingDevice>(shared_from_this()),
						client,  _1));
			}else{
				RESTConfigurationRespond(client);
			}
			return true;
		
		}else{
			Channel *channel = channelById(path.get());
			if (!channel) return false;
		
			UrlPath spath = path.sub();
			if (spath.leaf()){
				handleRESTChannel(client, channel);
				return true;
			};
		
			if (spath.matches("output")){
				return handleRESTOutput(spath.sub(), client, channel);
			}else if (spath.matches("input")){
				return handleRESTInput(spath.sub(), client, channel);
			}
		}		
		return false;
	}
}

