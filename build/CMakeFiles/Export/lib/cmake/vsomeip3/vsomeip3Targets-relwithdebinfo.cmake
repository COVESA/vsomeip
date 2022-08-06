#----------------------------------------------------------------
# Generated CMake target import file for configuration "RelWithDebInfo".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "vsomeip3" for configuration "RelWithDebInfo"
set_property(TARGET vsomeip3 APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(vsomeip3 PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELWITHDEBINFO "Boost::system;Boost::thread;Boost::filesystem"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/lib/libvsomeip3.so.3.1.20"
  IMPORTED_SONAME_RELWITHDEBINFO "libvsomeip3.so.3"
  )

list(APPEND _IMPORT_CHECK_TARGETS vsomeip3 )
list(APPEND _IMPORT_CHECK_FILES_FOR_vsomeip3 "${_IMPORT_PREFIX}/lib/libvsomeip3.so.3.1.20" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
