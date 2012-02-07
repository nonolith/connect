// Nonolith Connect
// https://github.com/nonolith/connect
// Websocketpp handler class
// Released under the terms of the GNU GPLv3+
// (C) 2012 Nonolith Labs, LLC
// Authors:
//   Kevin Mehall <km@kevinmehall.net>

#include <boost/shared_ptr.hpp>
#include "websocketpp.hpp"
#include "websocket_connection_handler.hpp"

#include <map>
#include <string>
#include <vector>

class data_server_handler : public websocketpp::connection_handler {
public:
	data_server_handler() {}
	virtual ~data_server_handler() {}
	
	void on_client_connect(websocketpp::session_ptr client);
	
	void on_open(websocketpp::session_ptr client);
		
	void on_close(websocketpp::session_ptr client);
	
	void on_message(websocketpp::session_ptr client,const std::string &msg);
	
	void on_message(websocketpp::session_ptr client,
		const std::vector<unsigned char> &data);
private:
	
};

typedef boost::shared_ptr<data_server_handler> data_server_handler_ptr;
