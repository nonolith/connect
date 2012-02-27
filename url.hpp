#pragma once
#include <string>
#include <vector>
#include <map>
using std::string;

void parse_query(const string& query, std::map<string, string>& map);
string map_get(std::map<string, string>& map, const string key, const string def="");

struct Url{
	Url(const string url);
	std::vector<std::string> pathparts;
	std::map<std::string, std::string> params;
};

struct UrlPath{
	UrlPath(Url &url_, unsigned level_):url(url_), level(level_){}
	
	Url& url;
	const unsigned level;
	
	UrlPath sub(){
		return UrlPath(url, level+1);
	}
	
	const string& get(){
		return url.pathparts.at(level);
	}
	
	bool matches(string m){
		return get() == m;
	}
	
	bool leaf(){
		return url.pathparts.size() <= level;
	}
	
	string param(string key, string def=""){
		return map_get(url.params, key, def);
	}
};
