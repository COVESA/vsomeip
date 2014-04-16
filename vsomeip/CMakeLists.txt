cmake_minimum_required (VERSION 2.8)
project (vsomeip)
set (CMAKE_VERBOSE_MAKEFILE off)

# OS
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(OperatingSystem "LINUX")
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

if (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
	set(OperatingSystem "FREEBSD")
endif (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")

set (CMAKE_CXX_FLAGS "-D${OperatingSystem} -DUSE_VSOMEIP_STATISTICS -DBOOST_LOG_DYN_LINK -g -std=c++0x -O0 -Wno-deprecated-register")
include_directories("include")

# Boost
find_package( Boost 1.54 COMPONENTS system thread program_options log REQUIRED )
include_directories( ${Boost_INCLUDE_DIR} )

# Base library
file(GLOB vsomeip_SRC
    "src/vsomeip/*.cpp"
)

add_library(vsomeip SHARED ${vsomeip_SRC})

file (GLOB vsomeip-sd_SRC
    "src/vsomeip.sd/*.cpp"
)

add_library(vsomeip-sd SHARED ${vsomeip-sd_SRC})

# Daemon
file (GLOB vsomeipd_SRC
    "src/vsomeip.daemon/*.cpp"
)

# Daemon
add_executable(vsomeipd ${vsomeipd_SRC})
target_link_libraries(vsomeipd vsomeip ${Boost_LIBRARIES} rt dl)

# Test executables
add_executable(application_sample src/vsomeip.examples/application_sample.cpp)
target_link_libraries(application_sample vsomeip ${Boost_LIBRARIES} dl)

add_executable(client_sample src/vsomeip.examples/client_sample.cpp)
target_link_libraries(client_sample vsomeip ${Boost_LIBRARIES} dl)

add_executable(service_sample src/vsomeip.examples/service_sample.cpp)
target_link_libraries(service_sample vsomeip ${Boost_LIBRARIES} dl)

add_executable(external-client_sample src/vsomeip.examples/external-client_sample.cpp)
target_link_libraries(external-client_sample vsomeip ${Boost_LIBRARIES} dl)

add_executable(external-service_sample src/vsomeip.examples/external-service_sample.cpp)
target_link_libraries(external-service_sample vsomeip ${Boost_LIBRARIES} dl)

add_executable(external-sd-client_sample src/vsomeip.examples/external-sd-client_sample.cpp)
target_link_libraries(external-sd-client_sample vsomeip-sd vsomeip ${Boost_LIBRARIES} dl)

add_executable(external-sd-service_sample src/vsomeip.examples/external-sd-service_sample.cpp)
target_link_libraries(external-sd-service_sample vsomeip-sd vsomeip ${Boost_LIBRARIES} dl)
