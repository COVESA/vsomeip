# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.23

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/kali/projects/vsomeip

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/kali/projects/vsomeip

# Include any dependencies generated for this target.
include test/CMakeFiles/pending_subscription_test_service.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include test/CMakeFiles/pending_subscription_test_service.dir/compiler_depend.make

# Include the progress variables for this target.
include test/CMakeFiles/pending_subscription_test_service.dir/progress.make

# Include the compile flags for this target's objects.
include test/CMakeFiles/pending_subscription_test_service.dir/flags.make

test/CMakeFiles/pending_subscription_test_service.dir/pending_subscription_tests/pending_subscription_test_service.cpp.o: test/CMakeFiles/pending_subscription_test_service.dir/flags.make
test/CMakeFiles/pending_subscription_test_service.dir/pending_subscription_tests/pending_subscription_test_service.cpp.o: test/pending_subscription_tests/pending_subscription_test_service.cpp
test/CMakeFiles/pending_subscription_test_service.dir/pending_subscription_tests/pending_subscription_test_service.cpp.o: test/CMakeFiles/pending_subscription_test_service.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/kali/projects/vsomeip/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object test/CMakeFiles/pending_subscription_test_service.dir/pending_subscription_tests/pending_subscription_test_service.cpp.o"
	cd /home/kali/projects/vsomeip/test && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT test/CMakeFiles/pending_subscription_test_service.dir/pending_subscription_tests/pending_subscription_test_service.cpp.o -MF CMakeFiles/pending_subscription_test_service.dir/pending_subscription_tests/pending_subscription_test_service.cpp.o.d -o CMakeFiles/pending_subscription_test_service.dir/pending_subscription_tests/pending_subscription_test_service.cpp.o -c /home/kali/projects/vsomeip/test/pending_subscription_tests/pending_subscription_test_service.cpp

test/CMakeFiles/pending_subscription_test_service.dir/pending_subscription_tests/pending_subscription_test_service.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/pending_subscription_test_service.dir/pending_subscription_tests/pending_subscription_test_service.cpp.i"
	cd /home/kali/projects/vsomeip/test && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/kali/projects/vsomeip/test/pending_subscription_tests/pending_subscription_test_service.cpp > CMakeFiles/pending_subscription_test_service.dir/pending_subscription_tests/pending_subscription_test_service.cpp.i

test/CMakeFiles/pending_subscription_test_service.dir/pending_subscription_tests/pending_subscription_test_service.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/pending_subscription_test_service.dir/pending_subscription_tests/pending_subscription_test_service.cpp.s"
	cd /home/kali/projects/vsomeip/test && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/kali/projects/vsomeip/test/pending_subscription_tests/pending_subscription_test_service.cpp -o CMakeFiles/pending_subscription_test_service.dir/pending_subscription_tests/pending_subscription_test_service.cpp.s

# Object files for target pending_subscription_test_service
pending_subscription_test_service_OBJECTS = \
"CMakeFiles/pending_subscription_test_service.dir/pending_subscription_tests/pending_subscription_test_service.cpp.o"

# External object files for target pending_subscription_test_service
pending_subscription_test_service_EXTERNAL_OBJECTS =

test/pending_subscription_test_service: test/CMakeFiles/pending_subscription_test_service.dir/pending_subscription_tests/pending_subscription_test_service.cpp.o
test/pending_subscription_test_service: test/CMakeFiles/pending_subscription_test_service.dir/build.make
test/pending_subscription_test_service: libvsomeip3.so.3.1.20
test/pending_subscription_test_service: /usr/lib/aarch64-linux-gnu/libboost_system.so.1.74.0
test/pending_subscription_test_service: /usr/lib/aarch64-linux-gnu/libboost_thread.so.1.74.0
test/pending_subscription_test_service: /usr/lib/aarch64-linux-gnu/libboost_filesystem.so.1.74.0
test/pending_subscription_test_service: lib/libgtest.a
test/pending_subscription_test_service: /usr/lib/aarch64-linux-gnu/libboost_atomic.so.1.74.0
test/pending_subscription_test_service: test/CMakeFiles/pending_subscription_test_service.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/kali/projects/vsomeip/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable pending_subscription_test_service"
	cd /home/kali/projects/vsomeip/test && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/pending_subscription_test_service.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
test/CMakeFiles/pending_subscription_test_service.dir/build: test/pending_subscription_test_service
.PHONY : test/CMakeFiles/pending_subscription_test_service.dir/build

test/CMakeFiles/pending_subscription_test_service.dir/clean:
	cd /home/kali/projects/vsomeip/test && $(CMAKE_COMMAND) -P CMakeFiles/pending_subscription_test_service.dir/cmake_clean.cmake
.PHONY : test/CMakeFiles/pending_subscription_test_service.dir/clean

test/CMakeFiles/pending_subscription_test_service.dir/depend:
	cd /home/kali/projects/vsomeip && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/kali/projects/vsomeip /home/kali/projects/vsomeip/test /home/kali/projects/vsomeip /home/kali/projects/vsomeip/test /home/kali/projects/vsomeip/test/CMakeFiles/pending_subscription_test_service.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : test/CMakeFiles/pending_subscription_test_service.dir/depend

