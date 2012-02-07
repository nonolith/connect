// Nonolith Connect
// https://github.com/nonolith/connect
// Streaming device base class
// Released under the terms of the GNU GPLv3+
// (C) 2012 Nonolith Labs, LLC
// Authors:
//   Kevin Mehall <km@kevinmehall.net>

#pragma once

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <set>
#include <map>

#include "dataserver.hpp"


typedef std::pair<ClientConn*, unsigned> ListenerId;
struct StreamListener;
typedef std::map<ListenerId, StreamListener*> listener_map_t;

struct Channel;
struct Stream;
struct OutputSource;

struct Stream{
	Stream(const string _id, const string _dn, const string _units, float _min, float _max, unsigned _outputMode=0, float _uncertainty=0):
		id(_id),
		displayName(_dn),
		units(_units),
		min(_min),
		max(_max),
		
		outputMode(_outputMode),
		gain(1),
		uncertainty(_uncertainty),
		data(0){};

	~Stream(){
		if (data){
			free(data);
		}
	}
	
	JSONNode toJSON();

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
	
	// Gain factor
	unsigned gain;
	
	float uncertainty;

	/// Raw data buffer
	float* data;
};


class StreamingDevice: public Device{
	public: 
		StreamingDevice(float _sampleTime):
			captureState(false),
			captureDone(false),
			captureLength(0),
			captureSamples(0),
			captureContinuous(false),
			sampleTime(_sampleTime),
			capture_i(0),
			capture_o(0) {}
		virtual ~StreamingDevice(){clearAllListeners();}
		
		virtual JSONNode stateToJSON();
		
		virtual void onClientAttach(ClientConn *c);
		virtual void onClientDetach(ClientConn *c);
		virtual bool processMessage(ClientConn& session, string& cmd, JSONNode& n);
		
		listener_map_t listeners;
		virtual void addListener(StreamListener *l);
		virtual void cancelListen(ListenerId lid);
		virtual void clearAllListeners();
		virtual void resetAllListeners();
		
		void handleData();
		
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

		virtual void setOutput(Channel* channel, OutputSource* source);
		virtual void setGain(Channel* channel, Stream* stream, int gain){}

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

	protected:
		void notifyCaptureState();
		void notifyConfig();
		void notifyCaptureReset();
		void notifyDisconnect();
		void notifyOutputChanged(Channel *channel, OutputSource *outputSource);
		void notifyGainChanged(Channel* channel, Stream* stream, int gain);
		void done_capture();
		void handleNewData();

		virtual void on_reset_capture() = 0;
		virtual void on_start_capture() = 0;
		virtual void on_pause_capture() = 0;
};

struct Channel{
	Channel(const string _id, const string _dn):
		id(_id), displayName(_dn), source(0){}
		
	JSONNode toJSON();
	
	const string id;
	const string displayName;

	Stream* streamById(const std::string&);
	
	std::vector<Stream*> streams;
	OutputSource *source;
};


struct OutputSource{
	virtual string displayName() = 0;
	
	virtual float getValue(unsigned sample, float sampleTime) = 0;
	
	virtual void describeJSON(JSONNode &n);

	const unsigned mode;
	
	/// The output sample number at which this source was added
	unsigned startSample;
	
	/// true if this source's effect has come back as input
	bool effective;
	
	virtual void initialize(unsigned sample, OutputSource* prevSrc){};

	protected:
		OutputSource(unsigned m): mode(m), startSample(0), effective(false){};
};

OutputSource *makeConstantSource(unsigned m, int value);

Stream* findStream(const string& deviceId, const string& channelId, const string& streamId);
