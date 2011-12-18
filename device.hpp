#pragma once
#ifndef INCLUDE_FROM_DATASERVER_HPP
#error "device.hpp should not be included directly"
#endif

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <set>

struct Channel;
struct Stream;
struct OutputSource;
struct DeviceEventListener;

struct Stream{
	Stream(const string _id, const string _dn, const string _units, float _min, float _max, unsigned _outputMode=0):
		id(_id),
		displayName(_dn),
		units(_units),
		min(_min),
		max(_max),
		
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

	/// Allocate space for /size/ samples
	bool allocate(unsigned size);

	/// mode for output that "sources" this stream's variable
	/// 0 if outputting this variable is not supported.
	unsigned outputMode;

	/// Raw data buffer
	float* data;
};


class Device: public boost::enable_shared_from_this<Device> {
	public: 
		Device(float _sampleTime):
			captureState(false),
			captureDone(false),
			captureLength(0),
			captureSamples(0),
			captureContinuous(false),
			sampleTime(_sampleTime),
			capture_i(0),
			capture_o(0) {}
		virtual ~Device(){};
		
		std::set<DeviceEventListener*> listeners;
		
		void addEventListener(DeviceEventListener *l){
			listeners.insert(l);
		}
		
		void removeEventListener(DeviceEventListener *l){
			listeners.erase(l);
		}
		
		
		/// Allocate resources to capture the specified number of seconds of data
		/// If continuous, capture indefinitely, keeping seconds seconds of history.
		void prepare_capture(float seconds, bool continuous);
		
		/// Set time = 0
		void reset_capture();

		/// Start capturing
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

		/// True if capturing
		bool captureState;
		
		/// True if the capture is completed
		bool captureDone;
		
		float captureLength;
		
		/// Number of samples in current capture
		/// Allocated size (elements) of stream.data
		unsigned captureSamples;
		
		/// True if configured for continuous (ring buffer) sampling
		bool captureContinuous;
		
		/// Time of a sample
		float sampleTime;
		
		/// IN sample counter
	    /// index of next-written element is capture_i%captureSamples
		unsigned capture_i; //TODO: what if it wraps (11 hours)
		
		/// OUT sample counter
		unsigned capture_o;

		std::vector<Channel*> channels;
		
		/// Store a sample to a stream
		/// Note: when you are done putting samples, call sampleDone();
		inline void put(Stream& s, float p){
			if (!s.data || !captureSamples || (capture_i>=captureSamples && !captureContinuous)) return;
			s.data[capture_i % captureSamples]=p;
		}

		/// Get the sample corresponding to buffer_i==i. If it is not in
		/// memory (either overwritten or not yet collected), returns NaN. 
		inline float get(Stream& s, unsigned i){
			if (   !s.data || !captureSamples   // not prepared
				|| i>=capture_i             // not yet collected
				|| (capture_i>captureSamples && i<=capture_i-captureSamples)) // overwritten
				return NAN;
			else
				return s.data[i%captureSamples];
		}

		/// Returns the lowest buffer index currently available
		inline unsigned buffer_min(){
			if (capture_i < captureSamples)
				return 0;
			else
				return capture_i - captureSamples;
		}

		/// Returns the highest buffer index currently available
		inline unsigned buffer_max(){
			return capture_i;
		}
		
		inline void sampleDone(){
			capture_i++;
		}
		
		void packetDone();

	protected:
		virtual void on_prepare_capture() = 0;
		virtual void on_reset_capture() = 0;
		virtual void on_start_capture() = 0;
		virtual void on_pause_capture() = 0;
		
		void notifyCaptureState();
		void notifyCaptureConfig();
		void notifyCaptureReset();
		void notifyOutputChanged(Channel *channel, OutputSource *outputSource);
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


struct OutputSource{
	virtual string displayName() = 0;
	virtual float valueTarget(){ return NAN; }
	
	virtual float getValue(unsigned sample, float sampleTime) = 0;

	const unsigned mode;
	
	/// The output sample number at which this source was added
	unsigned startSample;

	protected:
		OutputSource(unsigned m): mode(m){};
};

struct ConstantOutputSource: public OutputSource{
	ConstantOutputSource(unsigned m, float val): OutputSource(m), value(val){}
	virtual string displayName(){return "Constant";}
	virtual float getValue(unsigned sample, float sampleTime){ return value; }
	virtual float valueTarget(){ return value; }
	float value;
};

struct DeviceEventListener{
	device_ptr device;
	
	virtual void on_capture_config(){};
	virtual void on_capture_reset(){};
	virtual void on_capture_state_changed(){};
	virtual void on_device_info_changed(){};
	virtual void on_data_received() {};
	virtual void on_output_changed(Channel *channel, OutputSource *outputSource) {};
	
	inline void _setDevice(device_ptr &dev){
		device = dev;
		device->addEventListener(this);
		on_device_info_changed();
		on_capture_state_changed();
	}
	
	virtual ~DeviceEventListener(){
		if (device) device->removeEventListener(this);
	}
};

device_ptr getDeviceById(string id);
Stream* findStream(const string& deviceId, const string& channelId, const string& streamId);
