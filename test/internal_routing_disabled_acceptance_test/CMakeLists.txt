cmake_minimum_required(VERSION 3.4...3.22)

# Workaround for version range in cmake_minimum_required() before 3.12
if(${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

project(internal_routing_disabled_acceptance_test LANGUAGES CXX)

add_compile_options(-pedantic -Wall -Wconversion -Wextra)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)

add_executable(${PROJECT_NAME} applet.cpp client.cpp server.cpp main.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE ${gtest_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} PRIVATE gtest Threads::Threads ${VSOMEIP_NAME} Boost::system)

enable_testing()
add_test(NAME ${PROJECT_NAME} COMMAND $<TARGET_FILE:${PROJECT_NAME}>)
add_dependencies(build_network_tests ${PROJECT_NAME})
configure_file(vsomeip.json vsomeip.json COPYONLY)
set_property(
    TEST ${PROJECT_NAME}
    APPEND PROPERTY ENVIRONMENT
    "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:${VSOMEIP_NAME}>"
    "VSOMEIP_CONFIGURATION=${CMAKE_CURRENT_BINARY_DIR}/vsomeip.json"
)
set_property(
    TEST ${PROJECT_NAME}
    APPEND PROPERTY TIMEOUT
    60
)
