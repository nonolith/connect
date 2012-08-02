// Nonolith Connect
// https://github.com/nonolith/connect
// USB device mixin
// Released under the terms of the GNU GPLv3+
// (C) 2012 Nonolith Labs, LLC
// Authors:
//   Kevin Mehall <km@kevinmehall.net>

#pragma once
#include <iostream>
#include <libusb/libusb.h>

class USB_device{
	public:
	int controlTransfer(uint8_t bmRequestType,
                            uint8_t bRequest,
                            uint16_t wValue,
                            uint16_t wIndex,
                            uint8_t* data,
                            uint16_t wLength,
                            unsigned timeout=25){
		return libusb_control_transfer(handle,
		           bmRequestType, bRequest, wValue, wIndex, data, wLength, timeout);                      
	}
	

	protected:
	USB_device(libusb_device *dev, libusb_device_descriptor &desc){
		int r = libusb_open(dev, &handle);
		if (r != 0){
			std::cerr << "Could not open device; error "<< r <<std::endl;
			throw ErrorStringException("Could not open device");
		}

		r = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, (unsigned char*)&serial, 32);
		if (r < 0){
			std::cerr << "Could not read string descriptor; error " << r << std::endl;
			throw ErrorStringException("Could not read serial number");
		}
	}
	
	void claimInterface(){
		int r = libusb_claim_interface(handle, 0);
		if (r != 0){
			std::cerr << "Could not claim interface; error "<<r<<std::endl;
			throw ErrorStringException("Could not claim interface");
		}
	}

	void releaseInterface(){
		if (!handle) return;
		int r = libusb_release_interface(handle, 0);
		if (r != 0 && r != -4){ // Ignore "device disconnected" errors
			std::cerr << "Could not release interface; error "<<r<<std::endl;
			throw ErrorStringException("Could not release interface");
		}
	}
	
	virtual ~USB_device(){
		if (handle) libusb_close(handle);
	}
	
	libusb_device_handle *handle;
	char serial[32];
	
	virtual bool processMessage(ClientConn& session, string& cmd, JSONNode& n);
};

#ifdef __MINGW32__
// Function is not defined in mingw
static inline size_t strnlen (const char *string, size_t maxlen){
  const char *end = (const char*) memchr(string, '\0', maxlen);
  return end ? (size_t) (end - string) : maxlen;
}
#endif
