#pragma once
#ifndef INCLUDE_FROM_DATASERVER_HPP
#error "device.hpp should not be included directly"
#endif

#include <boost/function.hpp>

typedef boost::function<void()> void_function;

class Event;

class EventListener{
	public:
		EventListener(): event(0){}
		EventListener(Event& e, void_function h){subscribe(e, h);}
		~EventListener(){unsubscribe();}

		void subscribe(Event& e, void_function h);
		void unsubscribe();

		Event* event;
		void_function handler;
	private:
		EventListener(const EventListener&);
   		EventListener& operator=(const EventListener&);
};

class Event{
	public:
		Event(){}
		~Event();
		void notify();
		std::set<EventListener*> listeners;
	private:
		Event(const Event&);
   		Event& operator=(const Event&);
};