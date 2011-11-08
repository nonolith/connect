
#include "dataserver.hpp"
#include "websocketpp.hpp"


void devicesRequest(websocketpp::session_ptr client){
	
	client->start_http(200);
}


void handleJSONRequest(std::vector<std::string> &pathparts, websocketpp::session_ptr client){
	if (pathparts[2] != "v0"){
		client->start_http(404, "API version not supported");
	}

	if (pathparts[3] == "devices"){
		devicesRequest(client);
	}else{
		client->start_http(404, "REST object not found");
	}
}