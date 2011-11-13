#pragma once

#define INCLUDE_FROM_DATASERVER_HPP

#include <set>
#include <vector>
#include <string>
using std::string;

#include <boost/asio.hpp>
extern boost::asio::io_service io;

#include "event.hpp"
#include "device.hpp"

extern std::set<device_ptr> devices;

extern Event device_list_changed;
extern Event streaming_state_changed;

void usb_init();
void usb_scan_devices();
void usb_thread_main();