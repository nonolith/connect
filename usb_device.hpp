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
			return;
		}
	
		r = libusb_claim_interface(handle, 0);
		if (r != 0){
			std::cerr << "Could not claim interface; error "<<r<<std::endl;
			return;
		}

		r = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, (unsigned char*)serial, 32);
		if (r < 0){
			std::cerr << "Could not read string descriptor; error " << r << std::endl;
		}
	}
	
	virtual ~USB_device(){
		if (handle) libusb_close(handle);
	}
	
	libusb_device_handle *handle;
	char serial[32];
	
	virtual bool processMessage(ClientConn& session, string& cmd, JSONNode& n);
};
