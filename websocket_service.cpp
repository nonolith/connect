#include <iostream>
#include <boost/foreach.hpp>

#include "websocketpp.hpp"
#include "websocket_handler.hpp"

#include "dataserver.hpp"
#include "json.hpp"

struct StreamWatch{
	StreamWatch(const string& _id, Stream *s, int si, int ei, unsigned df):
		id(_id),
		stream(s),
		startIndex(si),
		endIndex(ei),
		decimateFactor(df),
		outIndex(0)
	{
		if (si == -1){
			// startIndex -1 means start at current position
			index = stream->buffer_fill_point;
			if (index > decimateFactor) index-=decimateFactor;	
		}else{
			index = si;
			if (index < decimateFactor) index = decimateFactor;		
		}
	}
	string id;
	Stream *stream;

	// stream sample indexes
	int startIndex;
	int endIndex;
	unsigned decimateFactor;
	unsigned index;

	// resampled indexes
	unsigned outIndex;

	EventListener data_received_l;

	inline bool isComplete(){
		if (endIndex == -1) return false;
		return index >= (unsigned) endIndex;
	}

	inline bool isDataAvailable(){
		return index < stream->buffer_fill_point && !isComplete();
	}

	inline float nextSample(){
		float total;
		for (unsigned i = (index-decimateFactor); i <= index; i++){
			total += stream->data[i];}
		total /= decimateFactor;
		index += decimateFactor;
		outIndex++;
		return total;
	}
};

typedef std::pair<const string, StreamWatch*> watch_pair;

class ClientConn{
	public:
	ClientConn(websocketpp::session_ptr c): client(c){
		l_device_list_changed.subscribe(
			device_list_changed,
			boost::bind(&ClientConn::on_device_list_changed, this)
		);

		on_device_list_changed();
	}

	~ClientConn(){
		clearAllWatches();
	}

	void selectDevice(device_ptr dev){
		clearAllWatches();

		device = dev;

		l_capture_state_changed.subscribe(
			device->captureStateChanged,
			boost::bind(&ClientConn::on_capture_state_changed, this)
		);
		on_capture_state_changed();
		on_device_info_changed();
	}

	void watch(const string& id,
	           Stream *stream,
	           unsigned startIndex,
	           unsigned endIndex,
	           unsigned decimateFactor){
		std::map<string, StreamWatch*>::iterator it = watches.find(id);
		if (it != watches.end()){
			delete it->second;
			watches.erase(it);
		}
		StreamWatch *w = new StreamWatch(id, stream, startIndex, endIndex, decimateFactor);
		watches.insert(watch_pair(id, w));
		w->data_received_l.subscribe(
			stream->data_received,
			boost::bind(&ClientConn::on_data_received, this, w)
		);
		on_data_received(w);
	}

	void clearAllWatches(){
		BOOST_FOREACH(watch_pair &p, watches){
			delete p.second;
		}
		watches.clear();
	}

	websocketpp::session_ptr client;

	EventListener l_device_list_changed;
	EventListener l_capture_state_changed;
	std::map<string, StreamWatch*> watches;

	device_ptr device; //TODO: weak reference?

	void on_message(const std::string &msg){
//		std::cout << "RXD: " << msg <<std::endl;

		try{
			JSONNode n = libjson::parse(msg);
			string cmd = n.at("_cmd").as_string();

			if (cmd == "selectDevice"){
				string id = n.at("id").as_string();
				selectDevice(getDeviceById(id));
			}else if (cmd == "watch"){
				string id = n.at("id").as_string();
				string channel = n.at("channel").as_string();
				string streamName = n.at("stream").as_string();
				int startIndex = n.at("startIndex").as_int();
				int endIndex = n.at("endIndex").as_int();
				int decimateFactor = n.at("decimateFactor").as_int();
				
				//TODO: findStream of a particular device
				Stream* stream = findStream(device->getId(), channel, streamName); 
				watch(id, stream, startIndex, endIndex, decimateFactor);
			}else if (cmd == "prepareCapture"){
				float length = n.at("length").as_float();
				if (device) device->prepare_capture(length);
			}else if (cmd == "startCapture"){
				if (device) device->start_capture();
			}else if (cmd == "pauseCapture"){
				if (device) device->pause_capture();
			}else if (cmd == "set"){
				string cId = n.at("channel").as_string();
				Channel *channel = device->channelById(cId);
				if (!channel) return; //TODO: exception
				string source = n.at("source").as_string();
				if (source == "constant"){
					float val = n.at("value").as_float();
					unsigned mode = n.at("mode").as_int(); //TODO: validate
					device->setOutput(channel, new ConstantOutputSource(mode, val));
				}
			}
		}catch(std::exception &e){ // TODO: more helpful error message by catching different types
			std::cerr << "WS JSON error:" << e.what() << std::endl;
			return;
		}		
	}

	void on_message(const std::vector<unsigned char> &data){
		
	}

	void sendJSON(JSONNode &n){
		string jc = (string) n.write_formatted();
//		std::cout << "TXD: " << jc <<std::endl;
		client->send(jc);
	}

	void on_device_list_changed(){
		JSONNode n(JSON_NODE);
		JSONNode devices = jsonDevicesArray();
		devices.set_name("devices");

		n.push_back(JSONNode("_action", "devices"));
		n.push_back(devices);

		sendJSON(n);
	}

	void on_device_info_changed(){
		if (!device) return;

		JSONNode n(JSON_NODE);
		n.push_back(JSONNode("_action", "deviceInfo"));
	
		JSONNode d = toJSON(device, true);
		d.set_name("device");
		n.push_back(d);

		sendJSON(n);
	}

	void on_capture_state_changed(){
		if (device->captureState == CAPTURE_READY){
			clearAllWatches();
		}

		JSONNode n(JSON_NODE);
		n.push_back(JSONNode("_action", "captureState"));
		n.push_back(JSONNode("state", captureStateToString(device->captureState)));
		n.push_back(JSONNode("length", device->captureLength));

		sendJSON(n);
	}

	void on_data_received(StreamWatch* w){
		if (!w->isDataAvailable()){
			// Fast path if we haven't reached the next wanted sample yet
			return;
		}

		JSONNode n(JSON_NODE);
		n.push_back(JSONNode("_action", "update"));
		n.push_back(JSONNode("id", w->id));
		n.push_back(JSONNode("idx", w->outIndex));
		JSONNode a(JSON_ARRAY);
		a.set_name("data");
		while (w->isDataAvailable()){
			a.push_back(JSONNode("", w->nextSample()));
		}
		n.push_back(a);

		if (w->isComplete()){
			n.push_back(JSONNode("end", true));
			watches.erase(w->id);
			delete w;
		}

		sendJSON(n);
	}
};

std::map<websocketpp::session_ptr, ClientConn*> connections;

void data_server_handler::on_open(websocketpp::session_ptr client){
	connections.insert(std::pair<websocketpp::session_ptr, ClientConn*>(client, new ClientConn(client)));
}
	
void data_server_handler::on_close(websocketpp::session_ptr client){
	std::map<websocketpp::session_ptr, ClientConn*>::iterator it = connections.find(client);
	delete it->second;
	connections.erase(it);
}

void data_server_handler::on_message(websocketpp::session_ptr client,const std::string &msg){
	connections.find(client)->second->on_message(msg);
}

void data_server_handler::on_message(websocketpp::session_ptr client, const std::vector<unsigned char> &data) {
	connections.find(client)->second->on_message(data);
}
