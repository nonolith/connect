#pragma once
#include <set>
#include <vector>
#include <string>
using std::string;

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

class OutputChannel;
class InputChannel;
class OutputSource;

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

		std::vector<InputChannel*> input_channels;
		std::vector<OutputChannel*> output_channels;
};

struct InputChannel{
	InputChannel(const string _id, const string _dn, const string _units, const string startState):
		id(_id),
		displayName(_dn),
		units(_units),
		state(startState){};
	const string id;
	const string displayName;
	const string units;
	string state;

	std::vector<OutputChannel*> relatedOutputs;
};

struct OutputChannel{
	OutputChannel(const string _id):
		id(_id){};
	const string id;

	std::vector<InputChannel*> relatedInputs;

	OutputSource *source;
};


typedef boost::shared_ptr<Device> device_ptr;

void usb_init();
void usb_scan_devices();
void usb_thread_main();

extern std::set<device_ptr> devices;
