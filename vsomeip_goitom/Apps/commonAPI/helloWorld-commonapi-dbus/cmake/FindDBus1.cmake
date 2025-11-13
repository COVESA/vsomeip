# FindDBus1.cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(DBUS1 REQUIRED dbus-1)

include_directories(${DBUS1_INCLUDE_DIRS})
set(DBus1_INCLUDE_DIRS ${DBUS1_INCLUDE_DIRS})
set(DBus1_LIBRARIES ${DBUS1_LIBRARIES})
set(DBus1_LIBRARY_DIRS ${DBUS1_LIBRARY_DIRS})
set(DBus1_FOUND TRUE)

