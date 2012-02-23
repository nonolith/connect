// Nonolith Connect
// https://github.com/nonolith/connect
// Signal generation sources
// Released under the terms of the GNU GPLv3+
// (C) 2012 Nonolith Labs, LLC
// Authors:
//   Kevin Mehall <km@kevinmehall.net>

#define _USE_MATH_DEFINES
#include <cmath>
#include <iostream>

#include "../dataserver.hpp"
#include "../json.hpp"

#include "streaming_device.hpp"

inline void OutputSource::describeJSON(JSONNode &n){
	n.push_back(JSONNode("mode", mode));
	n.push_back(JSONNode("startSample", startSample));
	n.push_back(JSONNode("effective", effective));
	n.push_back(JSONNode("source", displayName()));
}

struct ConstantSource: public OutputSource{
	ConstantSource(unsigned m, float val): OutputSource(m), value(val){}
	virtual string displayName(){return "constant";}
	virtual float getValue(unsigned sample, float sampleTime){ return value; }
	
	virtual void describeJSON(JSONNode &n){
		OutputSource::describeJSON(n);
		n.push_back(JSONNode("value", value));
	}
	
	float value;
};

OutputSource *makeConstantSource(unsigned m, int value){
	return new ConstantSource(m, value);
}

struct AdvSquareWaveSource: public OutputSource{
	AdvSquareWaveSource(unsigned m, float _high, float _low, unsigned _highSamples, unsigned _lowSamples, int _phase):
		OutputSource(m), high(_high), low(_low), highSamples(_highSamples), lowSamples(_lowSamples), phase(_phase){}
	virtual string displayName(){return "adv_square";}
	
	virtual float getValue(unsigned sample, float sampleTime){
		unsigned s = (sample + phase) % (highSamples + lowSamples);
		if (s < lowSamples) return low;
		else                return high;
	}
	
	virtual void describeJSON(JSONNode &n){
		OutputSource::describeJSON(n);
		n.push_back(JSONNode("high", high));
		n.push_back(JSONNode("low", low));
		n.push_back(JSONNode("highSamples", highSamples));
		n.push_back(JSONNode("lowSamples", lowSamples));
	}
	
	float high, low;
	unsigned highSamples, lowSamples;
	int phase;
};

struct PeriodicSource: public OutputSource{
	PeriodicSource(unsigned m, float _offset, float _amplitude, float _period, float _phase=0, bool relPhase=false):
		OutputSource(m), offset(_offset), amplitude(_amplitude), period(_period), phase(_phase), relativePhase(relPhase){}
	
	virtual void describeJSON(JSONNode &n){
		OutputSource::describeJSON(n);
		n.push_back(JSONNode("offset", offset));
		n.push_back(JSONNode("amplitude", amplitude));
		n.push_back(JSONNode("period", period));
	}
	
	virtual void initialize(unsigned sample, OutputSource* prevSrc){
		PeriodicSource* s = dynamic_cast<PeriodicSource*>(prevSrc);
		if (s && relativePhase){
			phase += fmod(sample + s->phase, s->period)/s->period * period - sample;
			phase = fmod(phase, period);
		}
	}
	
	float offset, amplitude, period, phase;
	bool relativePhase;
};

struct SineWaveSource: public PeriodicSource{
	SineWaveSource(unsigned m, float _offset, float _amplitude, float _period, float _phase, bool relPhase):
		PeriodicSource(m, _offset, _amplitude, _period, _phase, relPhase) {}
	virtual string displayName(){return "sine";}
	virtual float getValue(unsigned sample, float SampleTime){
		return sin((sample + phase) * 2 * M_PI / period)*amplitude + offset;
	}
};

struct TriangleWaveSource: public PeriodicSource{
	TriangleWaveSource(unsigned m, float _offset, float _amplitude, float _period, float _phase, bool relPhase):
		PeriodicSource(m, _offset, _amplitude, _period, _phase, relPhase) {}
	virtual string displayName(){return "triangle";}
	virtual float getValue(unsigned sample, float SampleTime){
		return  (fabs(fmod((sample+phase),period)/period*2-1)*2-1)*amplitude + offset;
	}
};

struct SquareWaveSource: public PeriodicSource{
	SquareWaveSource(unsigned m, float _offset, float _amplitude, float _period, float _phase, bool relPhase):
		PeriodicSource(m, _offset, _amplitude, _period, _phase, relPhase) {}
	virtual string displayName(){return "square";}
	virtual float getValue(unsigned sample, float SampleTime){
		unsigned s = fmod(sample + phase, period);
		if (s < period/2) return offset+amplitude;
		else              return offset-amplitude;
	}
};



OutputSource* makeSource(JSONNode& n){
	string source = jsonStringProp(n, "source", "constant");
	unsigned mode = jsonFloatProp(n, "mode"); //TODO: validate
	if (source == "constant"){
		float val = jsonFloatProp(n, "value");
		return new ConstantSource(mode, val);
	}else if (source == "adv_square"){
		float high = jsonFloatProp(n, "high");
		float low = jsonFloatProp(n, "low");
		int highSamples = jsonIntProp(n, "high");
		int lowSamples = n.at("lowSamples").as_int();
		int phase = jsonIntProp(n, "phase", 0);
		return new AdvSquareWaveSource(mode, high, low, highSamples, lowSamples, phase);
	}else if (source == "sine" || source == "triangle" || source == "square"){
		float offset = jsonFloatProp(n, "offset");
		float amplitude = jsonFloatProp(n, "amplitude");
		float period = jsonFloatProp(n, "period");
		float phase = jsonFloatProp(n, "phase", 0);
		bool relPhase = jsonBoolProp(n, "relPhase", true);
		
		if (source == "sine")
			return new SineWaveSource(mode, offset, amplitude, period, phase, relPhase);
		else if (source == "triangle")
			return new TriangleWaveSource(mode, offset, amplitude, period, phase, relPhase);
		else if (source == "square")
			return new SquareWaveSource(mode, offset, amplitude, period, phase, relPhase);
	}
	throw ErrorStringException("Invalid source");
}
