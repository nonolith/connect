#pragma once
#include <set>
#include <vector>
#include <string>
using std::string;

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <boost/foreach.hpp>

extern boost::asio::io_service io;

struct Channel;
struct InputStream;
struct OutputStream;
struct OutputSource;

typedef boost::function<void()> void_function;

class Event;

class EventListener{
	public:
		EventListener(): event(0){}
		EventListener(Event& e, void_function h){subscribe(e, h);}
		~EventListener(){unsubscribe();}

		void subscribe(Event& e, void_function h);
		void unsubscribe();

		Event* event;
		void_function handler;
	private:
		EventListener(const EventListener&);
   		EventListener& operator=(const EventListener&);
};

class Event{
	public:
		Event(){}
		~Event();
		void notify();
		std::set<EventListener*> listeners;
	private:
		Event(const Event&);
   		Event& operator=(const Event&);
};

extern Event device_list_changed;
extern Event streaming_state_changed;

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

device_ptr getDeviceBySerial(string id);

void usb_init();
void usb_scan_devices();
void usb_thread_main();

extern std::set<device_ptr> devices;