cmake_minimum_required (VERSION 3.13)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package (vsomeip3 3.4.10 REQUIRED)
find_package( Boost 1.55 COMPONENTS system thread log REQUIRED )

include_directories (
    ${Boost_INCLUDE_DIR}
    ${VSOMEIP_INCLUDE_DIRS}
)

add_executable(response-sample-170 src/response-sample-170.cpp)
target_link_libraries(response-sample-170 vsomeip3 ${Boost_LIBRARIES})

add_executable(request-sample-170 src/request-sample-170.cpp)
target_link_libraries(request-sample-170 vsomeip3 ${Boost_LIBRARIES})

add_executable(response-sample-180 src/response-sample-180.cpp)
target_link_libraries(response-sample-180 vsomeip3 ${Boost_LIBRARIES})

add_executable(request-sample-180 src/request-sample-180.cpp)
target_link_libraries(request-sample-180 vsomeip3 ${Boost_LIBRARIES})

add_executable(proxy src/proxy.cpp)
target_link_libraries(proxy vsomeip3 ${Boost_LIBRARIES})

add_executable(service src/service.cpp)
target_link_libraries(service vsomeip3 ${Boost_LIBRARIES})

add_executable(RoutingManagerClient src/RoutingManagerClient.cpp)
target_link_libraries(RoutingManagerClient vsomeip3 ${Boost_LIBRARIES})

add_executable(RoutingManagerService src/RoutingManagerService.cpp)
target_link_libraries(RoutingManagerService vsomeip3 ${Boost_LIBRARIES})