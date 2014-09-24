cmake_minimum_required (VERSION 2.8)
project (vsomeip)
set (VSOMEIP_MAJOR_VERSION 0)
set (VSOMEIP_MINOR_VERSION 0)
set (VSOMEIP_PATCH_VERSION 1)
set (VSOMEIP_VERSION ${VSOMEIP_MAJOR_VERSION}.${VSOMEIP_MINOR_VERSION}.${VSOMEIP_PATCH_VERSION})
set (CMAKE_VERBOSE_MAKEFILE off)

###################################################################################################
# see http://www.cmake.org/Wiki/CMake/Tutorials/How_to_create_a_ProjectConfig.cmake_file
###################################################################################################

# Offer the user the choice of overriding the installation directories
set (INSTALL_LIB_DIR lib CACHE PATH "Installation directory for libraries")
set (INSTALL_BIN_DIR bin CACHE PATH "Installation directory for executables")
set (INSTALL_INCLUDE_DIR include CACHE PATH "Installation directory for header files")

if (WIN32 AND NOT CYGWIN)
    set (DEF_INSTALL_CMAKE_DIR CMake)
else ()
    set (DEF_INSTALL_CMAKE_DIR lib/CMake/vSomeIP)
endif ()

set (INSTALL_CMAKE_DIR ${DEF_INSTALL_CMAKE_DIR} CACHE PATH "Installation directory for CMake files")

# Make relative paths absolute (needed later on)
foreach (p LIB BIN INCLUDE CMAKE)
    set (var INSTALL_${p}_DIR)
    if (NOT IS_ABSOLUTE "${${var}}")
        set (${var} "${CMAKE_INSTALL_PREFIX}/${${var}}") # Add all targets to the build-tree export set
    endif ()
endforeach ()

###################################################################################################

# OS
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(OS "LINUX")
    set(DL_LIBRARY "dl")
    set(EXPORTSYMBOLS "-Wl,-export-dynamic -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exportmap.gcc")
    set(NO_DEPRECATED "")
    set(OPTIMIZE "")
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

if (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    set(OS "FREEBSD")
    set(DL_LIBRARY "")
    set(EXPORTSYMBOLS "")
    set(NO_DEPRECATED "-Wno-deprecated")
    set(OPTIMIZE "")
endif (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")

# Compiler settings
set (CMAKE_CXX_FLAGS "-D${OS} -DUSE_VSOMEIP_STATISTICS -DBOOST_LOG_DYN_LINK -g ${OPTIMIZE} -std=c++0x ${NO_DEPRECATED} ${EXPORTSYMBOLS}")

include_directories(
	"interface" 
)
# Threads
find_package(Threads REQUIRED)

# Boost
find_package( Boost 1.54 COMPONENTS system thread log REQUIRED )
include_directories( ${Boost_INCLUDE_DIR} )

# Base library
file(GLOB vsomeip_SRC
    "implementation/configuration/src/*.cpp"
    "implementation/endpoints/src/*.cpp"
    "implementation/logging/src/*.cpp"
    "implementation/message/src/*.cpp"
    "implementation/routing/src/*.cpp"
    "implementation/runtime/src/*.cpp"
    "implementation/utility/src/*.cpp"
)

add_library(vsomeip SHARED ${vsomeip_SRC})
target_link_libraries(vsomeip ${Boost_LIBRARIES} rt ${DL_LIBRARY})

file(GLOB vsomeip-sd_SRC
	"implementation/service_discovery/src/*.cpp"
)

add_library(vsomeip-sd SHARED ${vsomeip-sd_SRC})
target_link_libraries(vsomeip-sd vsomeip ${Boost_LIBRARIES} rt ${DL_LIBRARY})

# Examples
add_executable(request-sample examples/request-sample.cpp)
target_link_libraries(request-sample vsomeip ${Boost_LIBRARIES} ${DL_LIBRARY})

add_executable(response-sample examples/response-sample.cpp)
target_link_libraries(response-sample vsomeip ${Boost_LIBRARIES} ${DL_LIBRARY})

add_executable(subscribe-sample examples/subscribe-sample.cpp)
target_link_libraries(subscribe-sample vsomeip ${Boost_LIBRARIES} ${DL_LIBRARY})

add_executable(notify-sample examples/notify-sample.cpp)
target_link_libraries(notify-sample vsomeip ${Boost_LIBRARIES} ${DL_LIBRARY})

# Test
add_executable(configuration-test test/configuration-test.cpp)
target_link_libraries(configuration-test vsomeip ${Boost_LIBRARIES} ${DL_LIBRARY})

add_executable(magic-cookies-test-client test/magic-cookies-test-client.cpp)
target_link_libraries(magic-cookies-test-client vsomeip ${Boost_LIBRARIES} ${DL_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

###################################################################################################

file (GLOB_RECURSE vsomeip_INCLUDE "interface/*.hpp")

set_target_properties (vsomeip PROPERTIES PUBLIC_HEADER "${vsomeip_INCLUDE}")

install (
    TARGETS vsomeip 
    # IMPORTANT: Add the vsomeip library to the "export-set"
    EXPORT vsomeipTargets
    RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
    PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}/vsomeip" COMPONENT dev
)

# Add all targets to the build-tree export set
export (TARGETS vsomeip FILE "${PROJECT_BINARY_DIR}/vSomeIPTargets.cmake")

# Export the package for use from the build-tree
# (this registers the build-tree with a global CMake-registry)
export (PACKAGE vSomeIP)

# Create the vSomeIPConfig.cmake and vSomeIPConfigVersion files
file (RELATIVE_PATH REL_INCLUDE_DIR "${INSTALL_CMAKE_DIR}" "${INSTALL_INCLUDE_DIR}")

# ... for the build tree
set (CONF_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/interface" "${PROJECT_BINARY_DIR}")
configure_file (vSomeIPConfig.cmake.in "${PROJECT_BINARY_DIR}/vSomeIPConfig.cmake" @ONLY)

# ... for the install tree
set (CONF_INCLUDE_DIRS "\${VSOMEIP_CMAKE_DIR}/${REL_INCLUDE_DIR}")
configure_file (vSomeIPConfig.cmake.in "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/vSomeIPConfig.cmake" @ONLY)

# ... for both
configure_file (vSomeIPConfigVersion.cmake.in "${PROJECT_BINARY_DIR}/vSomeIPConfigVersion.cmake" @ONLY)

# Install the vSomeIPConfig.cmake and vSomeIPConfigVersion.cmake
install (
    FILES
    "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/vSomeIPConfig.cmake"
    "${PROJECT_BINARY_DIR}/vSomeIPConfigVersion.cmake"
    DESTINATION "${INSTALL_CMAKE_DIR}"
    COMPONENT dev
)

# Install the export set for use with the install-tree
install (
    EXPORT vsomeipTargets
    DESTINATION "${INSTALL_CMAKE_DIR}"
    COMPONENT dev
)
