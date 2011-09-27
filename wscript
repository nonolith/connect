def options(opt):
        opt.load('compiler_cxx')
def configure(cnf):
        cnf.load('compiler_cxx')
        cnf.check(features='cxx cxxprogram', lib=['usb-1.0'], uselib_store='LIBUSB')
def build(bld):
        bld(features='cxx cxxprogram', source='main.cpp', target='app', cxxflags=['-Wall', '-std=gnu++0x'], use=['LIBUSB'])
