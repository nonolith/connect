// USB data streaming
// http://nonolithlabs.com/cee
// (C) 2011 Kevin Mehall / Nonolith Labs <km@kevinmehall.net>
// Released under the terms of the GNU GPLv3+

#include <stdlib.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <libusb/libusb.h>
#include <sys/timeb.h>
#include <boost/bind.hpp>
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

const int CEE_sample_per = 160;
const int CEE_timer_clock = 4e6; // 4 MHz
const float CEE_sample_time = CEE_sample_per / (float) CEE_timer_clock; // 40us
const float CEE_I_gain = 45*.07;


CEE_device::CEE_device(libusb_device *dev, libusb_device_descriptor &desc):
	channel_a("a", "A"),
	channel_b("b", "B"),
	channel_a_v("av", "Voltage A", "V", 0.0, 5.0, "measure", CEE_sample_time),
	channel_a_i("ai", "Current A", "mA", -200, 200, "source",  CEE_sample_time),
	channel_b_v("bv", "Voltage B", "V", 0.0, 5.0, "measure", CEE_sample_time),
	channel_b_i("bi", "Current B", "mA", -200, 200, "source",  CEE_sample_time)
	{

	channels.push_back(&channel_a);
	channel_a.streams.push_back(&channel_a_v);
	channel_a.streams.push_back(&channel_a_i);
	channels.push_back(&channel_b);
	channel_b.streams.push_back(&channel_b_v);
	channel_b.streams.push_back(&channel_b_i);

	channel_a.source = new ConstantOutputSource(0, 0);
	channel_b.source = new ConstantOutputSource(0, 0);

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
	samples = ceil(captureLength/CEE_sample_time);
	std::cerr << "CEE prepare "<< samples <<" " << captureLength<<"/"<<CEE_sample_time<< std::endl;
	incount = outcount = 0;
	channel_a_v.allocate(samples);
	channel_a_i.allocate(samples);
	channel_b_v.allocate(samples);
	channel_b_i.allocate(samples);
}

void CEE_device::on_start_capture(){
	for (int i=0; i<N_TRANSFERS; i++){
		in_transfers[i] = libusb_alloc_transfer(0);
		unsigned char* buf = (unsigned char*) malloc(sizeof(IN_packet));
		libusb_fill_bulk_transfer(in_transfers[i], handle, EP_BULK_IN, buf, 64, in_transfer_callback, this, 5000);
		in_transfers[i]->flags |= LIBUSB_TRANSFER_FREE_BUFFER;
		libusb_submit_transfer(in_transfers[i]);
		
		out_transfers[i] = libusb_alloc_transfer(0);
		buf = (unsigned char *) malloc(sizeof(OUT_packet));
		fill_out_packet(buf);
		outcount++;
		libusb_fill_bulk_transfer(out_transfers[i], handle, EP_BULK_OUT, buf, 32, out_transfer_callback, this, 5000);
		out_transfers[i]->flags |= LIBUSB_TRANSFER_FREE_BUFFER;
		libusb_submit_transfer(out_transfers[i]);
	}
}

void CEE_device::on_pause_capture(){
	std::cerr << "on_pause_capture" <<std::endl;

	libusb_lock_events(NULL);
	for (int i=0; i<N_TRANSFERS; i++){
		if (in_transfers[i]){
			// zero user_data tells the callback to free on complete. this obj may be dead by then
			in_transfers[i]->user_data=0;

			libusb_cancel_transfer(in_transfers[i]);
			in_transfers[i] = 0;
		}
		
		if (out_transfers[i]){
			// zero user_data tells the callback to free on complete. this obj may be dead by then
			out_transfers[i]->user_data=0;
			
			libusb_cancel_transfer(out_transfers[i]);
			out_transfers[i] = 0;
		}
	}
	libusb_unlock_events(NULL);
}

