#pragma once
#include <string>
#include <boost/algorithm/string.hpp>


typedef std::string::const_iterator iterator_t;
using std::string;

struct Url{
	Url(const string url){
		iterator_t queryStart = std::find(url.begin(), url.end(), '?');
		string path(url.begin(), queryStart);
		boost::split(pathparts, path, boost::is_any_of("/"));
		
		if (pathparts.at(pathparts.size()-1) == "") pathparts.pop_back(); // Remove trailing /
		
		if (queryStart != url.end()) queryStart++; // skip '?'
		string query(queryStart, url.end());
	}
	
	std::vector<std::string> pathparts;
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
};
