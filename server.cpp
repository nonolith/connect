#include <iostream>
#include "websocketpp.hpp"
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string.hpp>

#include "dataserver.hpp"
#include "websocket_handler.hpp"

using boost::asio::ip::tcp;
const unsigned short port = 9003;

std::set <device_ptr> devices;
boost::asio::io_service io;

bool debugFlag;

Event device_list_changed;
Event capture_state_changed;

int main(int argc, char* argv[]){	
	data_server_handler_ptr handler(new data_server_handler());
	
	try {
		usb_init();

		if (argc==2 && string(argv[1])=="debug"){
			debugFlag = true;
		}
		else{
			debugFlag = false;
		}
		tcp::endpoint endpoint(tcp::v6(), port);
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
	}else if (pathparts[1] == "tcp"){
		//TODO: start TCP	
	}else{
		client->start_http(404, "Not found");
	}
}
