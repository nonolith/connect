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


if not os.path.exists('libusb/config.h'):
	os.system('cd libusb; ./autogen.sh && ./configure')

if sys.platform.startswith('linux'):
    libusb_platform = ['os/linux_usbfs.c', 'os/threads_posix.c']
elif sys.platform == 'darwin':
    libusb_platform = ['os/darwin_usb.c', 'os/threads_posix.c']
elif sys.platform.startswith('win'):
    libusb_platform = ['os/poll_windows.c', 'os/windows_usb.c', 'os/threads_windows.c']
else:
    print "Unknown platform", sys.platform
libusb_sources = Glob('libusb/libusb/*.c') + ['libusb/libusb/'+i for i in libusb_platform]
libusb = Library('usb', 
	libusb_sources, CFLAGS=['-Ilibusb', '-Ilibusb/libusb'],
	CCFLAGS = ['-DHAVE_CONFIG_H', '-I.',  '-I..', '-fvisibility=hidden', '-std=gnu99',
	           '-Wall', '-Wundef', '-Wunused', '-Wstrict-prototypes', '-Werror-implicit-function-declaration',
	            '-Wno-pointer-sign', '-Wshadow', '-pthread', '-g', '-O2']
)

boostlibs = ['boost_system','boost_date_time', 'boost_regex', 'boost_thread']

Program('server', sources, LIBS=['usb', 'websocketpp', 'json']+boostlibs, CCFLAGS=['-Wall', '-g', '-Iwebsocketpp/src'], LIBPATH='.')