void CEE_device::handle_in_packet(unsigned char *buffer){
	IN_packet *pkt = (IN_packet*) buffer;
	for (int i=0; i<10; i++){
		channel_a_v.put(pkt->data[i].av()*5.0/2048.0);
		channel_a_i.put(pkt->data[i].ai()*2.5/2048.0/CEE_I_gain*1000.0);
		channel_b_v.put(pkt->data[i].bv()*5.0/2048.0);
		channel_b_i.put(pkt->data[i].bi()*2.5/2048.0/CEE_I_gain*1000.0);
	}

	free(buffer);

	dataReceived.notify();
	
	if (channel_a_v.buffer_fill_point == samples){
		done_capture();
	}
}


uint16_t encode_out(CEE_chanmode mode, float val){
	if (mode == SVMI){
		return 4095*val/5.0;
	}else if (mode == SIMV){
		return 4095*(1.25+CEE_I_gain*val/1000.0)/2.5;
	}else return 0;
}

void CEE_device::fill_out_packet(unsigned char* buf){
	if (channel_a.source && channel_b.source){
		unsigned mode_a = channel_a.source->mode;
		unsigned mode_b = channel_a.source->mode;

		OUT_packet *pkt = (OUT_packet *)buf;

		pkt->flags = (mode_a<<4) | mode_b;

		for (int i=0; i<10; i++){
			pkt->data[i].pack(
				encode_out((CEE_chanmode)mode_a, channel_a.source->nextValue(outcount/CEE_sample_time)),
				encode_out((CEE_chanmode)mode_b, channel_b.source->nextValue(outcount/CEE_sample_time))
			);
		}	
	}else{
		memset(buf, 0, 32);
	}
	
}

void destroy_transfer(CEE_device *dev, libusb_transfer** list, libusb_transfer* t){
	t->user_data = 0;
	for (int i=0; i<N_TRANSFERS; i++){
		if (list[i] == t){
			list[i] = 0;
		}
	}
	//cerr << "Freeing lpacket "<< t << " " << t->status << endl;
	libusb_free_transfer(t);
}


/// Runs in USB thread
void in_transfer_callback(libusb_transfer *t){
	if (!t->user_data){
		//cerr << "Freeing in packet "<< t << " " << t->status << endl;
		libusb_free_transfer(t); // user_data was zeroed out when device was deleted
		return;
	}

	CEE_device *dev = (CEE_device *) t->user_data;

	if (t->status == LIBUSB_TRANSFER_COMPLETED){
		//cerr <<  millis() << " " << t << " complete " << t->actual_length << endl;
		io.post(boost::bind(&CEE_device::handle_in_packet, dev, t->buffer));
		t->buffer = (unsigned char*) malloc(sizeof(IN_packet));

		if (dev->incount < dev->samples){
			dev->incount++;
			libusb_submit_transfer(t);
		}else{
			// don't submit more transfers, but wait for all the transfers to complete
			cerr << "Queued last in packet" << endl;
			destroy_transfer(dev, dev->in_transfers, t);
		}
	}else{
		cerr << "ITransfer error "<< t->status << " " << t << endl;
		//TODO: notify main thread of error
		destroy_transfer(dev, dev->in_transfers, t);
	}
}


/// Runs in USB thread
void out_transfer_callback(libusb_transfer *t){
	if (!t->user_data){
		//cerr << "Freeing out packet "<< t << " " << t->status << endl;
		libusb_free_transfer(t); // user_data was zeroed out when device was deleted
		return;
	}

	CEE_device *dev = (CEE_device *) t->user_data;

	if (t->status == LIBUSB_TRANSFER_COMPLETED){
		if (dev->outcount < dev->samples){
			dev->fill_out_packet(t->buffer);
			dev->outcount++;
			libusb_submit_transfer(t);
		}
		//cerr << outcount << " " << millis() << " " << t << " sent " << t->actual_length << endl;
	}else{
		cerr << "OTransfer error "<< t->status << " " << t << endl;
		destroy_transfer(dev, dev->out_transfers, t);
	}
}
