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

    bld.program(features='cxx cxxprogram',
                source=[
                    'server.cpp',
                    'usb.cpp',
                    'cee/cee.cpp',
                ],
                target='server',
                cxxflags=['-Wall', '-g', '-std=gnu++0x', '-I../websocketpp/src'],
                use=['websocketpp', 'libusb']
    )
