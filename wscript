def options(opt):
        opt.load('compiler_cxx')

def configure(cnf):
    cnf.load('compiler_cxx')
    cnf.check(features='cxx cxxprogram', lib=['usb-1.0'], uselib_store='libusb')

    cnf.check(features='cxx cxxprogram', lib=['boost_system','boost_date_time','boost_regex', 'boost_thread'], uselib_store='boost')


def build(bld):
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
    websocketpp.use = ['boost']
    
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
    libjson.cxxflags = "-c -O3 -ffast-math -fexpensive-optimizations -combine -DNDEBUG".split()
    libjson.target = "libjson"
    libjson.name = "libjson"

    bld.program(features='cxx cxxprogram',
                source=[
                    'server.cpp',
                    'usb.cpp',
                    'json_service.cpp',
                    'cee/cee.cpp',
                ],
                target='server',
                cxxflags=['-Wall', '-g', '-std=gnu++0x', '-I../websocketpp/src'],
                use=['websocketpp', 'libusb', 'libjson']
    )
