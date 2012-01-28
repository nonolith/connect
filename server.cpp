#include <iostream>
#include "websocketpp.hpp"
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>

#include "dataserver.hpp"
#include "websocket_handler.hpp"

using boost::asio::ip::tcp;
const unsigned short port = 9003;

std::set <device_ptr> devices;
boost::asio::io_service io;

bool debugFlag = false;
bool allowRemote = false;
bool allowAnyOrigin = false;

Event device_list_changed;
Event capture_state_changed;

int main(int argc, char* argv[]){	
	data_server_handler_ptr handler(new data_server_handler());
	
	try {
		usb_init();
		
		for (int i=1; i<argc; i++){
			string arg(argv[i]);
			if (arg=="debug") debugFlag = true;
			if (arg=="allow-remote") allowRemote = true;
			if (arg=="allow-any-origin") allowAnyOrigin = true;
		}
		
		boost::asio::ip::address_v4 bind_addr;
		if (!allowRemote) bind_addr = boost::asio::ip::address_v4::loopback();
		
		tcp::endpoint endpoint(bind_addr, port);
		websocketpp::server_ptr server(new websocketpp::server(io, endpoint, handler));
		
		server->set_max_message_size(0xFFFF); // 64KiB
		server->start_accept();

		usb_scan_devices();
		
		io.run();
	} catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
	}
	
	return 0;
}

void handleJSONRequest(std::vector<std::string> &pathparts, websocketpp::session_ptr client);

void data_server_handler::on_client_connect(websocketpp::session_ptr client){
	static const boost::regex nonolith_domain("^https?://[[:w:]\\.-]*?nonolithlabs.com$");
	const string origin = client->get_client_header("Origin");

	if (!allowAnyOrigin && origin!="" && origin!="null" && 
	  origin!="http://localhost:8000" && !regex_match(origin, nonolith_domain)){
		client->start_http(403);
		std::cerr << "Rejected client with unknown origin " << origin << std::endl;
		return;
	}
	
	const std::string resource = client->get_resource();

	std::vector<std::string> pathparts;
	boost::split(pathparts, resource, boost::is_any_of("/"));

	if (resource == "/"){
		client->set_header("Location", "/json/v0/devices");
		client->start_http(301);
	}else if (pathparts[1] == "json"){
		handleJSONRequest(pathparts, client);
	}else if (pathparts[1] == "ws"){
		client->start_websocket();
	//}else if (pathparts[1] == "tcp"){
		//TODO: start TCP	
	}else{
		client->start_http(404, "Not found");
	}
}
