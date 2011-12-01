#pragma once

#include "../dataserver.hpp"
#include <boost/thread/mutex.hpp>

enum CEE_chanmode{
	DISABLED = 0,
	SVMI = 1,
	SIMV = 2,
};

inline int16_t signextend12(uint16_t v){
	return (v>((1<<11)-1))?(v - (1<<12)):v;
}

struct IN_sample{
	uint8_t avl, ail, aih_avh, bvl, bil, bih_bvh;

	int16_t av(){return signextend12((aih_avh&0x0f)<<8) | avl;}
	int16_t bv(){return signextend12((bih_bvh&0x0f)<<8) | bvl;}
	int16_t ai(){return signextend12((aih_avh&0xf0)<<4) | ail;}
	int16_t bi(){return signextend12((bih_bvh&0xf0)<<4) | bil;}
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
	uint8_t al, bl, bh_ah;
	void pack(uint16_t a, uint16_t b){
		al = a & 0xff;
		bl = b & 0xff;
		bh_ah = ((b>>4)&0xf0) |	(a>>8);
	}
} __attribute__((packed));

struct OUT_packet{
	unsigned char seqno;
	unsigned char flags;
	OUT_sample data[OUT_SAMPLES_PER_PACKET];
} __attribute__((packed));

class OutputPacketSource;

#define N_TRANSFERS 128
#define TRANSFER_SIZE 64

class CEE_device: public Device{
	public: 
	CEE_device(libusb_device *dev, libusb_device_descriptor &desc);
	virtual ~CEE_device();

	virtual const string model(){return "com.nonolithlabs.cee";}
	virtual const string hwversion(){return "unknown";}
	virtual const string fwversion(){return "unknown";}
	virtual const string serialno(){return serial;}

	libusb_device_handle *handle;
	char serial[32];

	libusb_transfer* in_transfers[N_TRANSFERS];
	libusb_transfer* out_transfers[N_TRANSFERS];

	Channel channel_a;
	Channel channel_b;

	InputStream channel_a_v;
	InputStream channel_a_i;
	InputStream channel_b_v;
	InputStream channel_b_i;

	void fill_out_packet(unsigned char*);
	void handle_in_packet(unsigned char*);

	unsigned samples, incount, outcount;

	protected:
	virtual void on_prepare_capture();
	virtual void on_start_capture();
	virtual void on_pause_capture();
};
