#define _USE_MATH_DEFINES
#include <cmath>

#include "dataserver.hpp"
#include "json.hpp"

struct SquareWaveSource: public OutputSource{
	SquareWaveSource(unsigned m, float _high, float _low, unsigned _highSamples, unsigned _lowSamples):
		OutputSource(m), high(_high), low(_low), highSamples(_highSamples), lowSamples(_lowSamples){}
	virtual string displayName(){return "Square";}
	
	virtual float getValue(unsigned sample, float sampleTime){
		unsigned s = sample % (highSamples + lowSamples);
		if (s < lowSamples) return low;
		else                return high;
	}
	
	float high, low;
	unsigned highSamples, lowSamples;
};

struct SineWaveSource: public OutputSource{
	SineWaveSource(unsigned m, float _offset, float _amplitude, int _period):
		OutputSource(m), offset(_offset), amplitude(_amplitude), period(_period){}
		
	virtual string displayName(){return "Sine";}
	
	virtual float getValue(unsigned sample, float SampleTime){
		return sin(sample * 2 * M_PI / period)*amplitude + offset;
	}
	
	float offset, amplitude;
	int period;
};



OutputSource* makeSource(JSONNode& n){
	string source = n.at("source").as_string();
	unsigned mode = n.at("mode").as_int(); //TODO: validate
	if (source == "constant"){
		float val = n.at("value").as_float();
		return new ConstantOutputSource(mode, val);
	}else if (source == "square"){
		float high = n.at("high").as_float();
		float low = n.at("low").as_float();
		int highSamples = n.at("highSamples").as_int();
		int lowSamples = n.at("lowSamples").as_int();
		return new SquareWaveSource(mode, high, low, highSamples, lowSamples);
	}else if (source == "sine"){
		float offset = n.at("offset").as_float();
		float amplitude = n.at("amplitude").as_float();
		int period = n.at("period").as_int();
		return new SineWaveSource(mode, offset, amplitude, period);
	}
}
