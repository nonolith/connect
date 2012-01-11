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
	
	bool triggerMode;
	bool triggered;
	float triggerLevel;
	Stream* triggerStream;
	int triggerHoldoff;

	inline void reset(){
		index = 0;
		outIndex = 0;
	}
	
	// return true if listener is to be kept, false if it is to be destroyed
	bool handleNewData();
	
	// return true if trigger was found
	bool findTrigger();
	
};

StreamListener *makeStreamListener(StreamingDevice* dev, ClientConn* client, JSONNode &n);
