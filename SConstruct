import sys, os, shutil

libs = ['usb_nonolith', 'websocketpp', 'json']
boostlibs = ['boost_system','boost_date_time', 'boost_regex', 'boost_thread']

env = Environment()

opts = Variables()
opts.Add(BoolVariable("mingwcross", "Cross-compile with mingw for Win32", 0))
opts.Add(BoolVariable("boost_static", "Statically link against Boost", 1))
Help(opts.GenerateHelpText(env))
opts.Update(env)

platform = None
target = None

env.Append(LIBPATH=['.'])

if sys.platform.startswith('linux'):
	platform = target = 'linux'
elif sys.platform == 'darwin':
	platform = target = 'osx'
elif sys.platform.startswith('win'):
	platform = target = 'windows'
else:
	print "Unknown platform"
	exit()

version = 'unknown'
try:	
	version = os.popen("git describe --always --dirty='*'").read().strip()
except:
	pass
	
if env['mingwcross']:
	target = 'windows'
	env.Platform('cygwin')
	env['CC']  = 'i586-mingw32msvc-gcc'
	env['CXX'] = 'i586-mingw32msvc-g++'
	env['AR']  = 'i586-mingw32msvc-ar'
	env['RANLIB'] = 'i586-mingw32msvc-ranlib'

frameworks = []

if target is 'linux':	
	static = True
	
	if static:
		boost_lib = '/usr/lib/'
		boostlibs = [env.File(boost_lib+env['LIBPREFIX']+i+env['LIBSUFFIX']) for i in boostlibs]
		libs = [env.File(env['LIBPREFIX']+i+env['LIBSUFFIX']) for i in libs]
		
	libs += ['udev', 'pthread', 'rt']
	
elif target is 'osx':
	boostlibs = [i+'-mt' for i in boostlibs]
	static = True
	if static:
		boost_lib = '/usr/local/lib/'
		boostlibs = [env.File(boost_lib+env['LIBPREFIX']+i+env['LIBSUFFIX']) for i in boostlibs]
		libs = [env.File(env['LIBPREFIX']+i+env['LIBSUFFIX']) for i in libs]

	libs += ['objc']
	frameworks = ['CoreFoundation', 'IOKit']
	LIBPATH=['.']
	
elif target is 'windows':
	if platform is 'windows':
		boost_dir='C:\\Program Files\\boost\\src\\'
		boost_lib=boost_dir+'stage\\lib\\'
		env.Append(CPPPATH = boost_dir)
	else:
		boost_lib = '../lib/win/boost/lib/'
		env.Append(CPPPATH = '../lib/win/boost/include')
		
	env.Append(
		tools=['mingw'],
		CPPFLAGS=["-D_WIN32_WINNT=0x0501", '-static', '-D BOOST_THREAD_USE_LIB', '-Wl,-subsystem,windows'],
		LIBPATH=[boost_lib, '.']
	)
	
	static = True
	boostlibs.remove('boost_thread')
	boostlibs.append('boost_thread_win32')
	if static:
		boostlibs = [env.File(boost_lib+env['LIBPREFIX']+i+'-mt-s'+env['LIBSUFFIX']) for i in boostlibs]
	else:
		boostlibs = [i+'-mgw46-mt-1_48' for i in boostlibs]
		
	libs += ['ws2_32', 'mswsock']
    


sources = Glob('*.cpp') + ['cee/cee.cpp', 'bootloader/bootloader.cpp']

json = env.Library('json', 
	Glob('libjson/Source/*.cpp'),
	CCFLAGS = "-c -O3 -ffast-math -fexpensive-optimizations".split()
)

websocketpp = env.Library('websocketpp', ['websocketpp/src/'+i for i in [
                'network_utilities.cpp',
                'websocket_frame.cpp',
                'websocket_server.cpp',
                'websocket_server_session.cpp',
                'websocket_session.cpp',
                'sha1/sha1.cpp',
                'base64/base64.cpp'
            ]], CCFLAGS=['-g', '-O3'])


libusb_cflags = []
if target is 'linux':
	libusb_os = ['os/linux_usbfs.c', 'os/threads_posix.c']
	libusb_cflags += ['-D OS_LINUX', '-D USBI_TIMERFD_AVAILABLE', '-D THREADS_POSIX', '-DPOLL_NFDS_TYPE=nfds_t', '-D HAVE_POLL_H']
elif target is 'osx':
	libusb_os = ['os/darwin_usb.c']
	libusb_cflags += ['-D OS_DARWIN', '-D THREADS_POSIX', '-DPOLL_NFDS_TYPE=nfds_t', '-D HAVE_POLL_H']
elif target is 'windows':
	libusb_os = ['os/poll_windows.c', 'os/windows_usb.c', 'os/threads_windows.c']
	libusb_cflags += ['-D OS_WINDOWS', '-DPOLL_NFDS_TYPE=unsigned int', '-D WINVER=0x0501']

libusb = env.Library('libusb_nonolith', ['libusb/libusb/'+i for i in [
				'core.c',
				'descriptor.c',
				'io.c',
				'sync.c',
			]+libusb_os], CFLAGS=['-g', '-O3', '-Ilibusb', '-Ilibusb/libusb']+libusb_cflags) 


libs += boostlibs

env.Program('nonolith-connect', sources, LIBS=libs, CCFLAGS=['-Wall', '-g', '-O3', '-Ilibusb', '-Iwebsocketpp/src', '-shared', "-DVERSION='%s'"%version], FRAMEWORKS=frameworks)
