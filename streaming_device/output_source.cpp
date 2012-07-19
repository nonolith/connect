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
#include <vector>
#include <utility>

using std::vector;
using std::pair;

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
	virtual float getValue(unsigned sample, double sampleTime){ return value; }
	
	virtual void describeJSON(JSONNode &n){
		OutputSource::describeJSON(n);
		n.push_back(JSONNode("value", value));
	}
	
	float value;
};

OutputSource *makeConstantSource(unsigned m, float value){
	return new ConstantSource(m, value);
}

struct AdvSquareWaveSource: public OutputSource{
	AdvSquareWaveSource(unsigned m, float _high, float _low, unsigned _highSamples, unsigned _lowSamples, int _phase):
		OutputSource(m), high(_high), low(_low), highSamples(_highSamples), lowSamples(_lowSamples), phase(_phase){}
	virtual string displayName(){return "adv_square";}
	
	virtual float getValue(unsigned sample, double sampleTime){
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
	PeriodicSource(unsigned m, float _offset, float _amplitude, double _period, double _phase=0, bool relPhase=false):
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
	
	virtual double getPhaseZeroAfterSample(unsigned sample){
		return (double) sample + period - fmod(sample+phase, period);
	}
	
	double offset, amplitude, period, phase;
	bool relativePhase;
};

struct SineWaveSource: public PeriodicSource{
	SineWaveSource(unsigned m, float _offset, float _amplitude, double _period, double _phase, bool relPhase):
		PeriodicSource(m, _offset, _amplitude, _period, _phase, relPhase) {}
	virtual string displayName(){return "sine";}
	virtual float getValue(unsigned sample, double SampleTime){
		return sin((sample + phase) * 2 * M_PI / period)*amplitude + offset;
	}
};

struct TriangleWaveSource: public PeriodicSource{
	TriangleWaveSource(unsigned m, float _offset, float _amplitude, double _period, double _phase, bool relPhase):
		PeriodicSource(m, _offset, _amplitude, _period, _phase, relPhase) {}
	virtual string displayName(){return "triangle";}
	virtual float getValue(unsigned sample, double SampleTime){
		return  (fabs(fmod((sample+phase-period/4),period)/period*2-1)*2-1)*amplitude + offset;
	}
};

struct SquareWaveSource: public PeriodicSource{
	SquareWaveSource(unsigned m, float _offset, float _amplitude, double _period, double _phase, bool relPhase):
		PeriodicSource(m, _offset, _amplitude, _period, _phase, relPhase) {}
	virtual string displayName(){return "square";}
	virtual float getValue(unsigned sample, double SampleTime){
		unsigned s = fmod(sample + phase, period);
		if (s < period/2) return offset+amplitude;
		else              return offset-amplitude;
	}
};

struct ArbitraryWaveformSource: public OutputSource{
	
	struct Point{
		unsigned t;
		float v;
		Point(unsigned t_, float v_): t(t_), v(v_){};
	};
	typedef vector<Point> Point_vec;

	ArbitraryWaveformSource(unsigned m, int offset_, Point_vec& values_, int repeat_count_):
		OutputSource(m), offset(offset_), values(values_), index(0), repeat_count(repeat_count_){}
	virtual string displayName(){return "arb";}
	
	virtual float getValue(unsigned sample, double sampleTime){
		unsigned length = values.size();
		
		// All times are relative to offset
		sample -= offset;
		
		unsigned time1, time2;
		float value1, value2;
		
		while(1){
			time1  = values[index].t;
			value1 = values[index].v;
			
			unsigned nextIndex = index+1;
			
			if (nextIndex >= length){
				// repeat == -1 means infinite
				if (repeat_count>1 || repeat_count==-1){
					if (repeat_count>0) repeat_count--;
					index = 0;
					offset += time1;
					sample -= time1;
					continue;
				}else{
					// If repeat is disabled, the last value remains forever
					return value1;
				}
			}
		
			time2  = values[nextIndex].t;
			value2 = values[nextIndex].v;
			
			if (sample >= time2){
				// When we pass the next point, move forward in the list
				index = nextIndex;
				continue;
			}else{
				break;
			}
		}
		
		// For the first point
		if (sample < time1) return value1;
		
		// Proportion of the time between the last point and the next point
		double p = (((double)sample) - time1)/(((double)time2) - time1);
		//std::cout << sample << " time1=" << time1 << " time2=" << time2 << " val1=" << value1 << " val2="<<value2 << " p=" << p <<std::endl;
		
		// Trapezoidal interpolation
		return (1-p) * value1 + p*value2;
	}
	
	virtual void describeJSON(JSONNode &n){
		OutputSource::describeJSON(n);
	}
	
	virtual void initialize(unsigned sample, OutputSource* prevSrc){
		if (offset == -1){
			offset = sample;
		}
	}
	
	int offset;
	Point_vec values;
	unsigned index;
	int repeat_count;
};

OutputSource* makeSource(unsigned mode, const string& source, float offset, float amplitude, double period, double phase, bool relPhase){
	if (source == "sine")
			return new SineWaveSource(mode, offset, amplitude, period, phase, relPhase);
	else if (source == "triangle")
		return new TriangleWaveSource(mode, offset, amplitude, period, phase, relPhase);
	else if (source == "square")
		return new SquareWaveSource(mode, offset, amplitude, period, phase, relPhase);
	throw ErrorStringException("Invalid source");
}

OutputSource* makeAdvSquare(unsigned mode, float high, float low, unsigned highSamples, unsigned lowSamples, unsigned phase){
	return new AdvSquareWaveSource(mode, high, low, highSamples, lowSamples, phase);
}

OutputSource* makeSource(JSONNode& n){
	string source = jsonStringProp(n, "source", "constant");
	unsigned mode = jsonFloatProp(n, "mode", 0); //TODO: validate
	if (source == "constant"){
		float val = jsonFloatProp(n, "value", 0);
		return new ConstantSource(mode, val);
		
	}else if (source == "adv_square"){
		float high = jsonFloatProp(n, "high");
		float low = jsonFloatProp(n, "low");
		int highSamples = jsonIntProp(n, "highSamples");
		int lowSamples = jsonIntProp(n, "lowSamples");
		int phase = jsonIntProp(n, "phase", 0);
		return new AdvSquareWaveSource(mode, high, low, highSamples, lowSamples, phase);
		
	}else if (source == "sine" || source == "triangle" || source == "square"){
		float offset = jsonFloatProp(n, "offset");
		float amplitude = jsonFloatProp(n, "amplitude");
		double period = jsonFloatProp(n, "period");
		double phase = jsonFloatProp(n, "phase", 0);
		bool relPhase = jsonBoolProp(n, "relPhase", true);
		return makeSource(mode, source, offset, amplitude, period, phase, relPhase);
		
	}else if (source=="arb"){
		unsigned offset = jsonIntProp(n, "offset", -1);
		unsigned repeat = jsonIntProp(n, "repeat", 0);
		
		ArbitraryWaveformSource::Point_vec values;
		JSONNode j_values = n.at("values");
		for(JSONNode::iterator i=j_values.begin(); i!=j_values.end(); i++){
			values.push_back(ArbitraryWaveformSource::Point(
					jsonIntProp(*i, "t"),
					jsonFloatProp(*i, "v")));
		}
		
		if (values.size() < 1) throw ErrorStringException("Arb wave must have at least one point");
		
		return new ArbitraryWaveformSource(mode, offset, values, repeat);
	}
	throw ErrorStringException("Invalid source");
}
