#pragma once

#include "../dataserver.hpp"
#include "../usb_device.hpp"

#define REQ_INFO 0xB0
#define REQ_ERASE 0xB1
#define REQ_START_WRITE 0xB2
#define REQ_CRC_APP 0xB3
#define REQ_CRC_BOOT 0xB4
#define REQ_RESET 0xBF

struct BootloaderInfo{
	uint32_t magic;
	uint8_t version;
	uint32_t devid;
	uint16_t page_size;  // Page size in bytes
	uint32_t app_section_end; // Byte address of end of flash. Add one for flash size
	uint32_t entry_jmp_pointer; // App code can jump to this pointer to enter the bootloader
	char hw_product[16];
	char hw_version[16];
} __attribute__((packed));

class Bootloader_device: public Device, USB_device{
	public: 
	Bootloader_device(libusb_device *dev, libusb_device_descriptor &desc);
	
	virtual const string model(){return "com.nonolithlabs.bootloader";}
	virtual const string hwversion(){return "unknown";}
	virtual const string fwversion(){return "unknown";}
	virtual const string serialno(){return serial;}
	
	virtual bool processMessage(ClientConn& session, string& cmd, JSONNode& n);
	virtual void onClientAttach(ClientConn *c);
	
	BootloaderInfo info;
	
	void getInfo();
	void erase();
	int write(string data);
	unsigned crc_app();
	unsigned crc_boot();
	void reset();
};
