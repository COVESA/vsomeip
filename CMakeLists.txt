# Copyright (C) 2015-2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG) This Source Code Form is subject to the
# terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed with this file, You can obtain
# one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.13)

if(POLICY CMP0177)
  # Non-normalized paths in install, see comment below
  cmake_policy(SET CMP0177 OLD)
endif()
if(POLICY CMP0167)
  # Use CMake's FindBoost module, see comment below.  Needed until minimum
  # boost version is >=1.70
  cmake_policy(SET CMP0167 NEW)
endif()
if(POLICY CMP0144)
  # find_package uses upper-case <PACKAGENAME>_ROOT variables
  cmake_policy(SET CMP0144 NEW)
endif()

project(vsomeip)

set(VSOMEIP_NAME vsomeip3)
set(VSOMEIP_COMPAT_NAME vsomeip)

set(VSOMEIP_MAJOR_VERSION 3)
set(VSOMEIP_MINOR_VERSION 5)
set(VSOMEIP_PATCH_VERSION 6)
set(VSOMEIP_HOTFIX_VERSION 0)

set(VSOMEIP_VERSION ${VSOMEIP_MAJOR_VERSION}.${VSOMEIP_MINOR_VERSION}.${VSOMEIP_PATCH_VERSION})
set(PACKAGE_VERSION ${VSOMEIP_VERSION}) # Used in documentation/doxygen.in

if(${VSOMEIP_HOTFIX_VERSION} EQUAL 0)
  add_definitions(-DVSOMEIP_VERSION="${VSOMEIP_VERSION}")
else()
  add_definitions(-DVSOMEIP_VERSION="${VSOMEIP_VERSION}.${VSOMEIP_HOTFIX_VERSION}")
endif()

if(NOT GTEST_ROOT)
  if(DEFINED ENV{GTEST_ROOT})
    set(GTEST_ROOT $ENV{GTEST_ROOT})
  else()
    set(GTEST_ROOT
        "n/a"
        CACHE STRING "Path to root folder of googletest. Must be set for building the tests."
    )
  endif()
endif()

include(GNUInstallDirs)

# Offer the user the choice of overriding the installation directories
set(INSTALL_LIB_DIR
    ${CMAKE_INSTALL_LIBDIR}
    CACHE PATH "Installation directory for libraries"
)
set(INSTALL_BIN_DIR
    ${CMAKE_INSTALL_BINDIR}
    CACHE PATH "Installation directory for executables"
)
set(INSTALL_INCLUDE_DIR
    ${CMAKE_INSTALL_INCLUDEDIR}
    CACHE PATH "Installation directory for header files"
)

if(WIN32 AND NOT CYGWIN)
  set(DEF_INSTALL_CMAKE_DIR CMake)
else()
  set(DEF_INSTALL_CMAKE_DIR lib/cmake/${VSOMEIP_NAME})
endif()

set(INSTALL_CMAKE_DIR
    ${DEF_INSTALL_CMAKE_DIR}
    CACHE PATH "Installation directory for CMake files"
)

# Make relative paths absolute (needed later on)
foreach(
  p
  LIB
  BIN
  INCLUDE
  CMAKE
)
  set(var INSTALL_${p}_DIR)
  if(NOT IS_ABSOLUTE "${${var}}")
    set(ABSOLUTE_${var} "${CMAKE_INSTALL_PREFIX}/${${var}}") # Add all targets to the build-tree export set
  endif()
endforeach()

# Set a default build type if none was specified
# This is a CMake anti-pattern.
set(default_build_type "RelWithDebInfo")
if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE
      "${default_build_type}"
      CACHE STRING "Choose the type of build." FORCE
  )
  # Set the possible values of build type for cmake-gui
  set_property(
    CACHE CMAKE_BUILD_TYPE
    PROPERTY STRINGS
             "Debug"
             "Release"
             "MinSizeRel"
             "RelWithDebInfo"
  )
endif()

set(CMAKE_CXX_STANDARD 17)

# Threads
find_package(Threads REQUIRED)

# OS
add_library(OS_INTERFACE INTERFACE IMPORTED)
add_library(DL_INTERFACE INTERFACE IMPORTED)

# Boost: once this is pushed passed 1.70, it should be found using CONFIG rather
# than CMake's FindBoost module, see policy CMP0167
find_package(
  Boost 1.66 REQUIRED
  COMPONENTS system thread filesystem
)

