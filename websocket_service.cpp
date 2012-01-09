#include <iostream>
#include <boost/foreach.hpp>

#include "websocketpp.hpp"
#include "websocket_handler.hpp"

#include "dataserver.hpp"
#include "json.hpp"


struct WebsocketClientConn: public ClientConn{
	WebsocketClientConn(websocketpp::session_ptr c): client(c){
		l_device_list_changed.subscribe(
			device_list_changed,
			boost::bind(&WebsocketClientConn::on_device_list_changed, this)
		);

		on_device_list_changed();
	}
	
	void sendJSON(JSONNode &n){
		string jc = (string) n.write();
		if (debugFlag){
			std::cout << "TXD: " << jc <<std::endl;
		}
		client->send(jc);
	}

	void on_device_list_changed(){
		JSONNode n(JSON_NODE);
		JSONNode devices = jsonDevicesArray();
		devices.set_name("devices");

		n.push_back(JSONNode("_action", "devices"));
		n.push_back(devices);

		sendJSON(n);
	}
	
	void on_message(const std::string &msg){
		if (debugFlag){
			std::cout << "RXD: " << msg << std::endl;
		}
		try{
			JSONNode n = libjson::parse(msg);
			string cmd = n.at("_cmd").as_string();

			if (cmd == "selectDevice"){
				string id = n.at("id").as_string();
				selectDevice(getDeviceById(id));
				return;
			}
			
			if (!device){
				std::cerr<<"selectDevice before using other WS calls"<<std::endl;
				return;
			}
			
			if (device->processMessage(*this, cmd, n)) return;
			
			std::cerr << "Unknown command " << cmd << std::endl;
		}catch(std::exception &e){ // TODO: more helpful error message by catching different types
			std::cerr << "WS JSON error:" << e.what() << std::endl;
			return;
		}		
	}
	
	void on_message(const std::vector<unsigned char> &data){
	
	}
	
	websocketpp::session_ptr client;
	EventListener l_device_list_changed;
};

std::map<websocketpp::session_ptr, WebsocketClientConn*> connections;

void data_server_handler::on_open(websocketpp::session_ptr client){
	connections.insert(std::pair<websocketpp::session_ptr, WebsocketClientConn*>(client, new WebsocketClientConn(client)));
}
	
void data_server_handler::on_close(websocketpp::session_ptr client){
	std::map<websocketpp::session_ptr, WebsocketClientConn*>::iterator it = connections.find(client);
	delete it->second;
	connections.erase(it);
}

void data_server_handler::on_message(websocketpp::session_ptr client,const std::string &msg){
	connections.find(client)->second->on_message(msg);
}

void data_server_handler::on_message(websocketpp::session_ptr client, const std::vector<unsigned char> &data) {
	connections.find(client)->second->on_message(data);
}
