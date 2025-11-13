cmake_minimum_required (VERSION 3.13)

set (CMAKE_CXX_FLAGS "-g -std=c++0x")

find_package (vsomeip3 3.4.10 REQUIRED)
find_package( Boost 1.55 COMPONENTS system thread log REQUIRED )

include_directories (
    ${Boost_INCLUDE_DIR}
    ${VSOMEIP_INCLUDE_DIRS}
)

add_executable(response-sample src/response-sample.cpp)
target_link_libraries(response-sample vsomeip3 ${Boost_LIBRARIES})

add_executable(request-sample src/request-sample.cpp)
target_link_libraries(request-sample vsomeip3 ${Boost_LIBRARIES})