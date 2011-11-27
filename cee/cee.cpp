// USB data streaming
// http://nonolithlabs.com/cee
// (C) 2011 Kevin Mehall / Nonolith Labs <km@kevinmehall.net>
// Released under the terms of the GNU GPLv3+

#include <stdlib.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <libusb-1.0/libusb.h>
#include <sys/timeb.h>
using namespace std;

#include "cee.hpp"

#define EP_BULK_IN 0x81
#define EP_BULK_OUT 0x02

long millis(){
	struct timeb tp;
	ftime(&tp);
	return tp.millitm;
}

void in_transfer_callback(libusb_transfer *t);
void out_transfer_callback(libusb_transfer *t);

CEE_device::CEE_device(libusb_device *dev, libusb_device_descriptor &desc):
	channel_a("a", "A"),
	channel_b("b", "B"),
	channel_a_v("av", "Voltage A", "V", "measure", 0, 0, 0),
	channel_a_i("ai", "Current A", "I", "source", 0, 0, 0),
	channel_b_v("bv", "Voltage B", "V", "measure", 0, 0, 0),
	channel_b_i("bi", "Current B", "I", "source", 0, 0, 0)
	{
	int r = libusb_open(dev, &handle);
	if (r != 0){
		cerr << "Could not open device"<<endl;
		return;
	}
	
	r = libusb_claim_interface(handle, 0);
	if (r != 0){
		cerr << "Could not claim interface"<<endl;
		return;
	}

	libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, (unsigned char *) serial, 32);
	cerr << "Found a CEE: "<< serial << endl;
}

CEE_device::~CEE_device(){
	pause_capture();
	libusb_close(handle);
}

void CEE_device::on_prepare_capture(){
	
}

void CEE_device::on_start_capture(){
	for (int i=0; i<N_TRANSFERS; i++){
		in_transfers[i] = libusb_alloc_transfer(0);
		unsigned char* buf = (unsigned char*) malloc(sizeof(IN_packet));
		libusb_fill_bulk_transfer(in_transfers[i], handle, EP_BULK_IN, buf, 64, in_transfer_callback, this, 500);
		libusb_submit_transfer(in_transfers[i]);
		
		out_transfers[i] = libusb_alloc_transfer(0);
		buf = (unsigned char *) malloc(sizeof(OUT_packet));
		libusb_fill_bulk_transfer(out_transfers[i], handle, EP_BULK_OUT, buf, 32, out_transfer_callback, this, 500);
		out_transfers[i]->flags |= LIBUSB_TRANSFER_FREE_BUFFER;
		libusb_submit_transfer(out_transfers[i]);
	}
}

void CEE_device::on_pause_capture(){
	for (int i=0; i<N_TRANSFERS; i++){
		if (in_transfers[i]){
			// zero user_data tells the callback to free on complete. this obj may be dead by then
			in_transfers[i]->user_data=0;
			
			int r=-1;
			if (in_transfers[i]->status != LIBUSB_TRANSFER_CANCELLED){
				r = libusb_cancel_transfer(in_transfers[i]);
			}
			if (r != 0){
				libusb_free_transfer(in_transfers[i]);
			}
			in_transfers[i] = 0;
		}
		
		if (out_transfers[i]){
			// zero user_data tells the callback to free on complete. this obj may be dead by then
			out_transfers[i]->user_data=0;
			int r=-1;
			if (out_transfers[i]->status != LIBUSB_TRANSFER_CANCELLED){
				r = libusb_cancel_transfer(out_transfers[i]);
			}
			if (r != 0){
				libusb_free_transfer(in_transfers[i]);
			}
			out_transfers[i] = 0;
		}
	}
}

void CEE_device::in_transfer_complete(libusb_transfer *t){
	if (t->status == LIBUSB_TRANSFER_COMPLETED){
		//cerr <<  millis() << " " << t << " complete " << t->actual_length << endl;
	}else{
		cerr << "ITransfer error "<< t->status << " " << t << endl;
	}
	
	if (1){
		libusb_submit_transfer(t);
	}else{
		// don't submit more transfers, but wait for all the transfers to complete
		t->status = LIBUSB_TRANSFER_CANCELLED;
		if (1){
			done_capture();
			cerr << "Done." << endl;
		}
	}
}

void CEE_device::out_transfer_complete(libusb_transfer *t){
	if (t->status == LIBUSB_TRANSFER_COMPLETED){
		cin.read((char*)t->buffer, sizeof(OUT_packet));
		libusb_submit_transfer(t);
		//cerr <<  millis() << " " << t << " sent " << t->actual_length << endl;
	}else{
		cerr << "OTransfer error "<< t->status << " " << t << endl;
		done_capture();
	}
}


void in_transfer_callback(libusb_transfer *t){
	if (t->user_data){
		((CEE_device *)(t->user_data))->in_transfer_complete(t);
	}else{ // user_data was zeroed out when device was deleted
		libusb_free_transfer(t);
	}
}

void out_transfer_callback(libusb_transfer *t){
	if (t->user_data){
		((CEE_device *)(t->user_data))->out_transfer_complete(t);
	}else{ // user_data was zeroed out when device was deleted
		libusb_free_transfer(t);
	}
}