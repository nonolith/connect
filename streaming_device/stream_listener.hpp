// Nonolith Connect
// https://github.com/nonolith/connect
// Listeners - streaming data requests
// Released under the terms of the GNU GPLv3+
// (C) 2012 Nonolith Labs, LLC
// Authors:
//   Kevin Mehall <km@kevinmehall.net>

#include <map>
#include <vector>

#include "streaming_device.hpp"

struct StreamListener{
	unsigned id;
	virtual bool isFromClient(ClientConn* c){return false;}
	
	StreamingDevice* device;
	std::vector<Stream*> streams;

	// stream sample index
	unsigned decimateFactor;
	unsigned index;

	unsigned outIndex;
	int count;
	
	bool triggerMode;
	bool triggered;
	bool triggerRepeat;
	float triggerLevel;
	Stream* triggerStream;
	int triggerHoldoff;
	int triggerOffset;
	unsigned triggerForce;
	unsigned triggerForceIndex;

	inline void reset(){
		index = 0;
		outIndex = 0;
	}
	
	unsigned howManySamples();
	
	// return true if listener is to be kept, false if it is to be destroyed
	virtual bool handleNewData(){return false;}
	
	// return true if trigger was found
	bool findTrigger();
};

struct WSStreamListener: public StreamListener{
	ClientConn* client;
	virtual bool isFromClient(ClientConn* c){return c == client;}
	virtual bool handleNewData();
};

listener_ptr makeStreamListener(StreamingDevice* dev, ClientConn* client, JSONNode &n);
