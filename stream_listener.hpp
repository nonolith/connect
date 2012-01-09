#include <map>

#include "streaming_device.hpp"

struct StreamListener{
	StreamListener(ListenerId _id, ClientConn& _client, StreamingDevice *d, Stream *s, unsigned df, int startSample, int _count=-1):
		id(_id),
		client(&_client),
		device(d),
		stream(s),
		decimateFactor(df),
		outIndex(0),
		count(_count)
	{
		if (startSample < 0){
			// startSample -1 means start at current position
			int i = (int)(d->buffer_max()) + startSample + 1;
			if (i < 0) index = 0;
			else       index = i;
		}else{
			index = startSample;
		}
	}

	const ListenerId id;
	ClientConn* client;
	StreamingDevice *device;
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
		for (unsigned i = (index-decimateFactor+1); i <= index; i++){
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
	
	bool handleNewData(){
		bool keep = true;
		if (isDataAvailable()){
			JSONNode message(JSON_NODE);
			JSONNode listenerJSON(JSON_ARRAY);

			JSONNode n(JSON_NODE);

			n.push_back(JSONNode("id", id.second));
			n.push_back(JSONNode("idx", outIndex));
			n.push_back(JSONNode("sampleIndex", index));
		
			JSONNode a(JSON_ARRAY);
			a.set_name("data");
			while (isDataAvailable()){
				a.push_back(JSONNode("", nextSample()));
			}
			n.push_back(a);
		
			if (isComplete()){
				n.push_back(JSONNode("done", true));
				keep = false;
			}

			listenerJSON.push_back(n);
			message.push_back(JSONNode("_action", "update"));
			listenerJSON.set_name("listeners");
			message.push_back(listenerJSON);
			client->sendJSON(message);
		}
		return keep;
	}
};
