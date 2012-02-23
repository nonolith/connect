// Nonolith Connect
// https://github.com/nonolith/connect
// Device header file
// Released under the terms of the GNU GPLv3+
// (C) 2012 Nonolith Labs, LLC
// Authors:
//   Kevin Mehall <km@kevinmehall.net>

#pragma once
#ifndef INCLUDE_FROM_DATASERVER_HPP
#error "device.hpp should not be included directly"
#endif

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "url.hpp"

class ClientConn;

class Device: public boost::enable_shared_from_this<Device> {
	public: 
		Device(){}
		virtual ~Device(){};
		
		virtual JSONNode toJSON();
		
		std::set<ClientConn*> connections;
		
		virtual void onClientAttach(ClientConn *c){
			connections.insert(c);
		}
		
		virtual void onClientDetach(ClientConn *c){
			connections.erase(c);
		}
		
		virtual const string getId(){return model()+"~"+serialno();}
		virtual const string serialno(){return "0";}
		virtual const string model() {return "unknown";}
		virtual const string hwVersion(){return "unknown";}
		virtual const string fwVersion(){return "unknown";}
		
		virtual bool processMessage(ClientConn& session, string& cmd, JSONNode& n){ return false; }
		virtual bool handleREST(UrlPath path, websocketpp::session_ptr client){return false;}
		
		virtual void onDisconnect();
		
		protected:
			void broadcastJSON(JSONNode &n);
};

typedef boost::shared_ptr<Device> device_ptr;

device_ptr getDeviceById(string id);

class ClientConn{
	public:
		device_ptr device;
		
	virtual ~ClientConn(){
		if (device) device->onClientDetach(this);
	}
	
	virtual void selectDevice(device_ptr dev){
		if (device){
			device->onClientDetach(this);
		}
		device = dev;
		device->onClientAttach(this);
	}
	
	virtual void sendJSON(JSONNode &n) = 0;
};

