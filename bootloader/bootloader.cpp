// Xmega bootloader driver
// http://nonolithlabs.com/cee
// (C) 2012 Kevin Mehall / Nonolith Labs <km@kevinmehall.net>
// Released under the terms of the GNU GPLv3+

#include <stdlib.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <ios>
#include <iomanip>
#include <libusb/libusb.h>
#include <sys/timeb.h>
#include <boost/bind.hpp>
using namespace std;

#include "bootloader.hpp"

Bootloader_device::Bootloader_device(libusb_device *dev, libusb_device_descriptor &desc):
	USB_device(dev, desc){
	
	cerr << "Found a bootloader: "<< serial << endl;
	
	memset(&info, 0, sizeof(info));
	
	getInfo();
}

bool Bootloader_device::processMessage(ClientConn& session, string& cmd, JSONNode& n){
	if (cmd == "erase"){
		erase();
		JSONNode reply;
		reply.push_back(JSONNode("_action", "return"));
		reply.push_back(JSONNode("id", jsonIntProp(n, "id", 0)));
		session.sendJSON(reply);
		
	}else if (cmd == "write"){
		string data = n.at("data").as_binary();
		int r = write(data);
		JSONNode reply;
		reply.push_back(JSONNode("_action", "return"));
		reply.push_back(JSONNode("id", jsonIntProp(n, "id", 0)));
		reply.push_back(JSONNode("result", r));
		session.sendJSON(reply);
	
	}else if (cmd == "crc_app" || cmd == "crc_boot"){
		unsigned crc=0;
		if (cmd == "crc_app"){
			crc = crc_app();
		}else if (cmd == "crc_boot"){
			crc = crc_boot();	
		}
		JSONNode reply;
		reply.push_back(JSONNode("_action", "return"));
		reply.push_back(JSONNode("id", jsonIntProp(n, "id", 0)));
		reply.push_back(JSONNode("crc", crc));
		session.sendJSON(reply);
	}else if (cmd == "reset"){
		reset();
	}else{
		return USB_device::processMessage(session, cmd, n);
	}
	return true;
}

std::string toHex(unsigned val) {
   std::stringstream ss;
   ss << std::uppercase << std::setw(8) << std::setfill('0') << std::hex;
   ss << val;
   return ss.str();
}

void Bootloader_device::onClientAttach(ClientConn *c){
	JSONNode n;
	n.push_back(JSONNode("_action", "info"));
	n.push_back(JSONNode("serial", serial));
	n.push_back(JSONNode("magic", toHex(ntohl(info.magic))));
	n.push_back(JSONNode("version", info.version));
	n.push_back(JSONNode("devid", toHex(ntohl(info.devid))));
	n.push_back(JSONNode("page_size", info.page_size));
	n.push_back(JSONNode("app_section_end", info.app_section_end));
	n.push_back(JSONNode("hw_product", string(info.hw_product, 16)));
	n.push_back(JSONNode("hw_version", string(info.hw_version, 16)));
	c->sendJSON(n);
}

void Bootloader_device::getInfo(){
	int r = controlTransfer(0x40|0x80, REQ_INFO, 0, 0, (unsigned char*) &info, sizeof(info));
	std::cerr<<"bootloader: getInfo "<<r<<std::endl;
}

void Bootloader_device::erase(){
	controlTransfer(0x40|0x80, REQ_ERASE, 0, 0, NULL, 0);
}

int Bootloader_device::write(string data){
	controlTransfer(0x40|0x80, REQ_START_WRITE, 0, 0, NULL, 0);
	std::cout << "Starting bootloader write " << info.page_size << " " << data.size() << std::endl;
	
	int transferred=0, r=0;
	r = libusb_bulk_transfer(handle, 1, (unsigned char*)data.c_str(), data.size(), &transferred, 1000);
	std::cout << "Wrote: " << transferred << ", " << r <<std::endl;
	return r;
}

unsigned Bootloader_device::crc_app(){
	unsigned ret=0;
	controlTransfer(0x40|0x80, REQ_CRC_APP, 0, 0, (unsigned char*) &ret, 64);
	return ret;
}

unsigned Bootloader_device::crc_boot(){
	unsigned ret=0;
	controlTransfer(0x40|0x80, REQ_CRC_BOOT, 0, 0, (unsigned char*) &ret, 64);
	return ret;
}

void Bootloader_device::reset(){
	controlTransfer(0x40|0x80, REQ_RESET, 0, 0, NULL, 0);
}
