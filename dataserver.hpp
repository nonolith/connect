#pragma once

#define INCLUDE_FROM_DATASERVER_HPP

#include <set>
#include <vector>
#include <string>
using std::string;

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>

extern boost::asio::io_service io;

#include "event.hpp"

extern Event device_list_changed;
extern Event streaming_state_changed;

#include "device.hpp"

void usb_init();
void usb_scan_devices();
void usb_thread_main();

extern std::set<device_ptr> devices;