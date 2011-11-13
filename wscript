def options(opt):
        opt.load('compiler_cxx')

def configure(cnf):
    cnf.load('compiler_cxx boost')
    cnf.check(features='cxx cxxprogram', lib=['usb-1.0'], uselib_store='usb')
    cnf.check_boost(lib='system date_time regex thread')

def build(bld):
    bld.env.STLIB_MARKER = ''
    bld.env.SHLIB_MARKER = ''

    websocketpp = bld.new_task_gen(features='cxx cxxstlib')
    websocketpp.source=['websocketpp/src/'+i for i in [
                'network_utilities.cpp',
                'websocket_frame.cpp',
                'websocket_server.cpp',
                'websocket_server_session.cpp',
                'websocket_session.cpp',
                'sha1/sha1.cpp',
                'base64/base64.cpp'
            ]]
    websocketpp.target='websocketpp'
    websocketpp.name='websocketpp'
    websocketpp.use = ['BOOST']
    
    libjson = bld.new_task_gen(features='cxx cxxstlib')
    libjson.source = ['libjson/Source/'+i for i in [
    	'internalJSONNode.cpp',
		'JSONAllocator.cpp',
		'JSON_Base64.cpp',
		'JSONChildren.cpp',
		'JSONDebug.cpp',
		'JSONIterators.cpp',
		'JSONMemory.cpp',
		'JSONNode.cpp',
		'JSONNode_Mutex.cpp',
		'JSONPreparse.cpp',
		'JSONStream.cpp',
		'JSONValidator.cpp',
		'JSONWorker.cpp',
		'JSONWriter.cpp',
		'libjson.cpp', 
    ]]
    libjson.cxxflags = "-c -O3 -ffast-math -fexpensive-optimizations".split()
    libjson.target = "json"
    libjson.name = "json"

    bld.program(features='cxx cxxprogram',
                source=[
                    'server.cpp',
                    'device.cpp',
                    'event.cpp',
                    'usb.cpp',
                    'json_service.cpp',
                    'websocket_service.cpp',
                    'cee/cee.cpp',
                    'demoDevice/demoDevice.cpp'
                ],
                target='server',
                cxxflags=['-Wall', '-g', '-std=gnu++0x', '-I../websocketpp/src'],
                use=['websocketpp', 'usb', 'json', 'BOOST']
    )
