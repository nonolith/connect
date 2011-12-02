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
	for (std::set<EventListener*>::iterator it=listeners.begin(); it!=listeners.end(); it++){
		(*it)->handler();
	}
}

Event::~Event(){
	BOOST_FOREACH(EventListener* e, listeners){
		e->event = 0;
	}
}