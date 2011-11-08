#include <string>
#include "libjson/libjson.h"

#include "dataserver.hpp"
#include "websocketpp.hpp"


JSONNode deviceToJSON(device_ptr d){
	JSONNode n(JSON_NODE);
	n.push_back(JSONNode("serial", d->serialno()));
	return n;
}

void devicesRequest(websocketpp::session_ptr client){
	JSONNode n(JSON_ARRAY);

	for (std::vector<device_ptr>::iterator it = devices.begin(); it != devices.end(); ++it){
		n.push_back(deviceToJSON(*it));
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