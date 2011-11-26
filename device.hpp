#pragma once
#ifndef INCLUDE_FROM_DATASERVER_HPP
#error "device.hpp should not be included directly"
#endif

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

struct Channel;
struct InputStream;
struct OutputSource;

enum CaptureState{
	CAPTURE_INACTIVE,
	CAPTURE_READY,
	CAPTURE_ACTIVE,
	CAPTURE_PAUSED,
	CAPTURE_DONE,
};

string captureStateToString(CaptureState s);

class Device: public boost::enable_shared_from_this<Device> {
	public: 
		Device(): captureState(CAPTURE_INACTIVE), captureLength(0) {}
		virtual ~Device(){};
		
		/// Allocate resources to capture the specified number of seconds of data
		void prepare_capture(float seconds);
		void start_capture();
		void pause_capture();

		virtual const string getId(){return model()+"~"+serialno();}
		virtual const string serialno(){return "0";}
		virtual const string model() = 0;
		virtual const string hwversion(){return "unknown";}
		virtual const string fwversion(){return "unknown";}

		Channel* channelById(const std::string&);

		Event captureStateChanged;
		CaptureState captureState;
		float captureLength;

		std::vector<Channel*> channels;

	protected:
		virtual void on_prepare_capture() = 0;
		virtual void on_start_capture() = 0;
		virtual void on_pause_capture() = 0;
		void done_capture();
};

typedef boost::shared_ptr<Device> device_ptr;

struct Channel{
	Channel(const string _id, const string _dn):
		id(_id), displayName(_dn){}
	const string id;
	const string displayName;

	InputStream* inputById(const std::string&);
	
	std::vector<InputStream*> inputs;
	OutputSource *output_source;
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

device_ptr getDeviceById(string id);
InputStream* findStream(const string& deviceId, const string& channelId, const string& streamId);
