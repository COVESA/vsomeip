cmake_minimum_required (VERSION 2.8)
project (vsomeip)
set (VSOMEIP_MAJOR_VERSION 0)
set (VSOMEIP_MINOR_VERSION 1)
set (VSOMEIP_PATCH_VERSION 0)
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
    set (OS "LINUX")
    set (DL_LIBRARY "dl")
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

if (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    set (OS "FREEBSD")
    set(DL_LIBRARY "")
endif (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")

set (CMAKE_CXX_FLAGS "-D${OS} -DUSE_VSOMEIP_STATISTICS -DBOOST_LOG_DYN_LINK -g -std=c++0x -O0 -Wno-deprecated-register")
include_directories("include")

# Boost
find_package( Boost 1.54 COMPONENTS system thread program_options log REQUIRED )
include_directories( ${Boost_INCLUDE_DIR} )

# Base library
file(GLOB vsomeip_SRC
    "src/vsomeip/*.cpp"
)

add_library(vsomeip SHARED ${vsomeip_SRC})
target_link_libraries(vsomeip ${Boost_LIBRARIES} rt ${DL_LIBRARY})

file (GLOB vsomeip-sd_SRC
    "src/vsomeip.sd/*.cpp"
)

add_library(vsomeip-sd SHARED ${vsomeip-sd_SRC})
target_link_libraries(vsomeip-sd vsomeip ${Boost_LIBRARIES} rt ${DL_LIBRARY})

# Daemon
file (GLOB vsomeipd_SRC
    "src/vsomeip.daemon/*.cpp"
)

# Daemon
add_executable(vsomeipd ${vsomeipd_SRC})
target_link_libraries(vsomeipd vsomeip ${Boost_LIBRARIES} rt ${DL_LIBRARY})

# Test executables
add_executable(application_sample src/vsomeip.examples/application_sample.cpp)
target_link_libraries(application_sample vsomeip ${Boost_LIBRARIES} ${DL_LIBRARY})

add_executable(client_sample src/vsomeip.examples/client_sample.cpp)
target_link_libraries(client_sample vsomeip ${Boost_LIBRARIES} ${DL_LIBRARY})

add_executable(service_sample src/vsomeip.examples/service_sample.cpp)
target_link_libraries(service_sample vsomeip ${Boost_LIBRARIES} ${DL_LIBRARY})

add_executable(external-client_sample src/vsomeip.examples/external-client_sample.cpp)
target_link_libraries(external-client_sample vsomeip ${Boost_LIBRARIES} ${DL_LIBRARY})

add_executable(external-service_sample src/vsomeip.examples/external-service_sample.cpp)
target_link_libraries(external-service_sample vsomeip ${Boost_LIBRARIES} ${DL_LIBRARY})

###################################################################################################

file (GLOB_RECURSE vsomeip_INCLUDE "include/*.hpp")

set_target_properties (vsomeip vsomeip-sd PROPERTIES PUBLIC_HEADER "${vsomeip_INCLUDE}")

install (
    TARGETS vsomeip vsomeip-sd
    # IMPORTANT: Add the vsomeip library to the "export-set"
    EXPORT vsomeipTargets
    RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
    PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}/vsomeip" COMPONENT dev
)

# Add all targets to the build-tree export set
export (TARGETS vsomeip vsomeip-sd FILE "${PROJECT_BINARY_DIR}/vSomeIPTargets.cmake")

# Export the package for use from the build-tree
# (this registers the build-tree with a global CMake-registry)
export (PACKAGE vSomeIP)

# Create the vSomeIPConfig.cmake and vSomeIPConfigVersion files
file (RELATIVE_PATH REL_INCLUDE_DIR "${INSTALL_CMAKE_DIR}" "${INSTALL_INCLUDE_DIR}")

# ... for the build tree
set (CONF_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/include" "${PROJECT_BINARY_DIR}")
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
