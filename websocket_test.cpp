#include "websocketpp.hpp"
#include <boost/asio.hpp>
#include <iostream>

#include "websocket_handler.hpp"

using boost::asio::ip::tcp;

int main(){
	std::string host = "localhost";
	unsigned short port = 9003;
	std::string full_host;
	
	data_server_handler_ptr handler(new data_server_handler());
	
	try {
		boost::asio::io_service io_service;
		tcp::endpoint endpoint(tcp::v6(), port);
		
		websocketpp::server_ptr server(
			new websocketpp::server(io_service, endpoint, handler)
		);
		
		server->set_max_message_size(0xFFFF); // 64KiB
		
		server->start_accept();
		
		std::cout << "Starting chat server on " << full_host << std::endl;
		
		io_service.run();
	} catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
	}
	
	return 0;
}

void data_server_handler::on_client_connect(websocketpp::session_ptr client){
	const std::string resource = client->get_resource();

	if (resource == "/"){
		client->set_header("Location", "/hello");
		client->start_http(301);
	}else if (resource == "/hello"){
		client->start_http(200, "<h1>Hello World!</h1>"+resource);
	}else{
		client->start_http(404, "Not found");
	}
	
}

void data_server_handler::on_open(websocketpp::session_ptr client){}
	
void data_server_handler::on_close(
	websocketpp::session_ptr client,uint16_t status,const std::string &reason){}

void data_server_handler::on_message(websocketpp::session_ptr client,const std::string &msg){}

void data_server_handler::on_message(websocketpp::session_ptr client,
	const std::vector<unsigned char> &data) {}