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

	const unsigned id;
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
};
