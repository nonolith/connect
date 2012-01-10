struct ErrorStringException : public std::exception{
   string s;
   ErrorStringException (string ss) throw() : s(ss) {}
   virtual const char* what() const throw() { return s.c_str(); }
   virtual ~ErrorStringException() throw() {}
};

inline string jsonStringProp(JSONNode &n, const char* prop){
	JSONNode::iterator i = n.find(prop);
	if (i != n.end() && i->type() == JSON_STRING) return i->as_string();
	else throw ErrorStringException(string("JSON missing string property: ") + prop);
}

inline bool jsonBoolProp(JSONNode &n, const char* prop){
	JSONNode::iterator i = n.find(prop);
	if (i != n.end() && i->type() == JSON_BOOL) return i->as_bool();
	else throw ErrorStringException(string("JSON missing bool property: ") + prop);
}

inline int jsonIntProp(JSONNode &n, const char* prop){
	JSONNode::iterator i = n.find(prop);
	if (i != n.end() && i->type() == JSON_NUMBER) return i->as_int();
	else throw ErrorStringException(string("JSON missing int property: ") + prop);
}

inline double jsonFloatProp(JSONNode &n, const char* prop){
	JSONNode::iterator i = n.find(prop);
	if (i != n.end() && i->type() == JSON_NUMBER) return i->as_float();
	else throw ErrorStringException(string("JSON missing float property: ") + prop);
}

inline string jsonStringProp(JSONNode &n, const char* prop, string def){
	JSONNode::iterator i = n.find(prop);
	if (i != n.end() && i->type() == JSON_STRING) return i->as_string();
	else return def;
}

inline int jsonIntProp(JSONNode &n, const char* prop, int def){
	JSONNode::iterator i = n.find(prop);
	if (i != n.end() && i->type() == JSON_NUMBER) return i->as_int();
	else return def;
}

inline bool jsonBoolProp(JSONNode &n, const char* prop, bool def){
	JSONNode::iterator i = n.find(prop);
	if (i != n.end() && i->type() == JSON_BOOL) return i->as_bool();
	else return def;
}

inline double jsonFloatProp(JSONNode &n, const char* prop, double def){
	JSONNode::iterator i = n.find(prop);
	if (i != n.end() && i->type() == JSON_NUMBER) return i->as_float();
	else return def;
}
