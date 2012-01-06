#pragma once

#define INCLUDE_FROM_DATASERVER_HPP

#include <set>
#include <vector>
#include <string>
using std::string;

#include <boost/asio.hpp>
extern boost::asio::io_service io;

#include "libjson/libjson.h"

#include "event.hpp"
#include "device.hpp"

extern bool debugFlag;

extern std::set<device_ptr> devices;

extern Event device_list_changed;

void usb_init();
void usb_scan_devices();
void usb_thread_main();

struct ErrorStringException : public std::exception{
   string s;
   ErrorStringException (string ss) throw() : s(ss) {}
   virtual const char* what() const throw() { return s.c_str(); }
   virtual ~ErrorStringException() throw() {}
};

