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

libs = ['usb_nonolith', 'websocketpp', 'json']


if sys.platform.startswith('linux'):
	boostlibs = ['boost_system','boost_date_time', 'boost_regex', 'boost_thread']
	frameworks = ['']
	pass
elif sys.platform == 'darwin':
	boostlibs = ['boost_system-mt','boost_date_time-mt', 'boost_regex-mt', 'boost_thread-mt']
	libs += ['objc']
	frameworks = ['CoreFoundation', 'IOKit']
elif sys.platform.startswith('win'):
    pass
else:
    print "Unknown platform", sys.platform

libs += boostlibs

Program('server', sources, LIBS=libs, CCFLAGS=['-Wall', '-g', '-Iwebsocketpp/src', '-shared'], LIBPATH=['.'], FRAMEWORKS=frameworks)

