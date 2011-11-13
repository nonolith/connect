#pragma once
#include <set>
#include <vector>
#include <string>
using std::string;

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

struct Channel;
struct InputStream;
struct OutputStream;
struct OutputSource;

class Device: public boost::enable_shared_from_this<Device> {
	public: 
		virtual ~Device(){};
		
		/// Start streaming for the specified number of samples
		virtual void start_streaming(unsigned samples) = 0;
		
		/// Cancel streaming
		virtual void stop_streaming() = 0;

		virtual const string serialno(){return "0";}
		virtual const string model() = 0;
		virtual const string hwversion(){return "unknown";}
		virtual const string fwversion(){return "unknown";}

		std::vector<Channel*> channels;
};

typedef boost::shared_ptr<Device> device_ptr;

struct Channel{
	Channel(const string _id, const string _dn):
		id(_id), displayName(_dn){}
	const string id;
	const string displayName;

	std::vector<InputStream*> inputs;
	std::vector<OutputStream*> outputs;
};

struct InputStream{
	InputStream(const string _id, const string _dn, const string _units, const string startState, int idx=0):
		id(_id),
		displayName(_dn),
		units(_units),
		state(startState),
		index(idx){};
	const string id;
	const string displayName;
	const string units;

	string state;
	int index;
};

struct OutputStream{
	OutputStream(const string _id):
		id(_id){};
	const string id;

	OutputSource *source;
};

device_ptr getDeviceBySerial(string id);

void usb_init();
void usb_scan_devices();
void usb_thread_main();

extern std::set<device_ptr> devices;
