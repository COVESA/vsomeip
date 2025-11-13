cmake_minimum_required(VERSION 3.13)
project(HelloWorld)

# Add custom CMake modules (for FindDBus1.cmake if needed)
# --- Find D-Bus using pkg-config BEFORE finding CommonAPI-DBus ---
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# --- Now find CommonAPI packages ---
find_package(CommonAPI REQUIRED)
find_package(CommonAPI-DBus REQUIRED)
if(DBus1_FOUND)
  link_directories(${DBus1_LIBRARY_DIRS})
endif()

# --- Boost ---
find_package(Boost 1.54 COMPONENTS system thread log REQUIRED)

set(PRJ_NAME HelloWorld)
set(CMAKE_VERBOSE_MAKEFILE on)

# --- Project options and settings ---
option(USE_FILE "Set to OFF to disable file logging" OFF)
option(USE_CONSOLE "Set to OFF to disable console logging" OFF)
set(MAX_LOG_LEVEL "DEBUG" CACHE STRING "maximum log level")
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build, options are: Debug Release." FORCE)

add_definitions(-DCOMMONAPI_LOGLEVEL=COMMONAPI_LOGLEVEL_${MAX_LOG_LEVEL})
if(USE_FILE)
  add_definitions(-DUSE_FILE)
endif()
if(USE_CONSOLE)
  add_definitions(-DUSE_CONSOLE)
endif()

# --- Source Files ---
set(PRJ_SRC_PATH src)
set(PRJ_SRC_GEN_PATH src-gen)
set(PRJ_SRC_GEN_COMMONAPI_PATH ${PRJ_SRC_GEN_PATH}/core/v0/commonapi/examples)
set(PRJ_SRC_GEN_COMMONAPI_DBUS_PATH ${PRJ_SRC_GEN_PATH}/dbus/v0/commonapi/examples)

set(PRJ_NAME_CLIENT ${PRJ_NAME}Client)
set(PRJ_NAME_SERVICE ${PRJ_NAME}Service)

file(GLOB PRJ_PROXY_GEN_SRCS ${PRJ_SRC_GEN_COMMONAPI_PATH}/*Proxy.cpp)
file(GLOB PRJ_STUB_GEN_SRCS ${PRJ_SRC_GEN_COMMONAPI_PATH}/*Stub*.cpp)
file(GLOB PRJ_STUB_IMPL_SRCS ${PRJ_SRC_PATH}/*Stub*.cpp)
set(PRJ_CLIENT_SRCS ${PRJ_SRC_PATH}/${PRJ_NAME_CLIENT}.cpp ${PRJ_PROXY_GEN_SRCS})
set(PRJ_SERVICE_SRCS ${PRJ_SRC_PATH}/${PRJ_NAME_SERVICE}.cpp ${PRJ_SRC_PATH}/${PRJ_NAME}StubImpl.cpp ${PRJ_STUB_GEN_SRCS} ${PRJ_STUB_IMPL_SRCS})

file(GLOB PRJ_DBUS_LIB_SRCS ${PRJ_SRC_GEN_COMMONAPI_DBUS_PATH}/*.cpp)

# --- Build Targets ---
add_executable(${PRJ_NAME_CLIENT} ${PRJ_CLIENT_SRCS})
add_executable(${PRJ_NAME_SERVICE} ${PRJ_SERVICE_SRCS})
add_library(${PRJ_NAME}-dbus SHARED ${PRJ_DBUS_LIB_SRCS})

# --- Link Libraries ---
target_link_libraries(${PRJ_NAME_CLIENT} CommonAPI CommonAPI-DBus ${Boost_LIBRARIES} ${DBUS1_LIBRARIES})
target_link_libraries(${PRJ_NAME_SERVICE} CommonAPI CommonAPI-DBus ${Boost_LIBRARIES} ${DBUS1_LIBRARIES})
target_link_libraries(${PRJ_NAME}-dbus CommonAPI-DBus ${Boost_LIBRARIES} ${DBUS1_LIBRARIES})

# --- Include directories for generated code ---
target_include_directories(${PRJ_NAME_CLIENT} PRIVATE src-gen/core src-gen/dbus)
target_include_directories(${PRJ_NAME_SERVICE} PRIVATE src-gen/core src-gen/dbus)
target_include_directories(${PRJ_NAME}-dbus PRIVATE src-gen/core src-gen/dbus)