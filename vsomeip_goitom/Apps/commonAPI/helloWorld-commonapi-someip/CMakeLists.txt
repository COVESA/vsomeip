cmake_minimum_required(VERSION 3.13)
project(World)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CommonAPI REQUIRED)
find_package(CommonAPI-SomeIP REQUIRED)
find_package(vsomeip3 REQUIRED)
find_package(Boost 1.54 COMPONENTS system thread log REQUIRED)

set(CMAKE_VERBOSE_MAKEFILE ON)

# --- v0 paths ---
set(V0_CORE_PATH src-gen/core/v0/commonapi/examples)
set(V0_SOMEIP_PATH src-gen/someip/v0/commonapi/examples)
set(V0_SRC_PATH src/v0)

# --- v1 and v2paths ---
set(V1_CORE_PATH src-gen/core/v1/commonapi/examples)
set(V1_SOMEIP_PATH src-gen/someip/v1/commonapi/examples)
set(V1_SRC_PATH src/v1)
set(V2_SRC_PATH src/v2)

# --- v0 sources ---
file(GLOB V0_CORE_SRCS ${V0_CORE_PATH}/*.cpp)
file(GLOB V0_SOMEIP_SRCS ${V0_SOMEIP_PATH}/*.cpp)
file(GLOB V0_SRC_SRCS ${V0_SRC_PATH}/*.cpp)

# --- v1 and v2sources ---
file(GLOB V1_CORE_SRCS ${V1_CORE_PATH}/*.cpp)
file(GLOB V1_SOMEIP_SRCS ${V1_SOMEIP_PATH}/*.cpp)
file(GLOB V1_SRC_SRCS ${V1_SRC_PATH}/*.cpp)
file(GLOB V2_SRC_SRCS ${V2_SRC_PATH}/*.cpp)

# --- v0 executables ---
add_executable(WorldClient_v0 ${V0_SRC_PATH}/WorldClient.cpp ${V0_CORE_SRCS} ${V0_SOMEIP_SRCS})
#add_executable(WorldService_v0 ${V0_SRC_PATH}/WorldService.cpp ${V0_CORE_SRCS} ${V0_SOMEIP_SRCS})
add_executable(WorldService_v0
    src/v0/WorldService.cpp
    src/v0/WorldStubImpl.cpp
    src/v0/BadServiceServiceIDStubImpl.cpp
    src/v0/BadServiceInstanceIDStubImpl.cpp
    ${V0_CORE_SRCS}
    ${V0_SOMEIP_SRCS}
)

# --- v1 executables ---
add_executable(WorldClient_v1 ${V1_SRC_PATH}/subscribe_sample.cpp ${V1_CORE_SRCS} ${V1_SOMEIP_SRCS})
#add_executable(WorldService_v1 ${V1_SRC_PATH}/notify_sample.cpp ${V1_CORE_SRCS} ${V1_SOMEIP_SRCS})
add_executable(WorldService_v1
    src/v1/notify_sample.cpp
    src/v1/EventsAndFieldsStubImpl.cpp
    ${V1_CORE_SRCS}
    ${V1_SOMEIP_SRCS}
)

# --- v1 executables ---
add_executable(WorldClient_v1_1 ${V2_SRC_PATH}/EventClient.cpp ${V1_CORE_SRCS} ${V1_SOMEIP_SRCS})
#add_executable(WorldService_v1 ${V1_SRC_PATH}/notify_sample.cpp ${V1_CORE_SRCS} ${V1_SOMEIP_SRCS})
add_executable(WorldService_v1_1
    src/v2/EventService.cpp
    src/v2/EventStubImpl.cpp
    ${V1_CORE_SRCS}
    ${V1_SOMEIP_SRCS}
)

# --- Link libraries ---
foreach(target IN ITEMS WorldClient_v0 WorldService_v0 WorldClient_v1 WorldService_v1 WorldClient_v1_1 WorldService_v1_1)
    target_link_libraries(${target} CommonAPI CommonAPI-SomeIP vsomeip3 ${Boost_LIBRARIES} pthread)
    target_include_directories(${target} PRIVATE src-gen/core src-gen/someip src)
    target_compile_options(${target} PRIVATE -Wall -Wextra -g)
endforeach()

# --- Optionally, install targets ---
install(TARGETS WorldClient_v0 WorldService_v0 WorldClient_v1 WorldService_v1 WorldClient_v1_1 WorldService_v1_1 RUNTIME DESTINATION bin)