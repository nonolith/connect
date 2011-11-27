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

class CEE_device: public Device{
	public: 
	CEE_device(libusb_device *dev, libusb_device_descriptor &desc);
	virtual ~CEE_device();

	virtual const string model(){return "com.nonolithlabs.cee";}
	virtual const string hwversion(){return "unknown";}
	virtual const string fwversion(){return "unknown";}
	virtual const string serialno(){return serial;}
	
	/// Called in USB event thread
	void in_transfer_complete(libusb_transfer *t);
	void out_transfer_complete(libusb_transfer *t);

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

	protected:
	virtual void on_prepare_capture();
	virtual void on_start_capture();
	virtual void on_pause_capture();
};
