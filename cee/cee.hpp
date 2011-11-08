#pragma once

#include "../dataserver.hpp"

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

class OutputPacketSource;

#define N_TRANSFERS 128
#define TRANSFER_SIZE 64


/// Buffer collecting and storing incoming packets
class InputPacketBuffer{
	public:
	InputPacketBuffer(): packet_count(0), buffer(0), write_end_index(0), write_start_index(0){}
	
	~InputPacketBuffer(){
		if (buffer) free(buffer);
	}
	
	/// Initialize the packet buffer to hold pcount packets (pcount*IN_SAMPLES_PER_PACKET samples)
	void init(unsigned pcount);
	
	/// Write the raw binary buffer contents (including packet headers) to the specified file
	void dumpToFile(const char* fname);
	
	/// Write the raw samples to a file as CSV (lines: a_v,a_i,b_v,b_i)
	void dumpCSV(const char* fname);
	
	
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
	inline unsigned char* writePacketStart(){
		return (unsigned char*) &buffer[write_end_index++];
	}
	
	/// Tell the buffer that a packet at ptr has been written
	inline void writePacketDone(unsigned char* ptr){
		/*if (ptr != (unsigned char*) &buffer[write_start_index]){
			cerr << "buffer fill skipped a piece "<< (unsigned *) ptr <<" "<< write_start_index <<" "<< (unsigned *) buffer << endl;
		}*/
		write_start_index += 1;
	}
	
	/// Return true if there is space in the buffer to start another write
	inline bool canStartWrite(){return write_end_index < packet_count;}
	
	/// Return true if the buffer is full, and all packets have been received
	inline bool fullyBuffered(){return write_start_index == packet_count;}
	
	unsigned packet_count;
	IN_packet *buffer;
	unsigned write_end_index; // index where newly queued writes will be placed
	unsigned write_start_index; // index that has started writing that has not yet completed
	// data between write_start_index and write_end_index is in an undefined state
	// data below write_start index is valid
};

class CEE_device: public Device{
	public: 
	CEE_device(libusb_device *dev, libusb_device_descriptor &desc);
	virtual ~CEE_device();
	
	/// Start streaming for the specified number of samples
	virtual void start_streaming(unsigned samples);
	
	/// Cancel streaming
	virtual void stop_streaming();
	
	/// Called in USB event thread
	void in_transfer_complete(libusb_transfer *t);
	void out_transfer_complete(libusb_transfer *t);

	libusb_device_handle *handle;
	unsigned char serial[32];
	libusb_transfer* in_transfers[N_TRANSFERS];
	libusb_transfer* out_transfers[N_TRANSFERS];
	int streaming;
	InputPacketBuffer in_buffer;
	OutputPacketSource* output_source;
};
