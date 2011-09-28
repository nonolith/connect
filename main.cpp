// USB data streaming
// http://nonolithlabs.com/cee
// (C) 2011 Kevin Mehall / Nonolith Labs <km@kevinmehall.net>
// Released under the terms of the GNU GPLv3+

#include <iostream>
#include <vector>
#include <fstream>
#include <libusb-1.0/libusb.h>
#include <sys/timeb.h>
using namespace std;


#define CEE_VID 0x9999
#define CEE_PID 0xffff

#define EP_BULK_IN 0x81
#define EP_BULK_OUT 0x02
#define N_TRANSFERS 32
#define TRANSFER_SIZE 64

long millis(){
	struct timeb tp;
	ftime(&tp);
	return tp.millitm;
}

void in_transfer_callback(libusb_transfer *t);
void out_transfer_callback(libusb_transfer *t);


struct IN_sample{
	unsigned a_v:12;
	unsigned a_i:12;
	unsigned b_v:12;
	unsigned b_i:12;
} __attribute__((packed));

struct IN_packet{
	char header[4];
	IN_sample data[10];	
} __attribute__((packed));

class PacketBuffer{
	public:
	PacketBuffer(): count(0), buffer(0), write_end_index(0), write_start_index(0){}
	
	void init(unsigned _elem_size, unsigned packet_count){
		if (buffer) free(buffer);
		count = packet_count;
		buffer = (IN_packet*) malloc(sizeof(IN_packet)*count);
	}
	
	~PacketBuffer(){
		if (buffer) free(buffer);
	}
	
	unsigned startIndex(){return 0;}
	unsigned endIndex(){return count-1;}
	
	unsigned char* writePacketStart(){
		return (unsigned char*) &buffer[write_end_index++];
	}
	
	void writePacketDone(unsigned char* ptr){
		if (ptr != (unsigned char*) &buffer[write_start_index]){
			cerr << "buffer fill skipped a piece "<< (unsigned *) ptr <<" "<< write_start_index <<" "<< (unsigned *) buffer << endl;
		}
		write_start_index += 1;
	}
	
	void dumpToFile(const char* fname){
		 fstream f(fname, ios::out | ios::binary);
		 f.write((char*)buffer, sizeof(IN_packet)*count);
		 f.close();
	}
	
	
	bool canStartWrite(){return write_end_index < count;}
	bool fullyBuffered(){return write_start_index == count;}
		
	unsigned count;
	IN_packet *buffer;
	unsigned write_end_index; // index where newly queued writes will be placed
	unsigned write_start_index; // index that has started writing that has not yet completed
	// data between write_start_index and write_end_index is in an undefined state
	// data below write_start index is valid
};


class CEE_device{
	public: 
	CEE_device(libusb_device *dev, libusb_device_descriptor &desc){
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
		cout << "Found a CEE: "<< serial << endl;
		
		streaming = 0;
	}
	
	~CEE_device(){
		stop_streaming();
		libusb_close(handle);
	}
	
	void start_streaming(){
		if (streaming) return;
		streaming = 1;
		
		in_buffer.init(6, 10*1000*60);
		
		for (int i=0; i<N_TRANSFERS; i++){
			in_transfers[i] = libusb_alloc_transfer(0);
			auto buf = in_buffer.writePacketStart();
			libusb_fill_bulk_transfer(in_transfers[i], handle, EP_BULK_IN, buf, 64, in_transfer_callback, this, 50);
			libusb_submit_transfer(in_transfers[i]);
			
			out_transfers[i] = libusb_alloc_transfer(0);
			buf = (unsigned char *) malloc(TRANSFER_SIZE);
			libusb_fill_bulk_transfer(out_transfers[i], handle, EP_BULK_OUT, buf, 30, out_transfer_callback, this, 50);
			out_transfers[i]->flags |= LIBUSB_TRANSFER_FREE_BUFFER;
			libusb_submit_transfer(out_transfers[i]);
		}
	}
	
	void stop_streaming(){
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
	
	void in_transfer_complete(libusb_transfer *t){
		if (t->status == LIBUSB_TRANSFER_COMPLETED){
			in_buffer.writePacketDone(t->buffer);
			//cout <<  millis() << " " << t << " complete " << t->actual_length << endl;
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
				cout << "Done." << endl;
				in_buffer.dumpToFile("inData.bin");
			}
		}
	}
	
	void out_transfer_complete(libusb_transfer *t){
		if (t->status == LIBUSB_TRANSFER_COMPLETED){
			libusb_submit_transfer(t);
			//cout <<  millis() << " " << t << " sent " << t->actual_length << endl;
		}else{
			cerr << "OTransfer error "<< t->status << " " << t << endl;
			stop_streaming();
		}
	}

	libusb_device_handle *handle;
	unsigned char serial[32];
	libusb_transfer* in_transfers[N_TRANSFERS];
	libusb_transfer* out_transfers[N_TRANSFERS];
	int streaming;
	PacketBuffer in_buffer;
};

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

vector <CEE_device*> devices;

void scan_bus(){
	libusb_device **devs;
	
	ssize_t cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0){
		cerr << "Error in get_device_list" << endl;
	}
	
	for (ssize_t i=0; i<cnt; i++){
		libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(devs[i], &desc);
		if (r<0){
			cerr << "Error in get_device_descriptor" << endl;
			continue;
		}
		if (desc.idVendor == CEE_VID && desc.idProduct == CEE_PID){
			devices.push_back(new CEE_device(devs[i], desc));
		}
	}
	
	libusb_free_device_list(devs, 1);
}


int main(){
	int r;
	
	r = libusb_init(NULL);
	if (r < 0){
		cerr << "Could not init libusb" << endl;
		return 1;
	}
	
	libusb_set_debug(NULL, 3);

	scan_bus();

	cout << devices.size() << " devices found" << endl;
	
	for (auto it=devices.begin() ; it < devices.end(); it++ ){
		(*it) -> start_streaming();
	}
	
	while(1) libusb_handle_events(NULL);
	
	for (auto it=devices.begin() ; it < devices.end(); it++ ){
		delete *it;
	}

	
	libusb_exit(NULL);
	return 0;
}