include(CMakeDependentOption)
cmake_dependent_option(
  ENABLE_WERROR
  "Enable -Werror flag"
  ON
  "CMAKE_SYSTEM_NAME MATCHES LINUX"
  OFF
)
if(ENABLE_WERROR)
  message(STATUS "Enabling -Werror flag")
  add_compile_options(-Werror)
endif()

# Variable to store compiler options used to export settings.  These are stored
# in a variable such that they can be easily removed in test apps (see
# test/CMakeLists.txt)
set(EXPORTSYMBOLS)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  set(OS "LINUX")
  target_link_libraries(DL_INTERFACE INTERFACE dl)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  if(NOT DEFINED _FORTIFY_SOURCE)
    set(_FORTIFY_SOURCE 2)
  endif()
  add_definitions(-D_GLIBCXX_USE_NANOSLEEP -D_FORTIFY_SOURCE=${_FORTIFY_SOURCE})
  add_compile_options(
    -Wall
    -Wextra
    -Wformat
    -Wformat-security
    -Wconversion
    -Wformat
    -Werror=format-security
    -Wpedantic
    -fexceptions
    -fstrict-aliasing
    -fstack-protector-strong
    -fasynchronous-unwind-tables
    -fno-omit-frame-pointer
    -Wno-inconsistent-missing-override
  )

  target_link_libraries(OS_INTERFACE INTERFACE Threads::Threads)

  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # This is only relevant for GCC and causes warnings on Clang

    set(EXPORTSYMBOLS
      -Wl,-export-dynamic
      -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exportmap.gcc
    )

    target_link_options(
      OS_INTERFACE
      INTERFACE
      -Wl,-z,relro,-z,now
      ${EXPORTSYMBOLS}
    )

    # There appear to be complication issues without this flag
    target_link_options(OS_INTERFACE INTERFACE -Wno-tsan)
  endif()

  # force all use of std::mutex and std::recursive_mutex to use runtime init instead of static initialization so mutexes
  # can be hooked to enable PI as needed
  add_definitions(-D_GTHREAD_USE_MUTEX_INIT_FUNC -D_GTHREAD_USE_RECURSIVE_MUTEX_INIT_FUNC)

