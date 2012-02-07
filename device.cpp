// Nonolith Connect
// https://github.com/nonolith/connect
// Device utility functions
// Released under the terms of the GNU GPLv3+
// (C) 2012 Nonolith Labs, LLC
// Authors:
//   Kevin Mehall <km@kevinmehall.net>

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

void Device::onDisconnect(){
	JSONNode n(JSON_NODE);
	n.push_back(JSONNode("_action", "deviceDisconnected"));
	broadcastJSON(n);
}

