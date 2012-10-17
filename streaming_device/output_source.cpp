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
#include <boost/foreach.hpp>

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
	n.push_back(JSONNode("hint", hint));
}

struct ConstantSource: public OutputSource{
	ConstantSource(unsigned m, float val): OutputSource(m), value(val){}
	virtual string displayName(){return "constant";}
	virtual float getValue(unsigned sample, double sampleTime){ return value; }
	
	virtual void describeJSON(JSONNode &n){
		OutputSource::describeJSON(n);
		n.push_back(JSONNode("value", value));
	}

	virtual double getPhaseZeroAfterSample(unsigned sample){
		return sample;
	}
	
	float value;
};

OutputSource *makeConstantSource(unsigned m, float value){
	return new ConstantSource(m, value);
}

struct AdvSquareWaveSource: public OutputSource{
	AdvSquareWaveSource(unsigned m, float _high, float _low, unsigned _highSamples, unsigned _lowSamples, int _phase, bool _relPhase):
		OutputSource(m), high(_high), low(_low), highSamples(_highSamples), lowSamples(_lowSamples), phase(_phase), relPhase(_relPhase){
			if (highSamples + lowSamples == 0)
				throw ErrorStringException("Square wave must have nonzero period.");
		}
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

	virtual double getPhaseZeroAfterSample(unsigned sample){
		unsigned per = highSamples+lowSamples;
		return sample + (per + lowSamples - (sample + phase) % per) % per;
	}

	virtual void initialize(unsigned sample, OutputSource* prevSrc){
		AdvSquareWaveSource* s = dynamic_cast<AdvSquareWaveSource*>(prevSrc);
		if (s && relPhase){
			unsigned period = highSamples + lowSamples;
			unsigned oldPeriod = s->highSamples + s->lowSamples;
			phase += round(float((sample + s->phase)%oldPeriod)/oldPeriod * period) - sample%period;
		}
		phase = fmod(phase, highSamples+lowSamples);
	}
	
	float high, low;
	unsigned highSamples, lowSamples;
	int phase;
	bool relPhase;
};

struct PeriodicSource: public OutputSource{
	PeriodicSource(unsigned m, float _offset, float _amplitude, double _period, double _phase=0, bool relPhase=false):
		OutputSource(m), offset(_offset), amplitude(_amplitude), period(_period), phase(_phase), relativePhase(relPhase){}
	
	virtual void describeJSON(JSONNode &n){
		OutputSource::describeJSON(n);
		n.push_back(JSONNode("offset", offset));
		n.push_back(JSONNode("amplitude", amplitude));
		n.push_back(JSONNode("period", period));
		n.push_back(JSONNode("phase", phase));
	}
	
	virtual void initialize(unsigned sample, OutputSource* prevSrc){
		PeriodicSource* s = dynamic_cast<PeriodicSource*>(prevSrc);
		if (s && relativePhase){
			phase += fmod(sample + s->phase, s->period)/s->period * period - sample;
		}
		phase = fmod(phase, period);
	}
	
	virtual double getPhaseZeroAfterSample(unsigned sample){
		return (double) sample + fmod(period - fmod(sample+phase, period), period);
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
		double s = fmod(sample + phase, period);
		if (s < period/2) return offset+amplitude;
		else              return offset-amplitude;
	}

	virtual double getPhaseZeroAfterSample(unsigned sample){
		// its own definition because it jumps instead of slides
		double s = fmod(sample+phase, period);
		return (double) sample + ceil(period - s);
	}
};

struct ArbitraryWaveformSource: public OutputSource{
	ArbitraryWaveformSource(unsigned m, int phase_, ArbWavePoint_vec& values_, int repeat_count_):
		OutputSource(m), phase(phase_), values(values_), index(0), repeat_count(repeat_count_){
			if (repeat_count == 0) repeat_count = 1;

			if (values.size() < 1) throw ErrorStringException("Arb wave must have at least one point.");
			if (values[0].t != 0) throw ErrorStringException("Arb wave first point must have t=0.");

			unsigned last_t = 0;

			BOOST_FOREACH(ArbWavePoint& point, values){
				if (point.t < last_t)
					throw ErrorStringException("Arb wave points must be in time order.");
				last_t = point.t;
			}

			if (period() == 0 && repeat_count != 1)
				throw ErrorStringException("Arb wave with repeat must have nonzero period.");
	}

