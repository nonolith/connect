cmake_minimum_required(VERSION 2.8.9)
project (nonolith-connect)

include(ExternalProject)

# Boost
find_package(Boost COMPONENTS system regex thread REQUIRED)

# LibJSON
file(GLOB JSON_SRC libjson/_internal/Source/*.cpp)
add_library(json STATIC ${JSON_SRC})

# Libusb
ExternalProject_Add ( usb_nonolith_external
	SOURCE_DIR ${CMAKE_SOURCE_DIR}/libusb
	CONFIGURE_COMMAND ${CMAKE_SOURCE_DIR}/libusb/configure --prefix=${CMAKE_BINARY_DIR}/libusb
	BUILD_COMMAND make
)
add_library(usb_nonolith UNKNOWN IMPORTED) 
set_property(TARGET usb_nonolith PROPERTY IMPORTED_LOCATION 
	${CMAKE_BINARY_DIR}/libusb/lib/libusb-1.0.a
 )
add_dependencies(usb_nonolith usb_nonolith_external)

# Websocketpp
ExternalProject_Add ( websocketpp_external
	SOURCE_DIR ${CMAKE_SOURCE_DIR}/websocketpp
	CONFIGURE_COMMAND ""
	BUILD_IN_SOURCE 1
	BUILD_COMMAND make
	INSTALL_COMMAND make install prefix=${CMAKE_BINARY_DIR}/websocketpp
)
add_library(websocketpp UNKNOWN IMPORTED) 
set_property(TARGET websocketpp PROPERTY IMPORTED_LOCATION 
	${CMAKE_BINARY_DIR}/websocketpp/lib
 )
add_dependencies(usb_nonolith websocketpp_external)

# Qt

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

find_package(Qt5Widgets)
find_package(Qt5WebKitWidgets)


# Nonolith Connect
include_directories ("${PROJECT_SOURCE_DIR}/libusb")
include_directories ("${PROJECT_SOURCE_DIR}/websocketpp/src")

SET (SRCS 
	main.cpp
	bootloader/bootloader.cpp
	cee/cee.cpp
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
	appshell/ui.cpp
	appshell/bridge_client.cpp
	appshell/html5applicationviewer/html5applicationviewer.cpp
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
		-l ws2_32
		-l mswsock
	)
	add_definitions (
		-Wl,-subsystem,windows
		-mwindows
	)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")

add_executable(nonolith-connect ${SRCS})
target_link_libraries(nonolith-connect 
	${PLATFORM_LIBRARIES}
	${Boost_SYSTEM_LIBRARY}
	${Boost_REGEX_LIBRARY}
	${Boost_THREAD_LIBRARY}
	json
	usb_nonolith
	websocketpp
)

qt5_use_modules(nonolith-connect Widgets WebKitWidgets)