elseif(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
  set(OS "FREEBSD")
  add_compile_options(-Wno-deprecated)
  target_link_libraries(OS_INTERFACE INTERFACE Threads::Threads)

elseif(${CMAKE_SYSTEM_NAME} MATCHES "QNX")
  set(OS "QNX")
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  add_compile_options(
    -fexceptions
    -fstrict-aliasing
    -fstack-protector
    -fasynchronous-unwind-tables
    -fno-omit-frame-pointer
  )
  target_link_options(OS_INTERFACE INTERFACE -Wl,-export-dynamic)

  target_link_libraries(OS_INTERFACE INTERFACE slog2)

  # QNX specific socket lib
  set(VSOMEIP_SOCKET_LIB
      "socket"
      CACHE STRING "Default socket library for QNX"
  )
  target_link_libraries(OS_INTERFACE INTERFACE $<$<PLATFORM_ID:QNX>:${VSOMEIP_SOCKET_LIB}>)

endif()

if(${OS})
  add_definitions(-D${OS})
endif()

#
# Options

find_package(PkgConfig)

# DLT
option(DISABLE_DLT "Disable DLT support" OFF)
if(NOT ${DISABLE_DLT})
  pkg_check_modules(DLT "automotive-dlt >= 2.11" IMPORTED_TARGET)
  if(DLT_FOUND)
    message(STATUS "automotive-dlt found and will be linked")
    add_compile_definitions(USE_DLT)
  endif()
else()
  message(STATUS "Not linking automotive-dlt")
endif()

# Signal handling
option(ENABLE_SIGNAL_HANDLING "Enable signal handling (SIGTERM, SIGINT, etc) in applicaiton_impl" OFF)
if(ENABLE_SIGNAL_HANDLING)
  add_definitions(-DVSOMEIP_ENABLE_SIGNAL_HANDLING)
endif()

# Event caching
option(ENABLE_DEFAULT_EVENT_CACHING "Enable default event caching")

if(NOT MSVC)
  # Sanitizers

  if(ENABLE_UNDEFINED_SANITIZER)
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
  endif()

  if(ENABLE_THREAD_SANITIZER)
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
  endif()

  if(ENABLE_LEAK_SANITIZER)
    add_compile_options(-fsanitize=leak)
    add_link_options(-fsanitize=leak)
  endif()

  if(ENABLE_ADDRESS_SANITIZER)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
  endif()

  if(ENABLE_PROFILING)
    add_compile_options(-pg)
  endif()

  # Valgrind
  find_program(VALGRIND "valgrind")
  if(VALGRIND)
    if(DEFINED VALGRIND_TYPE
       AND NOT
           VALGRIND_TYPE
           STREQUAL
           ""
    )
      set(TEST_ENTRYPOINT ${VALGRIND} --tool=${VALGRIND_TYPE} --trace-children=yes)
      add_definitions(-DBOOST_USE_VALGRIND)
    endif()

      if(VALGRIND_TYPE STREQUAL "massif")
        set(TEST_ENTRYPOINT ${TEST_ENTRYPOINT} --massif-out-file=${VALGRIND_LOGS_DIR}/test_name.out)
     endif()

     if(VALGRIND_TYPE STREQUAL "memcheck")
      set(TEST_ENTRYPOINT
          ${TEST_ENTRYPOINT}
          --leak-check=yes
          --suppressions=${VALGRIND_SUPPRESS_FILE}
          --log-file=${VALGRIND_LOGS_DIR}/test_name.out
      )
    endif()
  endif()
endif()

# Compatibility
option(ENABLE_COMPAT "Enable compatibility layer" OFF)

# Multiple routing managers
option(ENABLE_MULTIPLE_ROUTING_MANAGERS "Enable multiple routing managers" OFF)

# Security / Policy handling
option(DISABLE_SECURITY "Disable security features")
if(DISABLE_SECURITY)
  add_definitions(-DVSOMEIP_DISABLE_SECURITY)
endif()

# Suppress deprecation warnings for vSomeIP interfaces
add_definitions(-DVSOMEIP_INTERNAL_SUPPRESS_DEPRECATED)

#
# Dependencies

# cmake 3.15 introduced a new variable and a new format for the old one
if(DEFINED Boost_VERSION_MACRO)
  set(VSOMEIP_BOOST_VERSION ${Boost_VERSION_MACRO})
else()
  set(VSOMEIP_BOOST_VERSION ${Boost_VERSION})
endif()

message(STATUS "Using boost version: ${VSOMEIP_BOOST_VERSION}")

add_definitions(-DVSOMEIP_BOOST_VERSION=${VSOMEIP_BOOST_VERSION})

# SystemD
pkg_check_modules(SystemD "libsystemd")

if(NOT SystemD_FOUND OR ${CMAKE_SYSTEM_NAME} MATCHES "Android")
  message(STATUS "Systemd was not found, watchdog disabled!")
  add_definitions(-DWITHOUT_SYSTEMD)
else()
  target_link_libraries(OS_INTERFACE INTERFACE ${SystemD_LIBRARIES})
endif()

#
# Directories

if(MSVC)
  message("using MSVC Compiler")
  # add_definitions(-DVSOMEIP_DLL_COMPILATION) now it is controlled per target
  set(BOOST_WINDOWS_VERSION
      "0x600"
      CACHE
        STRING
        "Set the same Version as the Version with which Boost was built, otherwise there will be errors. (normaly 0x600 is for Windows 7 and 0x501 is for Windows XP)"
  )
  # Disable warning C4250 since it warns that the compiler is correctly following the C++ Standard. It's a
  # "We-Are-Doing-Things-By-The-Book" notice, not a real warning.
  add_compile_options(/EHsc /wd4250)
  add_definitions(
    -D_CRT_SECURE_NO_WARNINGS
    -D_SCL_SECURE_NO_WARNINGS
    -D_WINSOCK_DEPRECATED_NO_WARNINGS
    -D_WIN32_WINNT=${BOOST_WINDOWS_VERSION}
    -DWIN32
    -DBOOST_ASIO_DISABLE_IOCP
  )
  link_directories(${Boost_LIBRARY_DIR_DEBUG})
  add_compile_options(/MD$<$<CONFIG:Debug>:d>)
endif()

#
# Configuration library

file(GLOB vsomeip_cfg_SRC "implementation/configuration/src/*.cpp")
list(SORT vsomeip_cfg_SRC)
if(NOT ENABLE_MULTIPLE_ROUTING_MANAGERS)
  add_library(${VSOMEIP_NAME}-cfg SHARED)
  target_sources(${VSOMEIP_NAME}-cfg PRIVATE ${vsomeip_cfg_SRC})
  set_target_properties(
    ${VSOMEIP_NAME}-cfg PROPERTIES
    VERSION ${VSOMEIP_VERSION}
    SOVERSION ${VSOMEIP_MAJOR_VERSION}
    OUTPUT_NAME ${VSOMEIP_NAME}-cfg
    EXPORT_NAME ${VSOMEIP_NAME}-cfg
  )
  target_compile_features(${VSOMEIP_NAME}-cfg PRIVATE cxx_std_17)
  target_compile_definitions(${VSOMEIP_NAME}-cfg PUBLIC $<$<CXX_COMPILER_ID:MSVC>:VSOMEIP_DLL_COMPILATION_PLUGIN>)

  target_link_libraries(
    ${VSOMEIP_NAME}-cfg
    PRIVATE ${VSOMEIP_NAME}
            Boost::filesystem
            Boost::thread
            DL_INTERFACE
            OS_INTERFACE
  )
else()
  # We don't want this target to only conditionally exist.
  add_library(${VSOMEIP_NAME}-cfg INTERFACE)
  message(STATUS "Multiple routing managers enabled, \"disabling\" the
  configuration library.  Note that the unittests depend on this library so you
  won't likely be able to use the tests in this mode"
  )
endif()

#
# Base library

file(
  GLOB
  ${VSOMEIP_NAME}_SRC
  "implementation/endpoints/src/*.cpp"
  "implementation/logger/src/*.cpp"
  "implementation/tracing/src/*.cpp"
  "implementation/message/src/*.cpp"
  "implementation/plugin/src/*.cpp"
  "implementation/protocol/src/*.cpp"
  "implementation/routing/src/*.cpp"
  "implementation/runtime/src/*.cpp"
  "implementation/security/src/*.cpp"
  "implementation/utility/src/*.cpp"
)
if(ENABLE_MULTIPLE_ROUTING_MANAGERS)
  list(APPEND ${VSOMEIP_NAME}_SRC "implementation/configuration/src/configuration_impl.cpp")
endif()
list(SORT ${VSOMEIP_NAME}_SRC)

if(WIN32)
  list(
    FILTER ${VSOMEIP_NAME}_SRC
           EXCLUDE
           REGEX
           ".*uds.*"
  )
endif()

add_library(${VSOMEIP_NAME} SHARED)
target_sources(${VSOMEIP_NAME} PRIVATE ${${VSOMEIP_NAME}_SRC})
target_compile_features(${VSOMEIP_NAME} PRIVATE cxx_std_17)
target_include_directories(
  ${VSOMEIP_NAME}
  PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/interface>
         $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/implementation/configuration/include>
  INTERFACE $<INSTALL_INTERFACE:${INSTALL_INCLUDE_DIR}>
)
if(MSVC)
  set_target_properties(
    ${VSOMEIP_NAME}
    PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS
    ON
  )
else()
  set_target_properties(${VSOMEIP_NAME} PROPERTIES LINK_FLAGS "-Wl,-wrap,socket -Wl,-wrap,accept -Wl,-wrap,open")
endif()
target_compile_definitions(
  ${VSOMEIP_NAME}
  PRIVATE $<$<CXX_COMPILER_ID:MSVC>:VSOMEIP_DLL_COMPILATION>
          $<$<BOOL:${ENABLE_MULTIPLE_ROUTING_MANAGERS}>:VSOMEIP_ENABLE_MULTIPLE_ROUTING_MANAGERS>
          $<$<BOOL:${ENABLE_DEFAULT_EVENT_CACHING}>:VSOMEIP_ENABLE_DEFAULT_EVENT_CACHING>
)
target_include_directories(${VSOMEIP_NAME} SYSTEM PUBLIC $<BUILD_INTERFACE:${Boost_INCLUDE_DIRS}>)
set_target_properties(
  ${VSOMEIP_NAME} PROPERTIES
  SOVERSION ${VSOMEIP_MAJOR_VERSION}
  VERSION ${VSOMEIP_VERSION}
  OUTPUT_NAME ${VSOMEIP_NAME}
  EXPORT_NAME ${VSOMEIP_NAME}
)

# PRIVATE means the listed libraries won't be included in the "link interface", meaning the exported
# ${VSOMEIP_NAME}Targets.cmake targets won't try to link against them (which shouldn't be required). Boost library
# includes absolute build host paths as of writing, which also makes this important as it breaks the build.
target_link_libraries(
  ${VSOMEIP_NAME}
  PRIVATE Boost::filesystem
          Boost::thread
          $<TARGET_NAME_IF_EXISTS:PkgConfig::DLT>
          DL_INTERFACE
          OS_INTERFACE
          Threads::Threads
)

if(NOT WIN32)
  target_link_options(${VSOMEIP_NAME} PRIVATE "LINKER:-as-needed")
endif()

#
# Service Discovery library

file(GLOB vsomeip_sd_SRC "implementation/service_discovery/src/*.cpp")
list(SORT vsomeip_sd_SRC)

add_library(${VSOMEIP_NAME}-sd SHARED)
target_compile_features(${VSOMEIP_NAME}-sd PRIVATE cxx_std_17)
target_sources(${VSOMEIP_NAME}-sd PRIVATE ${vsomeip_sd_SRC})
set_target_properties(
  ${VSOMEIP_NAME}-sd PROPERTIES
  VERSION ${VSOMEIP_VERSION}
  SOVERSION ${VSOMEIP_MAJOR_VERSION}
  OUTPUT_NAME ${VSOMEIP_NAME}-sd
  EXPORT_NAME ${VSOMEIP_NAME}-sd
)
target_compile_definitions(${VSOMEIP_NAME}-sd PUBLIC $<$<CXX_COMPILER_ID:MSVC>:VSOMEIP_DLL_COMPILATION_PLUGIN>)
target_link_libraries(
  ${VSOMEIP_NAME}-sd
  PUBLIC ${VSOMEIP_NAME}
  PRIVATE DL_INTERFACE
          OS_INTERFACE
          Threads::Threads
)

#
# E2E library
file(GLOB_RECURSE vsomeip_e2e_SRC "implementation/e2e_protection/src/*.cpp")
list(SORT vsomeip_e2e_SRC)

add_library(${VSOMEIP_NAME}-e2e SHARED)
target_sources(${VSOMEIP_NAME}-e2e PRIVATE ${vsomeip_e2e_SRC})
set_target_properties(
  ${VSOMEIP_NAME}-e2e PROPERTIES
  VERSION ${VSOMEIP_VERSION}
  SOVERSION ${VSOMEIP_MAJOR_VERSION}
  OUTPUT_NAME ${VSOMEIP_NAME}-e2e
  EXPORT_NAME ${VSOMEIP_NAME}-e2e
)
target_compile_definitions(${VSOMEIP_NAME}-e2e PUBLIC $<$<CXX_COMPILER_ID:MSVC>:VSOMEIP_DLL_COMPILATION_PLUGIN>)
target_link_libraries(
  ${VSOMEIP_NAME}-e2e
  PUBLIC ${VSOMEIP_NAME}
  PRIVATE DL_INTERFACE OS_INTERFACE
)

#
# Compatibility library

if(ENABLE_COMPAT)
  set(VSOMEIP_COMPAT_MAJOR_VERSION 2)
  set(VSOMEIP_COMPAT_VERSION ${VSOMEIP_COMPAT_MAJOR_VERSION}.99.99)

  add_definitions(-DVSOMEIP_ENABLE_COMPAT)

  file(
    GLOB_RECURSE
    ${VSOMEIP_COMPAT_NAME}_SRC
    "implementation/compat/logging/src/*.cpp"
    "implementation/compat/message/src/*.cpp"
    "implementation/compat/runtime/src/*.cpp"
  )
  list(SORT ${VSOMEIP_COMPAT_NAME}_SRC)

  add_library(${VSOMEIP_COMPAT_NAME})
  target_sources(${VSOMEIP_COMPAT_NAME} SHARED ${${VSOMEIP_COMPAT_NAME}_SRC})
  target_compile_features(${VSOMEIP_COMPAT_NAME} PRIVATE cxx_std_17)
  target_compile_definitions(${VSOMEIP_COMPAT_NAME} PUBLIC $<$<CXX_COMPILER_ID:MSVC>:VSOMEIP_DLL_COMPILATION_PLUGIN>)
  set_target_properties(
    ${VSOMEIP_COMPAT_NAME} PROPERTIES
    VERSION ${VSOMEIP_COMPAT_VERSION}
    SOVERSION ${VSOMEIP_COMPAT_MAJOR_VERSION}
    OUTPUT_NAME ${VSOMEIP_COMPAT_NAME}
    EXPORT_NAME vsomeip-compat
  )
  target_include_directories(
    ${VSOMEIP_COMPAT_NAME}
    PUBLIC # Access to vsomeip3 includes
           $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/interface> # Access to vsomeip3 includes
           $<INSTALL_INTERFACE:include>
           $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/interface/compat>
           $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for generated files in build mode
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/implementation/configuration/include>
           $<INSTALL_INTERFACE:include/compat> # for clients in install mode
  )
  target_link_libraries(
    ${VSOMEIP_COMPAT_NAME}
    PRIVATE ${VSOMEIP_NAME}
            Boost::system
            Boost::thread
            DL_INTERFACE
            OS_INTERFACE
  )

endif()

#
# Configuration files

set(EXAMPLE_CONFIG_FILES
    "config/vsomeip.json"
    "config/vsomeip-local.json"
    "config/vsomeip-tcp-client.json"
    "config/vsomeip-tcp-service.json"
    "config/vsomeip-udp-client.json"
    "config/vsomeip-udp-service.json"
)

#
# Configuration parameters

if(${CMAKE_SYSTEM_NAME} MATCHES "QNX")
  set(VSOMEIP_BASE_PATH_DEFAULT "/var")
else()
  set(VSOMEIP_BASE_PATH_DEFAULT "/tmp")
endif()
set(VSOMEIP_BASE_PATH
    ${VSOMEIP_BASE_PATH_DEFAULT}
    CACHE PATH "Base path for vsomeip sockets"
)

set(VSOMEIP_DIAGNOSIS_ADDRESS "0x01")
if(DIAGNOSIS_ADDRESS)
  set(VSOMEIP_DIAGNOSIS_ADDRESS ${DIAGNOSIS_ADDRESS})
endif()

set(VSOMEIP_UNICAST_ADDRESS "127.0.0.1")
if(UNICAST_ADDRESS)
  set(VSOMEIP_UNICAST_ADDRESS ${UNICAST_ADDRESS})
endif()

set(VSOMEIP_ROUTING_READY_MESSAGE "SOME/IP routing ready.")
if(ROUTING_READY_MESSAGE)
  set(VSOMEIP_ROUTING_READY_MESSAGE ${ROUTING_READY_MESSAGE})
endif()

set(VSOMEIP_LOCAL_TCP_PORT_WAIT_TIME 100)
if(LOCAL_TCP_PORT_WAIT_TIME)
  set(VSOMEIP_LOCAL_TCP_PORT_WAIT_TIME ${LOCAL_TCP_PORT_WAIT_TIME})
endif()

set(VSOMEIP_LOCAL_TCP_PORT_MAX_WAIT_TIME 10000)
if(LOCAL_TCP_PORT_MAX_WAIT_TIME)
  if(${LOCAL_TCP_PORT_MAX_WAIT_TIME} GREATER ${LOCAL_TCP_PORT_WAIT_TIME})
    set(VSOMEIP_LOCAL_TCP_PORT_MAX_WAIT_TIME ${LOCAL_TCP_PORT_MAX_WAIT_TIME})
  else()
    set(VSOMEIP_LOCAL_TCP_PORT_MAX_WAIT_TIME ${LOCAL_TCP_PORT_WAIT_TIME})
  endif()
endif()

set(DEFAULT_CONFIGURATION_FOLDER
    "/etc/vsomeip"
    CACHE PATH "Default configuration folder"
)
message(STATUS "Default configuration folder: ${DEFAULT_CONFIGURATION_FOLDER}")

set(DEFAULT_CONFIGURATION_FILE
    "/etc/vsomeip.json"
    CACHE FILEPATH "Default configuration file"
)
message(STATUS "Default configuration file: ${DEFAULT_CONFIGURATION_FILE}")

message("Predefined base path: ${VSOMEIP_BASE_PATH}")
message("Predefined unicast address: ${VSOMEIP_UNICAST_ADDRESS}")
message("Predefined diagnosis address: ${VSOMEIP_DIAGNOSIS_ADDRESS}")
message("Predefined wait times for internal communication ports (TCP):\
 ${VSOMEIP_LOCAL_TCP_PORT_WAIT_TIME}\
 (max=${VSOMEIP_LOCAL_TCP_PORT_MAX_WAIT_TIME})"
)

#
# Installation

set(INCLUDE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/interface/vsomeip")

file(
  GLOB_RECURSE
  vsomeip_INCLUDE
  RELATIVE
  ${INCLUDE_PATH}
  "interface/*.h*"
)
list(SORT vsomeip_INCLUDE)

foreach(file ${vsomeip_INCLUDE})
  get_filename_component(dir ${file} DIRECTORY)
  # The destination contains non-normalized paths, this is deprecated.  Until
  # CMake 3.23 is the minimum version and FILE_SETs can be used, we're relying
  # on using the old CMP0177 policy.
  install(
    FILES "${INCLUDE_PATH}/${file}"
          DESTINATION "${INSTALL_INCLUDE_DIR}/vsomeip/${dir}"
          COMPONENT dev
  )
endforeach()

unset(INCLUDE_PATH)
unset(vsomeip_INCLUDE)
unset(file)

install(
  TARGETS ${VSOMEIP_NAME}
          # IMPORTANT: Add the vsomeip library to the "export-set"
          EXPORT ${VSOMEIP_NAME}Targets
          RUNTIME
          DESTINATION "${INSTALL_BIN_DIR}"
          COMPONENT bin
          LIBRARY
          DESTINATION "${INSTALL_LIB_DIR}"
          COMPONENT shlib
          ARCHIVE
          DESTINATION "${INSTALL_LIB_DIR}"
          COMPONENT dev
)

install(
  TARGETS ${VSOMEIP_NAME}-e2e LIBRARY
          DESTINATION "${INSTALL_LIB_DIR}"
          COMPONENT shlib
          RUNTIME
          DESTINATION "${INSTALL_BIN_DIR}"
          COMPONENT bin
)

if(NOT ENABLE_MULTIPLE_ROUTING_MANAGERS)
  install(
    TARGETS ${VSOMEIP_NAME}-cfg LIBRARY
            DESTINATION "${INSTALL_LIB_DIR}"
            COMPONENT shlib
            RUNTIME
            DESTINATION "${INSTALL_BIN_DIR}"
            COMPONENT bin
  )
endif()

install(
  TARGETS ${VSOMEIP_NAME}-sd LIBRARY
          DESTINATION "${INSTALL_LIB_DIR}"
          COMPONENT shlib
          RUNTIME
          DESTINATION "${INSTALL_BIN_DIR}"
          COMPONENT bin
)

if(ENABLE_COMPAT)
  install(
    TARGETS ${VSOMEIP_COMPAT_NAME}
            EXPORT vsomeipTargets
            LIBRARY
            DESTINATION "${INSTALL_LIB_DIR}"
            COMPONENT shlib
            RUNTIME
            DESTINATION "${INSTALL_BIN_DIR}"
            COMPONENT bin
  )

  export(TARGETS ${VSOMEIP_COMPAT_NAME} FILE "${PROJECT_BINARY_DIR}/vsomeipTargets.cmake")
  export(PACKAGE ${VSOMEIP_COMPAT_NAME})

  configure_file(vsomeipConfig.cmake.in "${PROJECT_BINARY_DIR}/vsomeipConfig.cmake" @ONLY)
  configure_file(vsomeipConfigVersion.cmake.in "${PROJECT_BINARY_DIR}/vsomeipConfigVersion.cmake" @ONLY)

  set(COMPAT_INSTALL_CMAKE_DIR "${DEF_INSTALL_CMAKE_DIR}-compat")

  install(
    EXPORT
    vsomeipTargets
    DESTINATION
    "${COMPAT_INSTALL_CMAKE_DIR}"
    COMPONENT
    dev
  )

  install(
    FILES "${PROJECT_BINARY_DIR}/vsomeipConfig.cmake" "${PROJECT_BINARY_DIR}/vsomeipConfigVersion.cmake"
          DESTINATION "${COMPAT_INSTALL_CMAKE_DIR}"
          COMPONENT dev
  )

  configure_file(vsomeip.pc.in ${PROJECT_BINARY_DIR}/vsomeip.pc @ONLY)
  install(FILES ${PROJECT_BINARY_DIR}/vsomeip.pc DESTINATION lib/pkgconfig)

endif()

install(
  FILES ${EXAMPLE_CONFIG_FILES}
        DESTINATION etc/vsomeip
        COMPONENT config
)

# Add all targets to the build-tree export set
export(TARGETS ${VSOMEIP_NAME} FILE "${PROJECT_BINARY_DIR}/${VSOMEIP_NAME}Targets.cmake")

# Export the package for use from the build-tree (this registers the build-tree with a global CMake-registry)
export(PACKAGE ${VSOMEIP_NAME})

# Create the ${VSOMEIP_NAME}Config.cmake and ${VSOMEIP_NAME}ConfigVersion files
configure_file(${VSOMEIP_NAME}Config.cmake.in "${PROJECT_BINARY_DIR}/${VSOMEIP_NAME}Config.cmake" @ONLY)
configure_file(${VSOMEIP_NAME}ConfigVersion.cmake.in "${PROJECT_BINARY_DIR}/${VSOMEIP_NAME}ConfigVersion.cmake" @ONLY)

# configure internal.hpp for correct version number
configure_file(
  "${PROJECT_SOURCE_DIR}/implementation/configuration/include/internal.hpp.in"
  "${PROJECT_BINARY_DIR}/implementation/configuration/include/internal.hpp"
)

# Install the ${VSOMEIP_NAME}Config.cmake and ${VSOMEIP_NAME}ConfigVersion.cmake
install(
  FILES "${PROJECT_BINARY_DIR}/${VSOMEIP_NAME}Config.cmake" "${PROJECT_BINARY_DIR}/${VSOMEIP_NAME}ConfigVersion.cmake"
        DESTINATION "${INSTALL_CMAKE_DIR}"
        COMPONENT dev
)

# Install the export set for use with the install-tree
install(
  EXPORT
  ${VSOMEIP_NAME}Targets
  DESTINATION
  "${INSTALL_CMAKE_DIR}"
  COMPONENT
  dev
)

#
# build documentation

add_custom_target(doc)

find_package(Doxygen)
if(NOT DOXYGEN_FOUND)
  message(WARNING "Doxygen is not installed. Documentation can not be built.")
else()
  # set configuration variables for doxygen.in
  set(PROJECT "vsomeip")
  set(DOCDIR documentation)
  set(SRCDIR .)
  set(GENERATE_HTML YES)
  set(GENERATE_HTMLHELP NO)
  set(GENERATE_CHI NO)
  set(GENERATE_LATEX NO)
  set(GENERATE_PDF NO)
  set(GENERATE_RTF NO)
  set(GENERATE_MAN NO)
  set(GENERATE_XML NO)
  set(HAVE_DOT YES)

  if(HAVE_DOT)
    # Note: the @DOT_PATH@ variable won't be used in doxygen.in as doxygen somehow manages to strip the last slash from
    # the path and therfore no graphs are generated. Therefore dot should be available in your $PATH
    find_program(DOT_PATH dot)
    if("${DOT_PATH}" STREQUAL "DOT_PATH-NOTFOUND")
      message(WARNING "dot (graphviz) is not installed. Graphs in documentation can't be generated.")
    else()
      message("dot found")
    endif()
  endif()

  configure_file(documentation/doxygen.in ${PROJECT_BINARY_DIR}/Doxyfile @ONLY)
  add_custom_target(
    doxygen-doc
    COMMAND ${DOXYGEN_EXECUTABLE} ${PROJECT_BINARY_DIR}/Doxyfile
    SOURCES ${PROJECT_BINARY_DIR}/Doxyfile
  )

  add_dependencies(doc doxygen-doc)
endif()

#
# create pkg-config file

if(NOT WIN32)
  configure_file(${VSOMEIP_NAME}.pc.in ${PROJECT_BINARY_DIR}/${VSOMEIP_NAME}.pc @ONLY)
  install(FILES ${PROJECT_BINARY_DIR}/${VSOMEIP_NAME}.pc DESTINATION lib/pkgconfig)
endif()

#
# build routing manager daemon (Non-Windows only)

if(NOT MSVC)
  add_subdirectory(examples/routingmanagerd)
  add_subdirectory(examples/hello_world)
endif()

# build tools
add_custom_target(tools)
add_subdirectory(tools/vsomeip_ctrl)

# build examples
add_custom_target(examples)
add_subdirectory(examples EXCLUDE_FROM_ALL)

# ####################################################################

# add test directory
enable_testing()

add_subdirectory(test EXCLUDE_FROM_ALL)

if(${CMAKE_SYSTEM_NAME} MATCHES "QNX")
  install(
    DIRECTORY ${PROJECT_BINARY_DIR}/test/
              DESTINATION bin/vsomeip_tests/test
              PATTERN "CMakeFiles" EXCLUDE
              PATTERN "*.cmake" EXCLUDE
              PATTERN "Makefile" EXCLUDE
  )

  install(FILES ${PROJECT_BINARY_DIR}/examples/routingmanagerd/routingmanagerd
                DESTINATION bin/vsomeip_tests/examples/routingmanagerd
  )
endif()