	virtual string displayName(){return "arb";}

	unsigned period(){
		return values[values.size()-1].t;
	}
	
	virtual float getValue(unsigned sample, double sampleTime){
		unsigned length = values.size();

		if (sample < startTime){
			sample = 0;
		}else{
			// All times are relative to startTime
			sample -= startTime;
		}
		
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
					startTime += time1;
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
		std::cout << sample << " time1=" << time1 << " time2=" << time2 << " val1=" << value1 << " val2="<<value2 << " p=" <<p<<" val="<<(1-p) * value1 + p*value2<<std::endl;
		
		// Trapezoidal interpolation
		return (1-p) * value1 + p*value2;
	}
	
	virtual void describeJSON(JSONNode &n){
		OutputSource::describeJSON(n);
		n.push_back(JSONNode("phase", phase));
		n.push_back(JSONNode("repeat", repeat_count));

		JSONNode points = JSONNode(JSON_ARRAY);
		BOOST_FOREACH(ArbWavePoint& point, values){
			JSONNode o;
			o.push_back(JSONNode("t", point.t));
			o.push_back(JSONNode("v", point.v));
			points.push_back(o);
		}
		points.set_name("values");
		n.push_back(JSONNode(points));
		n.push_back(JSONNode("period", period()));
	}
	
	virtual void initialize(unsigned sample, OutputSource* prevSrc){
		if (phase < 0){
			startTime = sample;
			phase = sample;
		}else if (repeat_count != 1){
			unsigned per = period();
			startTime = sample - sample % per + phase % per;
		}else{
			startTime = phase;
		}
	}

	virtual double getPhaseZeroAfterSample(unsigned sample){
		unsigned per = period();
		if (per == 0) return sample;
		return sample + (per - (sample - phase) % per) % per;
	}
	
	int phase;
	unsigned startTime;
	ArbWavePoint_vec values;
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

OutputSource* makeAdvSquare(unsigned mode, float high, float low, unsigned highSamples, unsigned lowSamples, unsigned phase, bool relPhase){
	return new AdvSquareWaveSource(mode, high, low, highSamples, lowSamples, phase, relPhase);
}

OutputSource* makeArbitraryWaveform(unsigned mode, int phase, ArbWavePoint_vec& values, int repeat_count){
	return new ArbitraryWaveformSource(mode, phase, values, repeat_count);
}

OutputSource* makeSource(JSONNode& n){
	string source = jsonStringProp(n, "source", "constant");
	unsigned mode = jsonFloatProp(n, "mode", 0); //TODO: validate
	string hint = jsonStringProp(n, "hint", "");

	OutputSource *r = 0;

	if (source == "constant"){
		float val = jsonFloatProp(n, "value", 0);
		r = makeConstantSource(mode, val);
		
	}else if (source == "adv_square"){
		float high = jsonFloatProp(n, "high");
		float low = jsonFloatProp(n, "low");
		int highSamples = jsonIntProp(n, "highSamples");
		int lowSamples = jsonIntProp(n, "lowSamples");
		int phase = jsonIntProp(n, "phase", 0);
		bool relPhase = jsonBoolProp(n, "relPhase", true);
		r = makeAdvSquare(mode, high, low, highSamples, lowSamples, phase, relPhase);
		
	}else if (source == "sine" || source == "triangle" || source == "square"){
		float offset = jsonFloatProp(n, "offset");
		float amplitude = jsonFloatProp(n, "amplitude");
		double period = jsonFloatProp(n, "period");
		double phase = jsonFloatProp(n, "phase", 0);
		bool relPhase = jsonBoolProp(n, "relPhase", true);
		r = makeSource(mode, source, offset, amplitude, period, phase, relPhase);
		
	}else if (source=="arb"){
		unsigned phase = jsonIntProp(n, "phase", -1);
		unsigned repeat = jsonIntProp(n, "repeat", 0);
		
		ArbWavePoint_vec values;
		JSONNode j_values = n.at("values");
		for(JSONNode::iterator i=j_values.begin(); i!=j_values.end(); i++){
			values.push_back(ArbWavePoint(
					jsonIntProp(*i, "t"),
					jsonFloatProp(*i, "v")));
		}
		
		r = makeArbitraryWaveform(mode, phase, values, repeat);
	}else{
		throw ErrorStringException("Invalid source");
	}

	r->hint = hint;
	return r;
}
