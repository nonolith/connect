#pragma once

#include "../dataserver.hpp"
#include "../streaming_device.hpp"
#include "../usb_device.hpp"
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
#define FLAG_PACKET_DROPPED (1<<0)

typedef struct IN_packet{
	uint8_t mode_a;
	uint8_t mode_b;
	uint8_t flags;
	uint8_t mode_seq;
	IN_sample data[10];	
} __attribute__((packed)) IN_packet;

#define OUT_SAMPLES_PER_PACKET 10
struct OUT_sample{
	uint8_t al, bl, bh_ah;
	void pack(uint16_t a, uint16_t b){
		al = a & 0xff;
		bl = b & 0xff;
		bh_ah = ((b>>4)&0xf0) |	(a>>8);
	}
} __attribute__((packed));

typedef struct OUT_packet{
	uint8_t mode_a;
	uint8_t mode_b;
	OUT_sample data[10];
} __attribute__((packed)) OUT_packet;

class OutputPacketSource;

#define N_TRANSFERS 128
#define TRANSFER_SIZE 64

class CEE_device: public StreamingDevice, USB_device{
	public: 
	CEE_device(libusb_device *dev, libusb_device_descriptor &desc);
	virtual ~CEE_device();
	
	virtual void configure(int mode, float sampleTime, unsigned samples, bool continuous, bool raw);

	virtual const string model(){return "com.nonolithlabs.cee";}
	virtual const string hwVersion(){return _hwversion;}
	virtual const string fwVersion(){return _fwversion;}
	virtual const string serialno(){return serial;}
	
	virtual bool processMessage(ClientConn& session, string& cmd, JSONNode& n);
	
	virtual void setOutput(Channel* channel, OutputSource* source);
	virtual void setGain(Channel* channel, Stream* stream, int gain);

	libusb_transfer* in_transfers[N_TRANSFERS];
	libusb_transfer* out_transfers[N_TRANSFERS];

	Channel channel_a;
	Channel channel_b;

	Stream channel_a_v;
	Stream channel_a_i;
	Stream channel_b_v;
	Stream channel_b_i;

	boost::mutex outputMutex;
	boost::mutex transfersMutex;
	void fill_out_packet(unsigned char*);
	void handle_in_packet(unsigned char*);

	/// count of IN and OUT packets, owned by USB thread
	unsigned incount, outcount;

	protected:
	string _hwversion, _fwversion;
	virtual void on_reset_capture();
	virtual void on_start_capture();
	virtual void on_pause_capture();
	uint16_t encode_out(CEE_chanmode mode, float val);
	void checkOutputEffective(Channel& channel);
};
