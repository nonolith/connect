#include <iostream>
#include <libusb-1.0/libusb.h>
#include <vector>
#include <set>
#include <boost/thread.hpp>

#include "dataserver.hpp"
#include "cee/cee.hpp"

using namespace std;

std::vector <device_ptr> devices;
set <libusb_device *> active_libusb_devices;

boost::thread* usb_thread;

#define CEE_VID 0x9999
#define CEE_PID 0xffff

void usb_init(){
	int r = libusb_init(NULL);
	if (r < 0){
		cerr << "Could not init libusb" << endl;
		abort();
	}

	usb_thread = new boost::thread(usb_thread_main);
}

// Dedicated thread for handling soft-real-time USB events
void usb_thread_main(){
	while(1) libusb_handle_events(NULL);
}

void usb_fini(){
	// TODO: kill the USB thread somehow
	usb_thread->join(); //currently blocks forever

	devices.empty();
	
	libusb_exit(NULL);
}

void usb_scan_devices(){
	cerr << "Scanning" << endl;	libusb_device **devs;
	
	ssize_t cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0){
		cerr << "Error in get_device_list" << endl;
	}
	
	for (ssize_t i=0; i<cnt; i++){
		if (active_libusb_devices.find(devs[i]) != active_libusb_devices.end()){
			continue; // Ignore device, as it's already in use
		}

		libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(devs[i], &desc);
		if (r<0){
			cerr << "Error in get_device_descriptor" << endl;
			continue;
		}
		if (desc.idVendor == CEE_VID && desc.idProduct == CEE_PID){
			devices.push_back(device_ptr(new CEE_device(devs[i], desc)));

			cerr << "Found a device" << std::endl;

			// Add the device to the active list so we don't re-add it
			// This is safe because the CEE_device holds a reference. Because known
			// devices are at these addresses, they are guaranteed not to be allocated
			// to another device object.
			active_libusb_devices.insert(devs[i]);
		}
	}
	libusb_free_device_list(devs, 1);
}

