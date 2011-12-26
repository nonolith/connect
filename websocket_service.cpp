#include <iostream>
#include <boost/foreach.hpp>

#include "websocketpp.hpp"
#include "websocket_handler.hpp"

#include "dataserver.hpp"
#include "json.hpp"

struct Listener{
	Listener(const string& _id, device_ptr d, Stream *s, unsigned df, int startSample, int _count=-1):
		id(_id),
		device(d),
		stream(s),
		decimateFactor(df),
		outIndex(0),
		count(_count)
	{
		unsigned m = d->buffer_min() + decimateFactor;
		if (startSample < 0){
			// startSample -1 means start at current position
			int i = (int)(d->buffer_max()) + startSample + 1;
			if (i > (int) m + (int) decimateFactor) 
				index = i - decimateFactor;
			else if (i > (int) m)
				index = i;
			else
				index = m;
		}else{
			index = startSample;
			if (index < m)
				index = m;
		}
	}

	const string id;
	device_ptr device;
	Stream *stream;

	// stream sample index
	unsigned decimateFactor;
	unsigned index;

	unsigned outIndex;
	int count;

	inline bool isComplete(){
		return count>0 && (int) outIndex >= count;
	}

	inline bool isDataAvailable(){
		return index < device->capture_i && !isComplete();
	}

	inline float nextSample(){
		float total=0;
		for (unsigned i = (index-decimateFactor); i <= index; i++){
			total += device->get(*stream, i);
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

class ClientConn: public DeviceEventListener{
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
		
		_setDevice(dev);
	}

	void listen(const string& id,
	           Stream *stream,
	           unsigned decimateFactor, int start=-1, int count=-1){
		cancelListen(id);
		Listener *w = new Listener(id, device, stream, decimateFactor, start, count);
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

	void on_message(const std::string &msg){
		if (debugFlag){
			std::cout << "RXD: " << msg << std::endl;
		}
		try{
			JSONNode n = libjson::parse(msg);
			string cmd = n.at("_cmd").as_string();

			if (cmd == "selectDevice"){
				string id = n.at("id").as_string();
				selectDevice(getDeviceById(id));
				return;
			}
			
			if (!device){
				std::cerr<<"selectDevice before using other WS calls"<<std::endl;
				return;
			}
				
			if (cmd == "listen"){
				string id = n.at("id").as_string();
				string channel = n.at("channel").as_string();
				string streamName = n.at("stream").as_string();
				int decimateFactor = n.at("decimateFactor").as_int();
				
				int startSample = -1;
				if (n.find("start") != n.end()) startSample = n.at("start").as_int();
				
				int count = -1;
				if (n.find("count") != n.end()) count = n.at("count").as_int();
				
				Stream* stream = device->findStream(channel, streamName); 
				listen(id, stream, decimateFactor, startSample, count);
				
			}else if (cmd == "cancelListen"){
				string id = n.at("id").as_string();
				cancelListen(id);
				
			}else if (cmd == "configure"){
				int mode = n.at("mode").as_int();
				unsigned samples = n.at("samples").as_int();
				float sampleTime = n.at("sampleTime").as_float();
				
				bool continuous = false;
				if (n.find("continuous") != n.end()){
					continuous = n.at("continuous").as_bool();
				}
				
				bool raw = false;
				if (n.find("raw") != n.end()){
					raw = n.at("raw").as_bool();
				}
				device->configure(mode, sampleTime, samples, continuous, raw);
				
			}else if (cmd == "startCapture"){
				device->start_capture();
				
			}else if (cmd == "pauseCapture"){
				device->pause_capture();
				
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
				
			}else if (cmd == "controlTransfer"){
				//TODO: this is device-specific. Should it go elsewhere?
				string id = n.at("id").as_string();
				uint8_t bmRequestType = n.at("bmRequestType").as_int();
				uint8_t bRequest = n.at("bRequest").as_int();
				uint16_t wValue = n.at("wValue").as_int();
				uint16_t wIndex = n.at("wIndex").as_int();
				uint16_t wLength = n.at("wLength").as_int();
				
				uint8_t data[wLength];
				
				bool isIn = bmRequestType & 0x80;
				
				if (!isIn){
					//TODO: also handle array input
					string datastr = n.at("data").as_string();
					datastr.copy((char*)data, wLength);
				}
				
				int ret = device->controlTransfer(bmRequestType, bRequest, wValue, wIndex, data, wLength);
				
				JSONNode reply(JSON_NODE);
				reply.push_back(JSONNode("_action", "controlTransferReturn"));
				reply.push_back(JSONNode("status", ret));
				reply.push_back(JSONNode("id", id));
				
				if (isIn && ret>=0){
					JSONNode data_arr(JSON_ARRAY);
					for (int i=0; i<ret && i<wLength; i++){
						data_arr.push_back(JSONNode("", data[i]));
					}
					data_arr.set_name("data");
					reply.push_back(data_arr);
				}
				
				sendJSON(reply);
				
				
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
		string jc = (string) n.write();
		if (debugFlag){
			std::cout << "TXD: " << jc <<std::endl;
		}
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

	void on_config(){
		if (!device) return;

		JSONNode n(JSON_NODE);
		n.push_back(JSONNode("_action", "deviceConfig"));
	
		JSONNode d = toJSON(device, true);
		d.set_name("device");
		n.push_back(d);

		sendJSON(n);
	}
	
	void on_capture_reset(){
		resetAllListeners();
		JSONNode n(JSON_NODE);
		n.push_back(JSONNode("_action", "captureReset"));
		sendJSON(n);
	}

	void on_capture_state_changed(){
		JSONNode n(JSON_NODE);
		n.push_back(JSONNode("_action", "captureState"));
		n.push_back(JSONNode("state", device->captureState));
		n.push_back(JSONNode("done", device->captureDone));
		sendJSON(n);
	}

	void on_data_received(){
		JSONNode message(JSON_NODE);
		JSONNode listenerJSON(JSON_ARRAY);
		bool dataToSend = false;

		std::map<string, Listener*>::iterator it;
		for (it=listeners.begin(); it!=listeners.end();){
			// Increment before (potentially) deleting the watch, as that invalidates the iterator
			std::map<string, Listener*>::iterator currentIt = it++;
			Listener* w = currentIt->second;

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
			
			if (w->isComplete()){
				n.push_back(JSONNode("done", true));
				listeners.erase(currentIt);
			}

			listenerJSON.push_back(n);
		}

		if (dataToSend){
			message.push_back(JSONNode("_action", "update"));
			listenerJSON.set_name("listeners");
			message.push_back(listenerJSON);
			sendJSON(message);
		}
	}
	
	void on_output_changed(Channel *channel, OutputSource *source){
		JSONNode n(JSON_NODE);
		n.push_back(JSONNode("_action", "outputChanged"));
		n.push_back(JSONNode("channel", channel->id));
		n.push_back(JSONNode("mode", source->mode));
		n.push_back(JSONNode("description", source->displayName()));
		n.push_back(JSONNode("startSample", source->startSample));
		n.push_back(JSONNode("valueTarget", source->valueTarget()));
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
