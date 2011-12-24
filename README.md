Nonolith Data Server
====================

This server runs in the background and manages USB communication with Nonolith
devices and provides their services to clients that connect over WebSocket or
TCP. Multiple programs can access devices simultaneously, for example
controlling a connected device with a custom script while simultaneously
monitoring and collecting data with
[Pixelpulse](http://nonolithlabs.com/pixelpulse). 

Dependencies
------------

`dataserver` is written in C++ and builds with `scons`. It needs several
boost libraries: `asio`, `system`, `date_time`, `regex`, `thread`. It also uses 
[libjson](http://sourceforge.net/projects/libjson/),
[websocketpp](https://github.com/zaphoyd/websocketpp), and
[libusb](http://libusb.sourceforge.net/api-1.0/index.html), which are included
as subdirectories or submodules of this git repository. Note that libusb is a
git snapshot, newer than the version found in Linux distributions. This version
contains bugfixes to the async API.

Websocket Protocol (v0)
-----------------------

* Note that the protocol is still subject to change until standardized as v1 *

Websocket endpoint is http://localhost:9003/ws/v0

Messages are encoded as JSON.

- - -

For client->server messages, the `_cmd` field is one of the following:

### selectDevice

### configure

### listen

### cancelListen

### startCapture

### pauseCapture

### set

### controlTransfer

- - -

For server -> client messages, the `_action` field is one of the following:

### devices

### deviceInfo

### captureState

### configuration

### captureReset

### update

### outputChanged

### controlTransferReturn


