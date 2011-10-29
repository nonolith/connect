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


#define CEE_VID 0x9999
#define CEE_PID 0xffff

#define EP_BULK_IN 0x81
#define EP_BULK_OUT 0x02
#define N_TRANSFERS 128
#define TRANSFER_SIZE 64

long millis(){
	struct timeb tp;
	ftime(&tp);
	return tp.millitm;
}

void in_transfer_callback(libusb_transfer *t);
void out_transfer_callback(libusb_transfer *t);

struct IN_sample{
	signed int a_v:12;
	signed int a_i:12;
	signed int b_v:12;
	signed int b_i:12;
} __attribute__((packed));


#define IN_SAMPLES_PER_PACKET 10
struct IN_packet{
	unsigned char seqno;
	unsigned char flags;
	unsigned char reserved[2];
	IN_sample data[IN_SAMPLES_PER_PACKET];	
} __attribute__((packed));


#define OUT_SAMPLES_PER_PACKET 10
struct OUT_sample{
	unsigned a:12;
	unsigned b:12;
} __attribute__((packed));

struct OUT_packet{
	unsigned char seqno;
	unsigned char flags;
	OUT_sample data[OUT_SAMPLES_PER_PACKET];
} __attribute__((packed));


/// Buffer collecting and storing incoming packets
class InputPacketBuffer{
	public:
	InputPacketBuffer(): packet_count(0), buffer(0), write_end_index(0), write_start_index(0){}
	
	~InputPacketBuffer(){
		if (buffer) free(buffer);
	}
	
	/// Initialize the packet buffer to hold pcount packets (pcount*IN_SAMPLES_PER_PACKET samples)
	void init(unsigned pcount){
		if (buffer) free(buffer);
		packet_count = pcount;
		buffer = (IN_packet*) malloc(sizeof(IN_packet)*pcount);
	}
	
	/// Write the raw binary buffer contents (including packet headers) to the specified file
	void dumpToFile(const char* fname){
		 fstream f(fname, ios::out | ios::binary);
		 f.write((char*)buffer, sizeof(IN_packet)*packet_count);
		 f.close();
	}
	
	/// Write the raw samples to a file as CSV (lines: a_v,a_i,b_v,b_i)
	void dumpCSV(const char* fname){
		fstream f(fname, ios::out);
		for (unsigned i=0; i<current_length(); i++){
			const IN_sample* s = &at(i);
			f << s->a_v << "," << s->a_i << "," << s->b_v << "," << s->b_i << "\n";
		}
		f.close();
	}
	
	
	
	//// Sample-level interface
	
	/// Number of samples the buffer will contain when filled
	inline unsigned full_length(){
		return packet_count*IN_SAMPLES_PER_PACKET;
	}
	
	/// Number of samples currently available
	inline unsigned current_length(){
		return write_start_index*IN_SAMPLES_PER_PACKET;
	}
	
	/// Get a sample at the specified index
	inline const IN_sample& at(unsigned i){
		i = i%full_length();
		return buffer[i/IN_SAMPLES_PER_PACKET].data[i%IN_SAMPLES_PER_PACKET];
	}
	
	/// Syntactic sugar for at()
	inline const IN_sample operator[](unsigned index){
		return at(index);
	}
	
	
	
	//// Interface for the USB code:
	
	/// Get the address at which to write a newly-queued packet
	unsigned char* writePacketStart(){
		return (unsigned char*) &buffer[write_end_index++];
	}
	
	/// Tell the buffer that a packet at ptr has been written
	void writePacketDone(unsigned char* ptr){
		if (ptr != (unsigned char*) &buffer[write_start_index]){
			cerr << "buffer fill skipped a piece "<< (unsigned *) ptr <<" "<< write_start_index <<" "<< (unsigned *) buffer << endl;
		}
		write_start_index += 1;
	}
	
	/// Return true if there is space in the buffer to start another write
	bool canStartWrite(){return write_end_index < packet_count;}
	
	/// Return true if the buffer is full, and all packets have been received
	bool fullyBuffered(){return write_start_index == packet_count;}
	
	unsigned packet_count;
	IN_packet *buffer;
	unsigned write_end_index; // index where newly queued writes will be placed
	unsigned write_start_index; // index that has started writing that has not yet completed
	// data between write_start_index and write_end_index is in an undefined state
	// data below write_start index is valid
};

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
	
	/// Start streaming for the specified number of samples
	InputPacketBuffer* start_streaming(unsigned samples, OutputPacketSource* source){
		if (streaming){
			stop_streaming();
		}
		
		streaming = 1;
		output_source = source;
		
		in_buffer.init(samples/IN_SAMPLES_PER_PACKET);
		
		for (int i=0; i<N_TRANSFERS; i++){
			in_transfers[i] = libusb_alloc_transfer(0);
			unsigned char* buf = in_buffer.writePacketStart();
			libusb_fill_bulk_transfer(in_transfers[i], handle, EP_BULK_IN, buf, 64, in_transfer_callback, this, 50);
			libusb_submit_transfer(in_transfers[i]);
			
			out_transfers[i] = libusb_alloc_transfer(0);
			buf = output_source->readPacketStart();
			libusb_fill_bulk_transfer(out_transfers[i], handle, EP_BULK_OUT, buf, 32, out_transfer_callback, this, 50);
			//out_transfers[i]->flags |= LIBUSB_TRANSFER_FREE_BUFFER;
			libusb_submit_transfer(out_transfers[i]);
		}
		
		return &in_buffer;
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
				in_buffer.dumpCSV("inData.csv");
			}
		}
	}
	
	void out_transfer_complete(libusb_transfer *t){
		if (t->status == LIBUSB_TRANSFER_COMPLETED){
			output_source->readPacketDone(t->buffer);
			t->buffer = output_source->readPacketStart();
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
	InputPacketBuffer in_buffer;
	OutputPacketSource* output_source;
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
	
	OutputPacketSource_constant source(0, 0, 0);
	if (devices.size()){
		devices.front()->start_streaming(100*1000*5, &source);
	}
	
	while(1) libusb_handle_events(NULL);
	
	for (vector <CEE_device*>::iterator it=devices.begin() ; it < devices.end(); it++ ){
		delete *it;
	}

	
	libusb_exit(NULL);
	return 0;
}
