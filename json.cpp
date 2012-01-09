#include "json.hpp"
#include <boost/foreach.hpp>

#include "streaming_device.hpp"

JSONNode Stream::toJSON(){
	Stream *s = this;
	JSONNode n(JSON_NODE);
	n.set_name(s->id);
	n.push_back(JSONNode("id", s->id));
	n.push_back(JSONNode("displayName", s->displayName));
	n.push_back(JSONNode("units", s->units));
	n.push_back(JSONNode("min", s->min));
	n.push_back(JSONNode("max", s->max));
	n.push_back(JSONNode("outputMode", s->outputMode));
	return n;
}

JSONNode Channel::toJSON(){
	Channel *channel = this;
	JSONNode n(JSON_NODE);
	n.set_name(channel->id);
	n.push_back(JSONNode("id", channel->id));
	n.push_back(JSONNode("displayName", channel->displayName));
	
	if (source){
		JSONNode output(JSON_NODE);
		output.set_name("output");
		source->describeJSON(output);
		n.push_back(output);
	}

	JSONNode streams(JSON_NODE);
	streams.set_name("streams");
	BOOST_FOREACH (Stream* i, channel->streams){
		streams.push_back(i->toJSON());
	}
	n.push_back(streams);

	return n;
}

JSONNode Device::toJSON(){
	Device* d = this;
	JSONNode n(JSON_NODE);
	n.set_name(d->getId());
	n.push_back(JSONNode("id", getId()));
	n.push_back(JSONNode("model", model()));
	n.push_back(JSONNode("hwVersion", hwVersion()));
	n.push_back(JSONNode("fwVersion", fwVersion()));
	n.push_back(JSONNode("serial", serialno()));
	return n;
}

JSONNode StreamingDevice::stateToJSON(){
	JSONNode n = toJSON();
	n.push_back(JSONNode("sampleTime", sampleTime));
	n.push_back(JSONNode("mode", devMode));
	n.push_back(JSONNode("samples", captureSamples));
	n.push_back(JSONNode("length", captureLength));
	n.push_back(JSONNode("continuous", captureContinuous));
	n.push_back(JSONNode("raw", rawMode));
	
	n.push_back(JSONNode("captureState", captureState));
	n.push_back(JSONNode("captureDone", captureDone));
	
	
	JSONNode channels(JSON_NODE);
	channels.set_name("channels");
	BOOST_FOREACH (Channel* c, this->channels){
		channels.push_back(c->toJSON());
	}
	n.push_back(channels);

	return n;
}

JSONNode jsonDevicesArray(bool includeChannels){
	JSONNode n(JSON_NODE);

	BOOST_FOREACH (device_ptr d, devices){
		n.push_back(d->toJSON());
	}
	return n;
}
