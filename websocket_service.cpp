#include <iostream>
#include <boost/foreach.hpp>

#include "websocketpp.hpp"
#include "websocket_handler.hpp"

#include "dataserver.hpp"
#include "json.hpp"

class ClientConn{
	public:
	ClientConn(websocketpp::session_ptr c): client(c){
		std::cout << "Opened" <<std::endl;
		l_device_list_changed.subscribe(
			device_list_changed,
			boost::bind(&ClientConn::on_device_list_changed, this)
		);
		l_streaming_state_changed.subscribe(
			streaming_state_changed,
			boost::bind(&ClientConn::on_streaming_state_changed, this)
		);

		on_device_list_changed();
		on_streaming_state_changed();
	}

	~ClientConn(){
		
	}

	websocketpp::session_ptr client;

	EventListener l_device_list_changed;
	EventListener l_streaming_state_changed;

	void on_message(const std::string &msg){
		std::cout << "Recd:" << msg <<std::endl;
	}

	void on_message(const std::vector<unsigned char> &data){
		
	}

	void sendJSON(JSONNode &n){
		string jc = (string) n.write_formatted();
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

	void on_streaming_state_changed(){
		JSONNode n(JSON_NODE);
		n.push_back(JSONNode("_action", "streamstate"));
		n.push_back(JSONNode("streaming", false));

		sendJSON(n);
	}
};

std::map<websocketpp::session_ptr, ClientConn*> connections;

void data_server_handler::on_open(websocketpp::session_ptr client){
	connections.insert(std::pair<websocketpp::session_ptr, ClientConn*>(client, new ClientConn(client)));
}
	
void data_server_handler::on_close(websocketpp::session_ptr client){
	std::map<websocketpp::session_ptr, ClientConn*>::iterator it = connections.find(client);
	delete it->second;
	connections.erase(it);
}

void data_server_handler::on_message(websocketpp::session_ptr client,const std::string &msg){
	connections.find(client)->second->on_message(msg);
}

void data_server_handler::on_message(websocketpp::session_ptr client, const std::vector<unsigned char> &data) {
	connections.find(client)->second->on_message(data);
}