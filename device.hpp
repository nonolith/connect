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
		
		/// Allocate resources to capture the specified number of seconds of data
		virtual void prepare_capture(float seconds) = 0;

		virtual void start_capture() = 0;
		virtual void pause_capture() = 0;

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
	InputStream(const string _id, const string _dn, const string _units, const string startState,
		        float _scale, float _offset, float _sampleTime):
		id(_id),
		displayName(_dn),
		units(_units),
		state(startState),
		data(0),
		buffer_size(0),
		buffer_fill_point(0),
		scale(_scale),
		offset(_offset),
		sampleTime(_sampleTime){};

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
	float sampleTime;

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

enum CaptureState{
	CAPTURE_INACTIVE,
	CAPTURE_READY,
	CAPTURE_ACTIVE,
	CAPTURE_PAUSED,
	CAPTURE_DONE,
};

string captureStateToString(CaptureState s);

extern CaptureState captureState;
extern float captureLength;

void prepareCapture(float seconds);
void startCapture();
void pauseCapture();