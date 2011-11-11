#include <string>
#include "libjson/libjson.h"

#include "dataserver.hpp"
#include "websocketpp.hpp"

#include <boost/foreach.hpp>


JSONNode toJSON(OutputChannel* channel){
	JSONNode n(JSON_NODE);
	n.set_name(channel->id);
	n.push_back(JSONNode("id", channel->id));
	return n;
}

JSONNode toJSON(InputChannel* channel){
	JSONNode n(JSON_NODE);
	n.set_name(channel->id);
	n.push_back(JSONNode("id", channel->id));
	n.push_back(JSONNode("displayName", channel->displayName));
	n.push_back(JSONNode("units", channel->units));
	return n;
}

JSONNode toJSON(device_ptr d){
	JSONNode n(JSON_NODE);
	n.push_back(JSONNode("model", d->model()));
	n.push_back(JSONNode("hwversion", d->hwversion()));
	n.push_back(JSONNode("fwversion", d->fwversion()));
	n.push_back(JSONNode("serial", d->serialno()));

	JSONNode inputChannels(JSON_NODE);
	inputChannels.set_name("inputChannels");
	BOOST_FOREACH (InputChannel* c, d->input_channels){
		inputChannels.push_back(toJSON(c));
	}
	n.push_back(inputChannels);

	JSONNode outputChannels(JSON_NODE);
	outputChannels.set_name("outputChannels");
	BOOST_FOREACH (OutputChannel* c, d->output_channels){
		outputChannels.push_back(toJSON(c));
	}
	n.push_back(outputChannels);

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