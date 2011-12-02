import sys, os

sources = Glob('*.cpp') + ['cee/cee.cpp', 'demoDevice/demoDevice.cpp']

json = Library('json', 
	Glob('libjson/Source/*.cpp'),
	CCFLAGS = "-c -O3 -ffast-math -fexpensive-optimizations".split()
)

websocketpp = Library('websocketpp', ['websocketpp/src/'+i for i in [
                'network_utilities.cpp',
                'websocket_frame.cpp',
                'websocket_server.cpp',
                'websocket_server_session.cpp',
                'websocket_session.cpp',
                'sha1/sha1.cpp',
                'base64/base64.cpp'
            ]])


Command('libusb/Makefile', [], 'cd libusb; ./autogen.sh && ./configure')
Command('libusb_nonolith.a', ['libusb/Makefile'], 'cd libusb; make; mv libusb/.libs/libusb-1.0.a ../libusb_nonolith.a')

boostlibs = ['boost_system','boost_date_time', 'boost_regex', 'boost_thread']

Program('server', sources, LIBS=['usb_nonolith', 'websocketpp', 'json']+boostlibs, CCFLAGS=['-Wall', '-g', '-Iwebsocketpp/src'], LIBPATH=['.'])