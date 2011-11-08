#include <iostream>
#include <libusb-1.0/libusb.h>
#include <vector>
#include <map>
#include <boost/thread.hpp>

#include "dataserver.hpp"
#include "cee/cee.hpp"

using namespace std;

set <device_ptr> devices;
map <libusb_device *, device_ptr> active_libusb_devices;

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

	// Copy the active device list. Remove devices from this copy that are still present,
	// so it can be compared to detect removed devices.
	map <libusb_device *, device_ptr> prev_devices = active_libusb_devices;
	map <libusb_device *, device_ptr>::iterator it;

	for (ssize_t i=0; i<cnt; i++){
		it = prev_devices.find(devs[i]);
		if (it != prev_devices.end()){
			prev_devices.erase(it);
			continue; // Ignore device, as it's already in use
		}

		libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(devs[i], &desc);
		if (r<0){
			cerr << "Error in get_device_descriptor" << endl;
			continue;
		}
		if (desc.idVendor == CEE_VID && desc.idProduct == CEE_PID){
			device_ptr p = device_ptr(new CEE_device(devs[i], desc));
			devices.insert(p);

			// Add the device to the active list so we don't re-add it
			// This is safe because the Device holds a reference. Because known
			// devices are at these addresses, they are guaranteed not to be allocated
			// to another device object.
			active_libusb_devices.insert(pair<libusb_device *, device_ptr>(devs[i], p));
		}
	}

	for (it=prev_devices.begin(); it != prev_devices.end(); ++it){
		cerr << "Device removed" << endl;
		// Iterate over devices that were not seen again and remove them
		active_libusb_devices.erase(it->first);
		devices.erase(it->second);
	}
	libusb_free_device_list(devs, 1);
}

