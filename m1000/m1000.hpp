// Nonolith Connect
// https://github.com/nonolith/connect
// USB Data streaming for CEE
// Released under the terms of the GNU GPLv3+
// (C) 2012 Nonolith Labs, LLC
// Authors:
//   Kevin Mehall <km@kevinmehall.net>

#pragma once

#include "../dataserver.hpp"
#include "../streaming_device/streaming_device.hpp"
#include "../usb_device.hpp"
#include <boost/thread/mutex.hpp>


#define N_TRANSFERS 64

class M1000_device: public StreamingDevice, USB_device{
	public:
	M1000_device(libusb_device *dev, libusb_device_descriptor &desc);
	virtual ~M1000_device();

	virtual void configure(int mode, double sampleTime, unsigned samples, bool continuous, bool raw);

	virtual const string model(){return "com.nonolithlabs.cee";}
	virtual const string hwVersion(){return _hwversion;}
	virtual const string fwVersion(){
		return "9999";
	}
	virtual const string serialno(){return serial;}

	virtual bool processMessage(ClientConn& session, string& cmd, JSONNode& n);
	JSONNode gpio(bool set, uint8_t dir=0, uint8_t out=0);
	virtual void setOutput(Channel* channel, OutputSource* source);

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
	void fillOutTransfer(unsigned char*);
	void handleInTransfer(unsigned char*);

	/// count of IN and OUT packets, owned by USB thread
	unsigned incount, outcount;

	bool firstPacket;

	int ntransfers, packets_per_transfer;

	protected:
	string _hwversion, _fwversion, _gitversion;
	virtual void on_reset_capture();
	virtual void on_start_capture();
	virtual void on_pause_capture();
	uint16_t encode_out(Chanmode mode, float val, uint32_t igain);
	void checkOutputEffective(Channel& channel);

	void readCalibration();
};
