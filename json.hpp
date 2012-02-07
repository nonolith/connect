// Nonolith Connect
// https://github.com/nonolith/connect
// JSON header
// Released under the terms of the GNU GPLv3+
// (C) 2012 Nonolith Labs, LLC
// Authors:
//   Kevin Mehall <km@kevinmehall.net>

#pragma once

#include "libjson/libjson.h"
#include "dataserver.hpp"

JSONNode jsonDevicesArray(bool includeChannels=false);
