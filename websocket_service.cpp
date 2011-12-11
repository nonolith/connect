#include <iostream>
#include <boost/foreach.hpp>

#include "websocketpp.hpp"
#include "websocket_handler.hpp"

#include "dataserver.hpp"
#include "json.hpp"

struct Listener{
	Listener(const string& _id, Stream *s, unsigned df, int startSample):
		id(_id),
		stream(s),
		decimateFactor(df),
		outIndex(0)
	{
		if (startSample < 0){
			// startSample -1 means start at current position
			index = stream->buffer_max() + startSample + 1;
			if (index > decimateFactor) index-=decimateFactor;
		}else{
			index = startSample;
			if (index < decimateFactor) index = decimateFactor;
		}
	}

	const string id;
	Stream *stream;

	// stream sample index
	unsigned decimateFactor;
	unsigned index;

	unsigned outIndex;

	inline bool isDataAvailable(){
		return index < stream->buffer_i;
	}

	inline float nextSample(){
		float total=0;
		for (unsigned i = (index-decimateFactor); i <= index; i++){
			total += stream->get(i);
		}
		total /= decimateFactor;
		index += decimateFactor;
		outIndex++;
		return total;
	}

	inline void reset(){
		index = decimateFactor;
		outIndex = 0;
	}
};

typedef std::pair<const string, Listener*> listener_pair;

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
		clearAllListeners();
	}

	void selectDevice(device_ptr dev){
		clearAllListeners();

		device = dev;

		l_capture_state_changed.subscribe(
			device->captureStateChanged,
			boost::bind(&ClientConn::on_capture_state_changed, this)
		);
		l_data_received.subscribe(
			device->dataReceived,
			boost::bind(&ClientConn::on_data_received, this)
		);
		on_capture_state_changed();
		on_device_info_changed();
	}

	void listen(const string& id,
	           Stream *stream,
	           unsigned decimateFactor, int start=-1){
		cancelListen(id);
		Listener *w = new Listener(id, stream, decimateFactor, start);
		listeners.insert(listener_pair(id, w));
		on_data_received();
	}

	void cancelListen(const string& id){
		std::map<string, Listener*>::iterator it = listeners.find(id);
		if (it != listeners.end()){
			delete it->second;
			listeners.erase(it);
		}
	}

	void clearAllListeners(){
		BOOST_FOREACH(listener_pair &p, listeners){
			delete p.second;
		}
		listeners.clear();
	}

	void resetAllListeners(){
		BOOST_FOREACH(listener_pair &p, listeners){
			p.second->reset();
		}
	}

	websocketpp::session_ptr client;

	EventListener l_device_list_changed;
	EventListener l_capture_state_changed;
	EventListener l_data_received;
	std::map<string, Listener*> listeners;

	device_ptr device;

	void on_message(const std::string &msg){
		try{
			JSONNode n = libjson::parse(msg);
			string cmd = n.at("_cmd").as_string();

			if (cmd == "selectDevice"){
				string id = n.at("id").as_string();
				selectDevice(getDeviceById(id));
			}else if (cmd == "listen"){
				string id = n.at("id").as_string();
				string channel = n.at("channel").as_string();
				string streamName = n.at("stream").as_string();
				int decimateFactor = n.at("decimateFactor").as_int();
				int startSample = n.at("start").as_int();
				
				//TODO: findStream of a particular device
				Stream* stream = findStream(device->getId(), channel, streamName); 
				listen(id, stream, decimateFactor, startSample);
			}else if (cmd == "cancelListen"){
				string id = n.at("id").as_string();
				cancelListen(id);
			}else if (cmd == "prepareCapture"){
				float length = n.at("length").as_float();
				bool continuous = false;
				if (n.find("continuous") != n.end()){
					continuous = n.at("continuous").as_bool();
				}
				if (device) device->prepare_capture(length, continuous);
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
			}else{
				std::cerr << "Unknown command " << cmd << std::endl;
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
			resetAllListeners();
		}

		JSONNode n(JSON_NODE);
		n.push_back(JSONNode("_action", "captureState"));
		n.push_back(JSONNode("state", captureStateToString(device->captureState)));
		n.push_back(JSONNode("length", device->captureLength));
		n.push_back(JSONNode("continuous", device->captureContinuous));
		sendJSON(n);
	}

	void on_data_received(){
		JSONNode message(JSON_NODE);
		JSONNode listenerJSON(JSON_ARRAY);
		bool dataToSend = false;

		BOOST_FOREACH(listener_pair &p, listeners){
			Listener *w = p.second;

			if (!w->isDataAvailable()){
				continue;
			}

			dataToSend = true;
			JSONNode n(JSON_NODE);

			n.push_back(JSONNode("id", w->id));
			n.push_back(JSONNode("idx", w->outIndex));
			
			JSONNode a(JSON_ARRAY);
			a.set_name("data");
			while (w->isDataAvailable()){
				a.push_back(JSONNode("", w->nextSample()));
			}
			n.push_back(a);

			listenerJSON.push_back(n);
		}

		if (dataToSend){
			message.push_back(JSONNode("_action", "update"));
			listenerJSON.set_name("listeners");
			message.push_back(listenerJSON);
			sendJSON(message);
		}
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
