#pragma once
#ifndef INCLUDE_FROM_DATASERVER_HPP
#error "device.hpp should not be included directly"
#endif

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

struct Channel;
struct Stream;
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
		/// If continuoys, capture indefinitely, keeping seconds seconds of history.
		/// Puts the device into captureState CAPTURE_READY
		void prepare_capture(float seconds, bool continuous);

		/// Start a prepared or paused capture
		void start_capture();

		/// Pause capturing
		void pause_capture();

		virtual const string getId(){return model()+"~"+serialno();}
		virtual const string serialno(){return "0";}
		virtual const string model() = 0;
		virtual const string hwVersion(){return "unknown";}
		virtual const string fwVersion(){return "unknown";}

		virtual void setOutput(Channel* channel, OutputSource* source);

		Channel* channelById(const std::string&);

		Event dataReceived;

		Event captureStateChanged;
		CaptureState captureState;
		float captureLength;
		bool captureContinuous;

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
		id(_id), displayName(_dn), source(0){}
	const string id;
	const string displayName;

	Stream* streamById(const std::string&);
	
	std::vector<Stream*> streams;
	OutputSource *source;
};

struct Stream{
	Stream(const string _id, const string _dn, const string _units, float _min, float _max,
				const string startState, float _sampleTime, unsigned _outputMode=0):
		id(_id),
		displayName(_dn),
		units(_units),
		min(_min),
		max(_max),
		state(startState),
		buffer_size(0),
		buffer_i(0),
		sampleTime(_sampleTime),
		outputMode(_outputMode),
		data(0){};

	~Stream(){
		if (data){
			free(data);
		}
	}

	const string id;
	const string displayName;
	const string units;

	float min, max;

	string state;

	/// Allocated elements of *data
	unsigned buffer_size;

	/// monotonically increasing sample counter
	/// index of next-written element is buffer_i%buffer_size
	unsigned buffer_i; //TODO: what if it wraps (11 hours)

	// True if configured for continuous (ring buffer) sampling
	bool continuous;

	/// Allocate space for /size/ samples
	void allocate(unsigned size, bool continuous);

	/// Store a sample and increment buffer_fill_point
	/// Note: when you are done putting samples, call data_received.notify()
	/// This is not called automatically, because it only needs to be called once
	/// if multiple samples are put at the same time.
	inline void put(float p){
		if (!data || !buffer_size || (buffer_i>=buffer_size && !continuous)) return;
		data[buffer_i++ % buffer_size]=p;
	}

	/// Get the sample corresponding to buffer_i==i. If it is not in
	/// memory (either overwritten or not yet collected), returns NaN. 
	inline float get(unsigned i){
		if (   !data || !buffer_size   // not prepared
		    || i>=buffer_i             // not yet collected
		    || (buffer_i>buffer_size && i<=buffer_i-buffer_size)) // overwritten
			return NAN;
		else
			return data[i%buffer_size];
	}

	/// Returns the lowest buffer index currently available
	inline unsigned buffer_min(){
		if (buffer_i < buffer_size)
			return 0;
		else
			return buffer_i - buffer_size;
	}

	/// Returns the highest buffer index currently available
	inline unsigned buffer_max(){
		return buffer_i;
	}
	
	float sampleTime;

	/// mode for output that "sources" this stream's variable
	/// 0 if outputting this variable is not supported.
	unsigned outputMode;

protected:
	/// Raw data buffer
	float* data;
};

struct OutputSource{
	virtual string displayName() = 0;
	virtual float nextValue(float time) = 0;

	const unsigned mode;

	protected:
		OutputSource(unsigned m): mode(m){};
};

struct ConstantOutputSource: public OutputSource{
	ConstantOutputSource(unsigned m, float val): OutputSource(m), value(val){}
	virtual string displayName(){return "Constant";};
	virtual float nextValue(float time){
		return value;
	}
	float value;
};

device_ptr getDeviceById(string id);
Stream* findStream(const string& deviceId, const string& channelId, const string& streamId);
