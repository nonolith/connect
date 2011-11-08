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
	
/// Initialize the packet buffer to hold pcount packets (pcount*IN_SAMPLES_PER_PACKET samples)
void InputPacketBuffer::init(unsigned pcount){
	if (buffer) free(buffer);
	packet_count = pcount;
	buffer = (IN_packet*) malloc(sizeof(IN_packet)*pcount);
}

/// Write the raw binary buffer contents (including packet headers) to the specified file
void InputPacketBuffer::dumpToFile(const char* fname){
	 fstream f(fname, ios::out | ios::binary);
	 f.write((char*)buffer, sizeof(IN_packet)*packet_count);
	 f.close();
}

/// Write the raw samples to a file as CSV (lines: a_v,a_i,b_v,b_i)
void InputPacketBuffer::dumpCSV(const char* fname){
	fstream f(fname, ios::out);
	for (unsigned i=0; i<current_length(); i++){
		const IN_sample* s = &at(i);
		f << s->a_v << "," << s->a_i << "," << s->b_v << "," << s->b_i << "\n";
	}
	f.close();
}
	
/// Stream source of output packets
class OutputPacketSource{
	public:
	
	/// Get the next packet to send to the device
	virtual unsigned char* readPacketStart() = 0;
	
	/// Notify that the passed packet has been sent and is no longer in use
	virtual void readPacketDone(unsigned char* packet){};
};

/// OutputPacketSource for periodic waveforms by repeating a buffer
class OutputPacketSource_buffer: public OutputPacketSource{
	public:
	OutputPacketSource_buffer(): buffer(0), packet_count(0), index(0){}
	
	virtual unsigned char* readPacketStart(){
		OUT_packet* p = &buffer[index];
		index = (index+1)%packet_count;
		return (unsigned char*) p;
	}
	
	OUT_packet* buffer;
	unsigned packet_count;
	unsigned index;
};


/// OutputPacketSource that sends the same sample repeatedly
class OutputPacketSource_constant: public OutputPacketSource{
	public:
	OutputPacketSource_constant(unsigned a, unsigned b, unsigned char flags){
		for (int i=0; i<OUT_SAMPLES_PER_PACKET; i++){
			packet.data[i].a=a;
			packet.data[i].b=b;
		}
		packet.flags = flags;
	}
	
	virtual unsigned char* readPacketStart(){
		return (unsigned char*) &packet;
	}
	
	OUT_packet packet;
};


CEE_device::CEE_device(libusb_device *dev, libusb_device_descriptor &desc){
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

	libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, serial, 32);
	cerr << "Found a CEE: "<< serial << endl;
	
	streaming = 0;
}

CEE_device::~CEE_device(){
	stop_streaming();
	libusb_close(handle);
}

/// Start streaming for the specified number of samples
void CEE_device::start_streaming(unsigned samples){
	if (streaming){
		stop_streaming();
	}
	
	streaming = 1;
	
	in_buffer.init(samples/IN_SAMPLES_PER_PACKET);
	
	for (int i=0; i<N_TRANSFERS; i++){
		in_transfers[i] = libusb_alloc_transfer(0);
		unsigned char* buf = in_buffer.writePacketStart();
		libusb_fill_bulk_transfer(in_transfers[i], handle, EP_BULK_IN, buf, 64, in_transfer_callback, this, 500);
		libusb_submit_transfer(in_transfers[i]);
		
		out_transfers[i] = libusb_alloc_transfer(0);
		buf = (unsigned char *) malloc(sizeof(OUT_packet));
		libusb_fill_bulk_transfer(out_transfers[i], handle, EP_BULK_OUT, buf, 32, out_transfer_callback, this, 500);
		out_transfers[i]->flags |= LIBUSB_TRANSFER_FREE_BUFFER;
		libusb_submit_transfer(out_transfers[i]);
	}
}

void CEE_device::stop_streaming(){
	if (!streaming) return;
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
	streaming = 0;
}

void CEE_device::in_transfer_complete(libusb_transfer *t){
	if (t->status == LIBUSB_TRANSFER_COMPLETED){
		in_buffer.writePacketDone(t->buffer);
		cout.write((const char*) t->buffer, sizeof(IN_packet));
		//cerr <<  millis() << " " << t << " complete " << t->actual_length << endl;
	}else{
		cerr << "ITransfer error "<< t->status << " " << t << endl;
	}
	
	if (in_buffer.canStartWrite()){
		t->buffer = in_buffer.writePacketStart();
		libusb_submit_transfer(t);
	}else{
		// don't submit more transfers, but wait for all the transfers to complete
		t->status = LIBUSB_TRANSFER_CANCELLED;
		if (in_buffer.fullyBuffered()){
			stop_streaming();
			cerr << "Done." << endl;
			in_buffer.dumpToFile("inData.bin");
			in_buffer.dumpCSV("inData.csv");
		}
	}
}

void CEE_device::out_transfer_complete(libusb_transfer *t){
	if (t->status == LIBUSB_TRANSFER_COMPLETED){
		output_source->readPacketDone(t->buffer);
		//t->buffer = output_source->readPacketStart();
		cin.read((char*)t->buffer, sizeof(OUT_packet));
		libusb_submit_transfer(t);
		//cerr <<  millis() << " " << t << " sent " << t->actual_length << endl;
	}else{
		cerr << "OTransfer error "<< t->status << " " << t << endl;
		stop_streaming();
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
