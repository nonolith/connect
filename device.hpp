#pragma once
#ifndef INCLUDE_FROM_DATASERVER_HPP
#error "device.hpp should not be included directly"
#endif

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

		virtual const string getId(){return model()+"~"+serialno();}
		virtual const string serialno(){return "0";}
		virtual const string model() = 0;
		virtual const string hwversion(){return "unknown";}
		virtual const string fwversion(){return "unknown";}

		Channel* channelById(const std::string&);

		std::vector<Channel*> channels;
};

typedef boost::shared_ptr<Device> device_ptr;

struct Channel{
	Channel(const string _id, const string _dn):
		id(_id), displayName(_dn){}
	const string id;
	const string displayName;

	InputStream* inputById(const std::string&);
	OutputStream* outputById(const std::string&);
	
	std::vector<InputStream*> inputs;
	std::vector<OutputStream*> outputs;
};

struct InputStream{
	InputStream(const string _id, const string _dn, const string _units, const string startState, int idx=0):
		id(_id),
		displayName(_dn),
		units(_units),
		state(startState),
		data(0),
		buffer_size(0),
		buffer_fill_point(0){};

	~InputStream(){
		if (data){
			free(data);
		}
	}

	const string id;
	const string displayName;
	const string units;

	string state;

	/// Raw data buffer
	uint16_t* data;

	/// Allocated elements of *data
	unsigned buffer_size;

	/// data[i] for i<buffer_fill_point is valid
	unsigned buffer_fill_point;

	/// Allocate space for /size/ samples
	void allocate(unsigned size);

	/// Store a sample and increment buffer_fill_point
	/// Note: when you are done putting samples, call data_received.notify()
	/// This is not called automatically, because it only needs to be called once
	/// if multiple samples are put at the same time.
	void put(uint16_t p);
	
	/// unit data = raw*scale + offset
	float scale, offset;
	float sample_time;

	/// Event fires after data has been put
	Event data_received;
};

struct OutputStream{
	OutputStream(const string _id):
		id(_id){};
	const string id;

	OutputSource *source;
};

InputStream* findStream(const string& deviceId, const string& channelId, const string& streamId);

void startStreaming();
void stopStreaming();