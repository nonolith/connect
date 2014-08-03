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
#include <vector>

#include "../dataserver.hpp"

enum Chanmode{
	DISABLED = 0,
	SVMI = 1,
	SIMV = 2,
};

struct StreamListener;
typedef boost::shared_ptr<StreamListener> listener_ptr;
typedef std::set<listener_ptr> listener_set_t;

struct Channel;
struct Stream;
struct OutputSource;

struct Stream{
	Stream(const string _id, const string _dn, const string _units, float _min, float _max, unsigned _outputMode=0, float _uncertainty=0, unsigned _gain=1):
		id(_id),
		displayName(_dn),
		units(_units),
		min(_min),
		max(_max),
		
		outputMode(_outputMode),
		gain(_gain),
		normalGain(_gain),
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
	
	// Internal device gain factor
	unsigned gain;
	
	// Default gain factor
	unsigned normalGain;
	
	double getGain(){return gain / (double) normalGain;}
	
	float uncertainty;

	/// Raw data buffer
	float* data;
};


class StreamingDevice: public Device{
	public: 
		StreamingDevice(double _sampleTime):
			captureState(false),
			captureDone(false),
			captureLength(0),
			captureSamples(0),
			captureContinuous(false),
			sampleTime(_sampleTime),
			capture_i(0),
			capture_o(0) {}
		
		virtual JSONNode stateToJSON(bool configOnly=false);
		
		virtual void onClientAttach(ClientConn *c);
		virtual void onClientDetach(ClientConn *c);
		virtual bool processMessage(ClientConn& session, string& cmd, JSONNode& n);
		virtual bool handleREST(UrlPath path, websocketpp::session_ptr client);
		
		listener_set_t listeners;
		virtual void addListener(listener_ptr l);
		virtual void cancelListen(listener_ptr l);
		virtual listener_ptr findListener(ClientConn* c, unsigned id);
		virtual void clearAllListeners();
		virtual void resetAllListeners();
		
		void handleData();
		
		/// Configure the device for the specified *mode* and allocate resources
		/// to capture the specified number of samples and sample rate
		/// If *continuous*, capture indefinitely, keeping the specified number
		/// of samples of history. If *raw*, units will be device LSB rather
		/// than converting to standard units.
		virtual void configure(int mode, double sampleTime, unsigned samples, bool continuous, bool raw)=0;
		
		/// Set time = 0
		void reset_capture();

		/// Start capturing
		void start_capture();

		/// Pause capturing
		void pause_capture();

		virtual void setOutput(Channel* channel, OutputSource* source);
		virtual void setInternalGain(Channel* channel, Stream* stream, int gain){}
		
		virtual void setGain(Channel* c, Stream* s, double gain){
			setInternalGain(c, s, round(gain * s->normalGain));
		}

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
		double sampleTime;

		/// Minimum allowed sampleTime
		double minSampleTime;
		
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
		
		
		inline float resample(Stream& s, unsigned start, unsigned count){
			if (   !s.data || !captureSamples   // not prepared
				|| start+count > capture_i             // not yet collected
				|| (capture_i>captureSamples && start<=capture_i-captureSamples)) // overwritten
				return NAN;
			float total = 0;
			for (unsigned i=0; i<count; i++){
				total += s.data[(start+i)%captureSamples];
			}
			return total/count;
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
		
		/// TODO: this doesn't really go here (cee-specific)
		int currentLimit;
		virtual void setCurrentLimit(unsigned limit){}
		
		void packetDone();
		
		/// Find a stream by its channel id and stream id
		Stream* findStream(const string& channelId, const string& streamId);
		
		virtual void onDisconnect();

	protected:
		void notifyCaptureState();
		void notifyConfig();
		void notifyCaptureReset();
		void notifyOutputChanged(Channel *channel, OutputSource *outputSource);
		void notifyGainChanged(Channel* channel, Stream* stream, int gain);
		void done_capture();
		void handleNewData();
		
		void RESTOutputRespond(websocketpp::session_ptr client, Channel *channel);
		void handleRESTOutputCallback(websocketpp::session_ptr client, Channel* channel, string postdata);
		bool handleRESTOutput(UrlPath path, websocketpp::session_ptr client, Channel* channel);
		bool handleRESTInput(UrlPath path, websocketpp::session_ptr client, Channel* channel);
		void handleRESTInputPOSTCallback(websocketpp::session_ptr client, Channel* channel, string postdata);
		void handleRESTDeviceCallback(websocketpp::session_ptr client, string postdata);
		void RESTDeviceRespond(websocketpp::session_ptr client);
		void handleRESTConfigurationCallback(websocketpp::session_ptr client, string postdata);
		void RESTConfigurationRespond(websocketpp::session_ptr client);
		
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
	
	virtual float getValue(unsigned sample, double sampleTime) = 0;
	
	virtual void describeJSON(JSONNode &n);

	const unsigned mode;
	
	/// The output sample number at which this source was added
	unsigned startSample;
	
	/// true if this source's effect has come back as input
	bool effective;

	/// hint passed by the client, not used but repeated back
	string hint;
	
	virtual void initialize(unsigned sample, OutputSource* prevSrc){};
	
	virtual double getPhaseZeroAfterSample(unsigned sample){return INFINITY;}

	virtual ~OutputSource(){};

	protected:
		OutputSource(unsigned m): mode(m), startSample(0), effective(false){};
};

struct ArbWavePoint{
	unsigned t;
	float v;
	ArbWavePoint(unsigned t_, float v_): t(t_), v(v_){};
};
typedef std::vector<ArbWavePoint> ArbWavePoint_vec;

OutputSource *makeConstantSource(unsigned m, float value);
OutputSource *makeSource(JSONNode& description);
OutputSource* makeSource(unsigned mode, const string& source, float offset, float amplitude, double period, double phase, bool relPhase);
OutputSource* makeAdvSquare(unsigned mode, float high, float low, unsigned highSamples, unsigned lowSamples, unsigned phase, bool relPhase);
OutputSource* makeArbitraryWaveform(unsigned mode, int offset, ArbWavePoint_vec& values, int repeat_count);

Stream* findStream(const string& deviceId, const string& channelId, const string& streamId);
