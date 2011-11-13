#include <string>
#include "libjson/libjson.h"

#include "dataserver.hpp"
#include "websocketpp.hpp"

#include <boost/foreach.hpp>


JSONNode toJSON(OutputStream* s){
	JSONNode n(JSON_NODE);
	n.set_name(s->id);
	n.push_back(JSONNode("id", s->id));
	return n;
}

JSONNode toJSON(InputStream* s){
	JSONNode n(JSON_NODE);
	n.set_name(s->id);
	n.push_back(JSONNode("id", s->id));
	n.push_back(JSONNode("displayName", s->displayName));
	n.push_back(JSONNode("units", s->units));
	return n;
}

JSONNode toJSON(Channel *channel){
	JSONNode n(JSON_NODE);
	n.set_name(channel->id);
	n.push_back(JSONNode("id", channel->id));
	n.push_back(JSONNode("displayName", channel->displayName));

	JSONNode inputChannels(JSON_NODE);
	inputChannels.set_name("inputs");
	BOOST_FOREACH (InputStream* i, channel->inputs){
		inputChannels.push_back(toJSON(i));
	}
	n.push_back(inputChannels);

	JSONNode outputChannels(JSON_NODE);
	outputChannels.set_name("outputs");
	BOOST_FOREACH (OutputStream* i, channel->outputs){
		outputChannels.push_back(toJSON(i));
	}
	n.push_back(outputChannels);

	return n;
}

JSONNode toJSON(device_ptr d){
	JSONNode n(JSON_NODE);
	n.push_back(JSONNode("model", d->model()));
	n.push_back(JSONNode("hwversion", d->hwversion()));
	n.push_back(JSONNode("fwversion", d->fwversion()));
	n.push_back(JSONNode("serial", d->serialno()));

	JSONNode channels(JSON_NODE);
	channels.set_name("channels");
	BOOST_FOREACH (Channel* c, d->channels){
		channels.push_back(toJSON(c));
	}
	n.push_back(channels);


	return n;
}

void devicesRequest(websocketpp::session_ptr client){
	JSONNode n(JSON_ARRAY);

	BOOST_FOREACH (device_ptr d, devices){
		n.push_back(toJSON(d));
	}

	std::string jc = (std::string) n.write_formatted();
	client->start_http(200, jc);
}

void startRequest(){
	BOOST_FOREACH (device_ptr d, devices){
		d->start_streaming(1000);
	}
}

void stopRequest(){
	BOOST_FOREACH (device_ptr d, devices){
		d->stop_streaming();
	}
}

void rawDataRequest(websocketpp::session_ptr client){
	JSONNode n(JSON_NODE);

	BOOST_FOREACH (device_ptr d, devices){
		JSONNode dn(JSON_NODE);
		dn.set_name(d->serialno());
		BOOST_FOREACH(Channel* c, d->channels){
			JSONNode cn(JSON_NODE);
			cn.set_name(c->id);
			BOOST_FOREACH (InputStream* i, c->inputs){
				JSONNode sn(JSON_ARRAY);
				sn.set_name(i->id);

				for(unsigned x=0; x<i->buffer_fill_point; x++){
					sn.push_back(JSONNode("", i->data[x]));
				}

				cn.push_back(sn);
			}
			
			dn.push_back(cn);	
		}
		n.push_back(dn);
	}

	std::string jc = (std::string) n.write_formatted();
	client->start_http(200, jc);
}

void handleJSONRequest(std::vector<std::string> &pathparts, websocketpp::session_ptr client){
	if (pathparts[2] != "v0"){
		client->start_http(404, "API version not supported");
		return;
	}

	if (pathparts[3] == "devices"){
		devicesRequest(client);
	}else if (pathparts[3] == "start" /*&& client->m_http_method=="POST"*/){
		startRequest();
		client->start_http(200, "true");
	}else if (pathparts[3] == "stop" /*&& client->m_http_method=="POST"*/){
		stopRequest();
		client->start_http(200, "true");
	}else if (pathparts[3] == "raw_data"){
		rawDataRequest(client);
	}else{
		client->start_http(404, "REST object not found");
	}
}