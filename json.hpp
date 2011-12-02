#pragma once

#include "libjson/libjson.h"

#include "dataserver.hpp"

JSONNode toJSON(InputStream* s);
JSONNode toJSON(Channel *channel);
JSONNode toJSON(device_ptr d, bool includeChannels=false);

JSONNode jsonDevicesArray(bool includeChannels=false);