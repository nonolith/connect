#include <boost/foreach.hpp>
#include "dataserver.hpp"
#include <iostream>


device_ptr getDeviceById(string id){
	BOOST_FOREACH(device_ptr d, devices){
		if (d->getId() == id) return d;
	}
	return device_ptr();
}

void Device::broadcastJSON(JSONNode& n){
	BOOST_FOREACH(ClientConn* c, connections){
		c->sendJSON(n);
	}
}

void Device::notifyDisconnect(){
	JSONNode n(JSON_NODE);
	n.push_back(JSONNode("_action", "deviceDisconnected"));
	broadcastJSON(n);
}

