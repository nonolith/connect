#include <string>
#include <boost/foreach.hpp>

#include "websocketpp.hpp"

#include "json.hpp"

void devicesRequest(websocketpp::session_ptr client){
	JSONNode n = jsonDevicesArray(true);
	string jc = (string) n.write_formatted();
	client->start_http(200, jc);
}

void rawDataRequest(websocketpp::session_ptr client){
	JSONNode n(JSON_NODE);

	BOOST_FOREACH (device_ptr d, devices){
		JSONNode dn(JSON_NODE);
		dn.set_name(d->serialno());
		BOOST_FOREACH(Channel* c, d->channels){
			JSONNode cn(JSON_NODE);
			cn.set_name(c->id);
			BOOST_FOREACH (InputStream* i, c->inputs){
				JSONNode sn(JSON_ARRAY);
				sn.set_name(i->id);

				for(unsigned x=0; x<i->buffer_fill_point; x++){
					sn.push_back(JSONNode("", i->data[x]));
				}

				cn.push_back(sn);
			}
			
			dn.push_back(cn);	
		}
		n.push_back(dn);
	}

	string jc = (string) n.write_formatted();
	client->start_http(200, jc);
}

void handleJSONRequest(std::vector<std::string> &pathparts, websocketpp::session_ptr client){
	if (pathparts[2] != "v0"){
		client->start_http(404, "API version not supported");
		return;
	}

	if (pathparts[3] == "devices"){
		devicesRequest(client);
	}else if (pathparts[3] == "raw_data"){
		rawDataRequest(client);
	}else{
		client->start_http(404, "REST object not found");
	}
}