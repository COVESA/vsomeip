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

# Utility rule file for symlink_initial_event_test_diff_client_ids_same_ports_slave_udp.json_3214.

# Include any custom commands dependencies for this target.
include test/CMakeFiles/symlink_initial_event_test_diff_client_ids_same_ports_slave_udp.json_3214.dir/compiler_depend.make

# Include the progress variables for this target.
include test/CMakeFiles/symlink_initial_event_test_diff_client_ids_same_ports_slave_udp.json_3214.dir/progress.make

test/CMakeFiles/symlink_initial_event_test_diff_client_ids_same_ports_slave_udp.json_3214: test/initial_event_test_diff_client_ids_same_ports_slave_udp.json

test/initial_event_test_diff_client_ids_same_ports_slave_udp.json: test/initial_event_tests/initial_event_test_diff_client_ids_same_ports_slave_udp.json
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/kali/projects/vsomeip/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Copying \"/home/kali/projects/vsomeip/test/initial_event_tests/initial_event_test_diff_client_ids_same_ports_slave_udp.json\" into build directory"
	cd /home/kali/projects/vsomeip/test && /usr/bin/cmake -E copy /home/kali/projects/vsomeip/test/initial_event_tests/initial_event_test_diff_client_ids_same_ports_slave_udp.json /home/kali/projects/vsomeip/test/initial_event_test_diff_client_ids_same_ports_slave_udp.json

symlink_initial_event_test_diff_client_ids_same_ports_slave_udp.json_3214: test/CMakeFiles/symlink_initial_event_test_diff_client_ids_same_ports_slave_udp.json_3214
symlink_initial_event_test_diff_client_ids_same_ports_slave_udp.json_3214: test/initial_event_test_diff_client_ids_same_ports_slave_udp.json
symlink_initial_event_test_diff_client_ids_same_ports_slave_udp.json_3214: test/CMakeFiles/symlink_initial_event_test_diff_client_ids_same_ports_slave_udp.json_3214.dir/build.make
.PHONY : symlink_initial_event_test_diff_client_ids_same_ports_slave_udp.json_3214

# Rule to build all files generated by this target.
test/CMakeFiles/symlink_initial_event_test_diff_client_ids_same_ports_slave_udp.json_3214.dir/build: symlink_initial_event_test_diff_client_ids_same_ports_slave_udp.json_3214
.PHONY : test/CMakeFiles/symlink_initial_event_test_diff_client_ids_same_ports_slave_udp.json_3214.dir/build

test/CMakeFiles/symlink_initial_event_test_diff_client_ids_same_ports_slave_udp.json_3214.dir/clean:
	cd /home/kali/projects/vsomeip/test && $(CMAKE_COMMAND) -P CMakeFiles/symlink_initial_event_test_diff_client_ids_same_ports_slave_udp.json_3214.dir/cmake_clean.cmake
.PHONY : test/CMakeFiles/symlink_initial_event_test_diff_client_ids_same_ports_slave_udp.json_3214.dir/clean

test/CMakeFiles/symlink_initial_event_test_diff_client_ids_same_ports_slave_udp.json_3214.dir/depend:
	cd /home/kali/projects/vsomeip && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/kali/projects/vsomeip /home/kali/projects/vsomeip/test /home/kali/projects/vsomeip /home/kali/projects/vsomeip/test /home/kali/projects/vsomeip/test/CMakeFiles/symlink_initial_event_test_diff_client_ids_same_ports_slave_udp.json_3214.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : test/CMakeFiles/symlink_initial_event_test_diff_client_ids_same_ports_slave_udp.json_3214.dir/depend

