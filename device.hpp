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
	string units;

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
		
		
		/// Configure the device for the specified *mode* and allocate resources
		/// to capture the specified number of samples and sample rate
		/// If *continuous*, capture indefinitely, keeping the specified number
		/// of samples of history. If *raw*, units will be device LSB rather
		/// than converting to standard units.
		virtual void configure(int mode, float sampleTime, unsigned samples, bool continuous, bool raw)=0;
		
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

		unsigned devMode;
		
		bool rawMode;
		
		
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
		
		/// Find a stream by its channel id and stream id
		Stream* findStream(const string& channelId, const string& streamId);
		
		virtual int controlTransfer(uint8_t bmRequestType,
		                            uint8_t bRequest,
		                            uint16_t wValue,
		                            uint16_t wIndex,
		                            uint8_t* data,
		                            uint16_t wLength){return -128;};
		
		virtual void onDisconnect(){ notifyDisconnect(); }

	protected:
		virtual void on_reset_capture() = 0;
		virtual void on_start_capture() = 0;
		virtual void on_pause_capture() = 0;
		
		void notifyCaptureState();
		void notifyConfig();
		void notifyCaptureReset();
		void notifyDisconnect();
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
	
	virtual float getValue(unsigned sample, float sampleTime) = 0;
	
	virtual void describeJSON(JSONNode &n) = 0;

	const unsigned mode;
	
	/// The output sample number at which this source was added
	unsigned startSample;
	
	/// true if this source's effect has come back as input
	bool effective;

	protected:
		OutputSource(unsigned m): mode(m), effective(false){};
};

OutputSource *makeConstantSource(unsigned m, int value);

struct DeviceEventListener{
	device_ptr device;
	
	virtual void on_config(){};
	virtual void on_capture_reset(){};
	virtual void on_capture_state_changed(){};
	virtual void on_device_info_changed(){};
	virtual void on_data_received() {};
	virtual void on_output_changed(Channel *channel, OutputSource *outputSource) {};
	virtual void on_disconnect(){};
	
	inline void _setDevice(device_ptr &dev){
		if (device){
			device->removeEventListener(this);
		}
		device = dev;
		device->addEventListener(this);
		on_config();
		on_capture_state_changed();
	}
	
	virtual ~DeviceEventListener(){
		if (device) device->removeEventListener(this);
	}
};

device_ptr getDeviceById(string id);
Stream* findStream(const string& deviceId, const string& channelId, const string& streamId);
