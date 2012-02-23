#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include "url.hpp"

typedef std::string::const_iterator iterator_t;
using std::string;

void parse_query(const string& query, std::map<string, string>& map){
		std::vector<std::string> vars;
		boost::split(vars, query, boost::is_any_of("&"));
		
		BOOST_FOREACH(string& pair, vars){
			std::string::size_type sep = pair.find("=",0);
			
			string key, value;		
			
			if (sep != std::string::npos) {
				key = pair.substr(0, sep);
				value = pair.substr(sep+1);
				map[key] = value;
			}
		}
}


Url::Url(const string url){
	iterator_t queryStart = std::find(url.begin(), url.end(), '?');
	string path(url.begin(), queryStart);
	boost::split(pathparts, path, boost::is_any_of("/"));
	
	if (pathparts.at(pathparts.size()-1) == "") pathparts.pop_back(); // Remove trailing /
	
	if (queryStart != url.end()) queryStart++; // skip '?'
	string query(queryStart, url.end());
	parse_query(query, params);
}
