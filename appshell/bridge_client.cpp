// Nonolith Connect
// https://github.com/nonolith/connect
// Websocket connection management
// Released under the terms of the GNU GPLv3+
// (C) 2013 Nonolith Labs, LLC
// Authors:
//   Kevin Mehall <km@kevinmehall.net>

#include <iostream>
#include <boost/foreach.hpp>

#include "dataserver.hpp"
#include "json.hpp"
#include "ui.h"

BridgeClientConn::BridgeClientConn(Bridge& b): bridge(b){}

void BridgeClientConn::connect() {
	l_device_list_changed.subscribe(
		device_list_changed,
		boost::bind(&BridgeClientConn::on_device_list_changed, this)
	);
	
	JSONNode n(JSON_NODE);
	n.push_back(JSONNode("_action", "serverHello"));
	n.push_back(JSONNode("server", "Nonolith Connect"));
	n.push_back(JSONNode("version", server_version));
	n.push_back(JSONNode("gitVersion", server_git_version));
	sendJSON(n);
	
	on_device_list_changed();
}
	
void BridgeClientConn::sendJSON(JSONNode &n){
	string jc = (string) n.write();
	if (debugFlag){
		std::cout << "TXD: " << jc <<std::endl;
	}
	bridge.onMessage(QString::fromStdString(jc));
}

void BridgeClientConn::on_device_list_changed(){
	JSONNode n(JSON_NODE);
	JSONNode devices = jsonDevicesArray();
	devices.set_name("devices");

	n.push_back(JSONNode("_action", "devices"));
	n.push_back(devices);

	sendJSON(n);
}
	
void BridgeClientConn::on_message(const std::string &msg){
	if (debugFlag){
		std::cout << "RXD: " << msg << std::endl;
	}

	int id=0;

	try{
		JSONNode n = libjson::parse(msg);
		string cmd = n.at("_cmd").as_string();
		id = jsonIntProp(n, "id", 0); // Collect the id to use in error message

		if (cmd == "selectDevice"){
			string id = n.at("id").as_string();
			device_ptr dev = getDeviceById(id);
			if (dev){
				selectDevice(dev);
			}else{
				std::cerr << "Error selecting device " << id << std::endl;
			}
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

		JSONNode j_error = JSONNode();
		j_error.push_back(JSONNode("_action", "error"));
		j_error.push_back(JSONNode("error", e.what()));
		if (id != 0) j_error.push_back(JSONNode("id", id));
		sendJSON(j_error);

		return;
	}
}

void BridgeClientConn::handleMessageToServer(std::string msg) {
	io.post(boost::bind(&BridgeClientConn::on_message, this, msg));
}

void BridgeClientConn::handleConnect(){
	io.post(boost::bind(&BridgeClientConn::connect, this));
}
