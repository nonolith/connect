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
		BOOST_FOREACH(EventListener* e, listeners){
			e->handler();
		}
}

Event::~Event(){
	BOOST_FOREACH(EventListener* e, listeners){
		e->event = 0;
	}
}