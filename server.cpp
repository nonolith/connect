#include <iostream>
#include "websocketpp.hpp"
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string.hpp>

#include "dataserver.hpp"
#include "websocket_handler.hpp"
#include "demoDevice/demoDevice.hpp"

using boost::asio::ip::tcp;
const unsigned short port = 9003;
const boost::posix_time::seconds rescan_interval = boost::posix_time::seconds(1); 

std::set <device_ptr> devices;
boost::asio::io_service io;

void on_rescan_timer(const boost::system::error_code& /*e*/, boost::asio::deadline_timer* t);

int main(){	
	data_server_handler_ptr handler(new data_server_handler());
	
	try {
		usb_init();

		device_ptr p = device_ptr(new DemoDevice());
		devices.insert(p);

		tcp::endpoint endpoint(tcp::v6(), port);
		websocketpp::server_ptr server(new websocketpp::server(io, endpoint, handler));
		
		server->set_max_message_size(0xFFFF); // 64KiB
		server->start_accept();

		boost::asio::deadline_timer rescan_timer(io, rescan_interval);
        rescan_timer.async_wait(boost::bind(&on_rescan_timer,boost::asio::placeholders::error, &rescan_timer));
		
		io.run();
	} catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
	}
	
	return 0;
}

void on_rescan_timer(const boost::system::error_code& /*e*/, boost::asio::deadline_timer* t){
	t->expires_at(t->expires_at() + rescan_interval);
	t->async_wait(boost::bind(on_rescan_timer,boost::asio::placeholders::error, t));
	usb_scan_devices();
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

void data_server_handler::on_open(websocketpp::session_ptr client){
	std::cout << "Opened" <<std::endl;
}
	
void data_server_handler::on_close(websocketpp::session_ptr client){
	std::cout << "Closed" <<std::endl;
}

void data_server_handler::on_message(websocketpp::session_ptr client,const std::string &msg){
	std::cout << "Recd:" << msg <<std::endl;;
}

void data_server_handler::on_message(websocketpp::session_ptr client,
	const std::vector<unsigned char> &data) {}
