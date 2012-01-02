import sys, os, shutil

libs = ['usb_nonolith', 'websocketpp', 'json']
boostlibs = ['boost_system','boost_date_time', 'boost_regex', 'boost_thread']

if sys.platform.startswith('linux'):
	frameworks = ['']
	libs = ['udev']
elif sys.platform == 'darwin':
	boostlibs = [i+'-mt' for i in boostlibs]
	libs += ['objc']
	frameworks = ['CoreFoundation', 'IOKit']
	LIBPATH=['.']
elif sys.platform.startswith('win'):
	boost_dir='C:\\Program Files\\boost\\src\\'
	boost_lib=boost_dir+'stage\\lib\\'
	env = Environment(
		tools=['mingw'],
		CPPPATH=boost_dir,
		CPPFLAGS=["-D_WIN32_WINNT=0x0501", '-static'],
		LIBPATH=[boost_lib, '.']
	)
	
	static = False
	
	if static:
		boostlibs = [env.File(boost_lib+env['LIBPREFIX']+i+'-mgw46-mt-1_48'+env['LIBSUFFIX']) for i in boostlibs]
	else:
		boostlibs = [i+'-mgw46-mt-1_48' for i in boostlibs]
		
	frameworks = ['']
	libs += ['ws2_32', 'mswsock']
else:
    print "Unknown platform", sys.platform

sources = Glob('*.cpp') + ['cee/cee.cpp']

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


def make_in_subdir(dir, libsrc, libdest):
	def fn(source, target, env, *args):
		print "Making in subdir"
		os.chdir(dir)
		try:
			r = os.system('sh autogen.sh')
			if r: raise IOError("Autogen failed")
			r = os.system('sh configure')
			if r: raise IOError("Configure failed")
			r = os.system('make')
			if r: raise IOError("Make failed")
			shutil.copy(libsrc, libdest)
		finally:
			os.chdir('..')
	return fn
	
			
#env.Command('libusb_nonolith.a', [], make_in_subdir('libusb', 'libusb/.libs/libusb-1.0.a', '../libusb_nonolith.a'))

libs += boostlibs

env.Program('server', sources, LIBS=libs, CCFLAGS=['-Wall', '-g', '-O3', '-Ilibusb', '-Iwebsocketpp/src'], FRAMEWORKS=frameworks)
