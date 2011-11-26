#include "json.hpp"
#include <boost/foreach.hpp>


JSONNode toJSON(InputStream* s){
	JSONNode n(JSON_NODE);
	n.set_name(s->id);
	n.push_back(JSONNode("id", s->id));
	n.push_back(JSONNode("displayName", s->displayName));
	n.push_back(JSONNode("units", s->units));
	return n;
}

JSONNode toJSON(Channel *channel){
	JSONNode n(JSON_NODE);
	n.set_name(channel->id);
	n.push_back(JSONNode("id", channel->id));
	n.push_back(JSONNode("displayName", channel->displayName));

	JSONNode inputChannels(JSON_NODE);
	inputChannels.set_name("inputs");
	BOOST_FOREACH (InputStream* i, channel->inputs){
		inputChannels.push_back(toJSON(i));
	}
	n.push_back(inputChannels);

	return n;
}

JSONNode toJSON(device_ptr d, bool includeChannels){
	JSONNode n(JSON_NODE);
	n.set_name(d->getId());
	n.push_back(JSONNode("id", d->getId()));
	n.push_back(JSONNode("model", d->model()));
	n.push_back(JSONNode("hwversion", d->hwversion()));
	n.push_back(JSONNode("fwversion", d->fwversion()));
	n.push_back(JSONNode("serial", d->serialno()));

	if (includeChannels){
		JSONNode channels(JSON_NODE);
		channels.set_name("channels");
		BOOST_FOREACH (Channel* c, d->channels){
			channels.push_back(toJSON(c));
		}
		n.push_back(channels);
	}

	return n;
}

JSONNode jsonDevicesArray(){
	JSONNode n(JSON_NODE);

	BOOST_FOREACH (device_ptr d, devices){
		n.push_back(toJSON(d));
	}
	return n;
}