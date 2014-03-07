cmake_minimum_required (VERSION 2.8)
project (vsomeip)
set (CMAKE_VERBOSE_MAKEFILE off)

set (CMAKE_CXX_FLAGS "-DUSE_VSOMEIP_STATISTICS -DBOOST_LOG_DYN_LINK -std=c++0x -O0")
include_directories("include")

file(GLOB vsomeip-base_SRC
    "src/vsomeip.base/*.cpp"
)

add_library(vsomeip-base SHARED ${vsomeip-base_SRC})

file (GLOB vsomeip-service_discovery_SRC
    "src/vsomeip.service_discovery/*.cpp"
)

add_library(vsomeip-service_discovery SHARED ${vsomeip-service_discovery_SRC})

# Daemon
file (GLOB vsomeipd_SRC
    "src/vsomeip.daemon/*.cpp"
)

# Daemon
add_executable(vsomeipd ${vsomeipd_SRC})
target_link_libraries(vsomeipd vsomeip-service_discovery vsomeip-base boost_system boost_thread boost_program_options boost_log)

# Test executables
add_executable(application-sample src/vsomeip.examples/application_sample.cpp)
target_link_libraries(application-sample vsomeip-base boost_system boost_thread boost_program_options boost_program_options boost_log)