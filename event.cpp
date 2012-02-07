// Nonolith Connect
// https://github.com/nonolith/connect
// Publish/subscribe events helper
// Released under the terms of the GNU GPLv3+
// (C) 2012 Nonolith Labs, LLC
// Authors:
//   Kevin Mehall <km@kevinmehall.net>

#include "dataserver.hpp"
#include <boost/foreach.hpp>

void EventListener::subscribe(Event& e, void_function h){
	unsubscribe();
	event = &e;
	handler = h;
	event->listeners.insert(this);
}

void EventListener::unsubscribe(){
	if (event){
		event->listeners.erase(this);
		event = 0;
	}
}

void Event::notify(){
	for (std::set<EventListener*>::iterator it=listeners.begin(); it!=listeners.end();){
		EventListener* e = *it;
		it++; // Increment before calling handler in case the handler removes this listener,
		      // which would invalidate the iterator.
		e->handler();
	}
}

Event::~Event(){
	BOOST_FOREACH(EventListener* e, listeners){
		e->event = 0;
	}
}
