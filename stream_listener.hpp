#include <map>
#include <vector>

#include "streaming_device.hpp"

struct StreamListener{
	ListenerId id;
	ClientConn* client;
	StreamingDevice* device;
	std::vector<Stream*> streams;

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
		index = 0;
		outIndex = 0;
	}
	
	bool handleNewData();
};

StreamListener *makeStreamListener(StreamingDevice* dev, ClientConn* client, JSONNode &n);
