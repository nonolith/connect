#define _USE_MATH_DEFINES
#include <cmath>

#include "dataserver.hpp"
#include "json.hpp"

struct ConstantSource: public OutputSource{
	ConstantSource(unsigned m, float val): OutputSource(m), value(val){}
	virtual string displayName(){return "constant";}
	virtual float getValue(unsigned sample, float sampleTime){ return value; }
	
	virtual void describeJSON(JSONNode &n){
		n.push_back(JSONNode("source", "constant"));
		n.push_back(JSONNode("value", value));
	}
	
	float value;
};

OutputSource *makeConstantSource(unsigned m, int value){
	return new ConstantSource(m, value);
}

struct SquareWaveSource: public OutputSource{
	SquareWaveSource(unsigned m, float _high, float _low, unsigned _highSamples, unsigned _lowSamples):
		OutputSource(m), high(_high), low(_low), highSamples(_highSamples), lowSamples(_lowSamples){}
	virtual string displayName(){return "square";}
	
	virtual float getValue(unsigned sample, float sampleTime){
		unsigned s = sample % (highSamples + lowSamples);
		if (s < lowSamples) return low;
		else                return high;
	}
	
	virtual void describeJSON(JSONNode &n){
		n.push_back(JSONNode("source", "square"));
		n.push_back(JSONNode("high", high));
		n.push_back(JSONNode("low", low));
		n.push_back(JSONNode("highSamples", highSamples));
		n.push_back(JSONNode("lowSamples", lowSamples));
	}
	
	float high, low;
	unsigned highSamples, lowSamples;
};

struct PeriodicSource: public OutputSource{
	PeriodicSource(unsigned m, float _offset, float _amplitude, int _period):
		OutputSource(m), offset(_offset), amplitude(_amplitude), period(_period){}
	
	virtual void describeJSON(JSONNode &n){
		n.push_back(JSONNode("source", displayName()));
		n.push_back(JSONNode("offset", offset));
		n.push_back(JSONNode("amplitude", amplitude));
		n.push_back(JSONNode("period", period));
	}
	
	float offset, amplitude;
	int period;
};

struct SineWaveSource: public PeriodicSource{
	SineWaveSource(unsigned m, float _offset, float _amplitude, int _period):
		PeriodicSource(m, _offset, _amplitude, _period) {}
	virtual string displayName(){return "sine";}
	virtual float getValue(unsigned sample, float SampleTime){
		return sin(sample * 2 * M_PI / period)*amplitude + offset;
	}
};

struct TriangleWaveSource: public PeriodicSource{
	TriangleWaveSource(unsigned m, float _offset, float _amplitude, int _period):
		PeriodicSource(m, _offset, _amplitude, _period) {}
	virtual string displayName(){return "triangle";}
	virtual float getValue(unsigned sample, float SampleTime){
		return  (fabs(fmod(sample,period)/period*2-1)*2-1)*amplitude + offset;
	}
};



OutputSource* makeSource(JSONNode& n){
	string source = n.at("source").as_string();
	unsigned mode = n.at("mode").as_int(); //TODO: validate
	if (source == "constant"){
		float val = n.at("value").as_float();
		return new ConstantSource(mode, val);
	}else if (source == "square"){
		float high = n.at("high").as_float();
		float low = n.at("low").as_float();
		int highSamples = n.at("highSamples").as_int();
		int lowSamples = n.at("lowSamples").as_int();
		return new SquareWaveSource(mode, high, low, highSamples, lowSamples);
	}else if (source == "sine" || source == "triangle"){
		float offset = n.at("offset").as_float();
		float amplitude = n.at("amplitude").as_float();
		int period = n.at("period").as_int();
		if (source == "sine")
			return new SineWaveSource(mode, offset, amplitude, period);
		else if (source == "triangle")
			return new TriangleWaveSource(mode, offset, amplitude, period);
	}else{
		throw ErrorStringException("Invalid source");
	}
}
