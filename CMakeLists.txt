cmake_minimum_required(VERSION 2.8)
project (nonolith-connect)

include(ExternalProject)

# Static
IF (STATIC OR (${CMAKE_SYSTEM_NAME} MATCHES "Windows"))
	set(Boost_USE_STATIC_LIBS   TRUE)
	set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
	set(CMAKE_EXE_LINK_DYNAMIC_CXX_FLAGS)
ENDIF()

# Boost
find_package(Boost COMPONENTS system regex thread chrono date_time REQUIRED)

# LibJSON
file(GLOB JSON_SRC libjson/_internal/Source/*.cpp)
add_library(json STATIC ${JSON_SRC})

# Libusb
ExternalProject_Add ( usb_nonolith_external
	SOURCE_DIR ${CMAKE_SOURCE_DIR}/libusb
	CONFIGURE_COMMAND ${CMAKE_SOURCE_DIR}/libusb/configure --host=${CROSS} --prefix=${CMAKE_BINARY_DIR}/libusb
	BUILD_COMMAND make
)
add_library(usb_nonolith UNKNOWN IMPORTED) 
set_property(TARGET usb_nonolith PROPERTY IMPORTED_LOCATION 
	${CMAKE_BINARY_DIR}/libusb/lib/libusb-1.0.a
 )
add_dependencies(usb_nonolith usb_nonolith_external)

# Websocketpp
set(WEBSOCKETPP_SRC
	websocketpp/src/websocket_server_session.cpp
	websocketpp/src/websocket_session.cpp
	websocketpp/src/websocket_server.cpp
	websocketpp/src/websocket_frame.cpp
    websocketpp/src/network_utilities.cpp
    websocketpp/src/sha1/sha1.cpp
    websocketpp/src/base64/base64.cpp
)
add_library(websocketpp STATIC ${WEBSOCKETPP_SRC})

# Nonolith Connect

include_directories ("${PROJECT_SOURCE_DIR}/libusb")
include_directories ("${PROJECT_SOURCE_DIR}/websocketpp/src")

SET (SRCS 
	bootloader/bootloader.cpp
	cee/cee.cpp
	m1000/m1000.cpp
	device.cpp
	event.cpp
	rest_service.cpp
	server.cpp
	streaming_device/output_source.cpp
	streaming_device/rest_api.cpp
	streaming_device/streaming_device.cpp
	streaming_device/stream_listener.cpp
	streaming_device/ws_api.cpp
	url.cpp
	usb.cpp
	version.cpp
	websocket_service.cpp
)

IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	SET (PLATFORM_LIBRARIES
		-lrt
		-lpthread
		-ludev
	)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")

IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	SET (PLATFORM_LIBRARIES
		-lobjc
	)
	add_definitions (
		-framework CoreFoundation
		-framework IOKit
	)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	SET (PLATFORM_LIBRARIES
		-static
		-static-libgcc
		-static-libstdc++
		-lws2_32
		-lmswsock
		-Wl,-subsystem,windows
		-mwindows
	)
	add_definitions (
		-D WIN32_LEAN_AND_MEAN
		-DBOOST_THREAD_USE_LIB
	)
	SET (CMAKE_EXE_LINKER_FLAGS
		-Wl,--allow-multiple-definition
	)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")

add_executable(nonolith-connect ${SRCS})
target_link_libraries(nonolith-connect 
	websocketpp
	${Boost_SYSTEM_LIBRARY}
	${Boost_REGEX_LIBRARY}
	${Boost_THREAD_LIBRARY}
	${Boost_CHRONO_LIBRARY}
	${Boost_DATE_TIME_LIBRARY}
	json
	usb_nonolith
	${PLATFORM_LIBRARIES}
)
