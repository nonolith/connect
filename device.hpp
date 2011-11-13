#pragma once
#ifndef INCLUDE_FROM_DATASERVER_HPP
#error "device.hpp should not be included directly"
#endif

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
		state(startState){};

	~InputStream(){
		free(data);
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

	void allocate(unsigned size){
		buffer_size = size;
		buffer_fill_point = 0;
		if (data){
			free(data);
		}
		data = (uint16_t*) malloc(buffer_size*sizeof(uint16_t));
	}

	void put(uint16_t p){
		if (buffer_fill_point < buffer_size){
			data[buffer_fill_point++]=p;
		}
	}
	
	/// unit data = raw*scale + offset
	float scale, offset;
	float sample_time;

	Event data_received;
};

struct OutputStream{
	OutputStream(const string _id):
		id(_id){};
	const string id;

	OutputSource *source;
};