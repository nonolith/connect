#include <map>
#include <vector>

#include "streaming_device.hpp"

struct StreamListener{
	StreamListener(ListenerId _id, ClientConn* _client, StreamingDevice *d,
		std::vector<Stream*> _streams, unsigned df, int startSample, int _count=-1):
		
		id(_id),
		client(_client),
		device(d),
		streams(_streams),
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
	StreamingDevice* device;
	const std::vector<Stream*> streams;

	// stream sample index
	unsigned decimateFactor;
	unsigned index;

	unsigned outIndex;
	int count;

	inline bool isComplete(){
		return count>0 && (int) outIndex >= count;
	}

	inline bool isDataAvailable(){
		return index + decimateFactor < device->capture_i && !isComplete();
	}

	inline void reset(){
		index = decimateFactor;
		outIndex = 0;
	}
	
	bool handleNewData();
};

StreamListener *makeStreamListener(StreamingDevice* dev, ClientConn* client, JSONNode &n);
