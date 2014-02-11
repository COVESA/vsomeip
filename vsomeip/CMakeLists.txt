cmake_minimum_required (VERSION 2.8)
project (vsomeip)

set (CMAKE_CXX_FLAGS "-DUSE_VSOMEIP_STATISTICS -DBOOST_LOG_DYN_LINK -std=c++0x -O0")
include_directories("include")

file(GLOB vsomeip-base_SRC
    "src/base/*.cpp"
)

add_library(vsomeip-base SHARED ${vsomeip-base_SRC})

file (GLOB vsomeip-service_discovery_SRC
    "src/service_discovery/*.cpp"
)

add_library(vsomeip-service_discovery SHARED ${vsomeip-service_discovery_SRC})

# Daemon
file (GLOB vsomeipd_SRC
    "src/daemon/*.cpp"
)

add_executable(vsomeipd ${vsomeipd_SRC})
target_link_libraries(vsomeipd vsomeip-service_discovery vsomeip-base boost_system boost_program_options boost_log_setup boost_log)

# Test executables
add_executable(vsomeip-serialization src/examples/serialization.cpp)
target_link_libraries(vsomeip-serialization vsomeip-base vsomeip-service_discovery boost_system)

add_executable(vsomeip-ip_client src/examples/ip_client.cpp)
target_link_libraries(vsomeip-ip_client vsomeip-base boost_system boost_thread)

add_executable(vsomeip-ip_service src/examples/ip_service.cpp)
target_link_libraries(vsomeip-ip_service vsomeip-base boost_system boost_thread)

add_executable(vsomeip-ip_resync_client src/examples/ip_resync_client.cpp)
target_link_libraries(vsomeip-ip_resync_client vsomeip-base boost_system boost_thread)

add_executable(vsomeip-ip_threaded_client src/examples/ip_threaded_client.cpp)
target_link_libraries(vsomeip-ip_threaded_client vsomeip-base boost_system boost_thread)

add_executable(vsomeip-ip_service_discovery_client src/examples/ip_service_discovery_client.cpp)
target_link_libraries(vsomeip-ip_service_discovery_client vsomeip-service_discovery vsomeip-base boost_system boost_thread)