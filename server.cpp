// Nonolith Connect
// https://github.com/nonolith/connect
// Server initialization and request dispatch
// Released under the terms of the GNU GPLv3+
// (C) 2012 Nonolith Labs, LLC
// Authors:
//   Kevin Mehall <km@kevinmehall.net>

#include <iostream>
#include "websocketpp.hpp"
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/regex.hpp>

#include "dataserver.hpp"
#include "websocket_handler.hpp"
#include "url.hpp"

using boost::asio::ip::tcp;
const unsigned short port = 9003;

std::set <device_ptr> devices;
boost::asio::io_service io;

bool debugFlag = false;
bool allowRemote = false;
bool allowAnyOrigin = false;

Event device_list_changed;
Event capture_state_changed;

int server_main(int argc, char* argv[]){
	data_server_handler_ptr handler(new data_server_handler());
	
	try {
		usb_init();
		
		bool net = false;
		for (int i=1; i<argc; i++){
			string arg(argv[i]);
			if (arg=="debug") debugFlag = true;
			if (arg=="allow-remote") allowRemote = true;
			if (arg=="allow-any-origin") allowAnyOrigin = true;
			if (arg=="net") net = true;
		}
		
		if (net) {
			boost::asio::ip::address_v4 bind_addr;
			if (!allowRemote) bind_addr = boost::asio::ip::address_v4::loopback();
			
			tcp::endpoint endpoint(bind_addr, port);
			websocketpp::server_ptr server(new websocketpp::server(io, endpoint, handler));
			
			server->set_max_message_size(0xFFFFF); // 1024KiB
			server->set_elog_level(0);
			server->start_accept();
		}

		usb_scan_devices();
		
		boost::asio::io_service::work work(io);

		io.run();
	} catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
	}
	
	return 0;
}

void do_shutdown() {
	usb_fini();
	io.stop();
}

void server_shutdown() {
	io.post(do_shutdown);
}

const string redir_url = "http://www.nonolithlabs.com/connect/?utm_source=connect&utm_medium=app&utm_campaign=server-redir";

void handleJSONRequest(UrlPath path, websocketpp::session_ptr client);

void data_server_handler::on_client_connect(websocketpp::session_ptr client){
	static const boost::regex nonolith_domain("^https?://[[:w:]\\.-]*?nonolithlabs.com$");
	static const boost::regex localhost_domain("^https?://localhost(:[[:d:]]+)?$");
	const string origin = client->get_client_header("Origin");

	if (!allowAnyOrigin 
			&& origin!=""
			&& origin!="null"
			&& !regex_match(origin, localhost_domain) 
			&& !regex_match(origin, nonolith_domain)
			){
		client->start_http(403, "Origin not allowed");
		std::cerr << "Rejected client with unknown origin " << origin << std::endl;
		return;
	}
	
	const std::string resource = client->get_resource();
	Url url(resource);
	UrlPath path(url, 1);

	try{
		if (path.leaf()){ // "/"
			client->set_header("Location", redir_url);
			client->start_http(301);
		}else if (path.matches("rest")){
			handleJSONRequest(path.sub(), client);
		}else if (path.matches("ws")){
			client->start_websocket();
		}else{
			client->start_http(404, "Not found");
		}
	}catch(std::exception& e){
		JSONNode j;
		j.push_back(JSONNode("error", e.what()));
		std::cerr << "Exception while processing request: " << e.what() <<std::endl;
		respondJSON(client, j, 500);
	}
}
