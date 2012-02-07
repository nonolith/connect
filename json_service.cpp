// Nonolith Connect
// https://github.com/nonolith/connect
// JSON REST endpoint handlers
// Released under the terms of the GNU GPLv3+
// (C) 2012 Nonolith Labs, LLC
// Authors:
//   Kevin Mehall <km@kevinmehall.net>

#include <string>
#include <boost/foreach.hpp>

#include "websocketpp.hpp"

#include "json.hpp"

void devicesRequest(websocketpp::session_ptr client){
	JSONNode n = jsonDevicesArray(true);
	string jc = (string) n.write_formatted();
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
