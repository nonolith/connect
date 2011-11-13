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

void handleJSONRequest(std::vector<std::string> &pathparts, websocketpp::session_ptr client){
	if (pathparts[2] != "v0"){
		client->start_http(404, "API version not supported");
		return;
	}

	if (pathparts[3] == "devices"){
		devicesRequest(client);
	}else{
		client->start_http(404, "REST object not found");
	}
}