# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.30

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
CMAKE_SOURCE_DIR = /home/point3/Desktop/Client-server

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/point3/Desktop/Client-server/build

# Include any dependencies generated for this target.
include src/CMakeFiles/server.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include src/CMakeFiles/server.dir/compiler_depend.make

# Include the progress variables for this target.
include src/CMakeFiles/server.dir/progress.make

# Include the compile flags for this target's objects.
include src/CMakeFiles/server.dir/flags.make

src/CMakeFiles/server.dir/server.c.o: src/CMakeFiles/server.dir/flags.make
src/CMakeFiles/server.dir/server.c.o: /home/point3/Desktop/Client-server/src/server.c
src/CMakeFiles/server.dir/server.c.o: src/CMakeFiles/server.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/point3/Desktop/Client-server/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object src/CMakeFiles/server.dir/server.c.o"
	cd /home/point3/Desktop/Client-server/build/src && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT src/CMakeFiles/server.dir/server.c.o -MF CMakeFiles/server.dir/server.c.o.d -o CMakeFiles/server.dir/server.c.o -c /home/point3/Desktop/Client-server/src/server.c

src/CMakeFiles/server.dir/server.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing C source to CMakeFiles/server.dir/server.c.i"
	cd /home/point3/Desktop/Client-server/build/src && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/point3/Desktop/Client-server/src/server.c > CMakeFiles/server.dir/server.c.i

src/CMakeFiles/server.dir/server.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling C source to assembly CMakeFiles/server.dir/server.c.s"
	cd /home/point3/Desktop/Client-server/build/src && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/point3/Desktop/Client-server/src/server.c -o CMakeFiles/server.dir/server.c.s

src/CMakeFiles/server.dir/client_mgmt.c.o: src/CMakeFiles/server.dir/flags.make
src/CMakeFiles/server.dir/client_mgmt.c.o: /home/point3/Desktop/Client-server/src/client_mgmt.c
src/CMakeFiles/server.dir/client_mgmt.c.o: src/CMakeFiles/server.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/point3/Desktop/Client-server/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building C object src/CMakeFiles/server.dir/client_mgmt.c.o"
	cd /home/point3/Desktop/Client-server/build/src && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT src/CMakeFiles/server.dir/client_mgmt.c.o -MF CMakeFiles/server.dir/client_mgmt.c.o.d -o CMakeFiles/server.dir/client_mgmt.c.o -c /home/point3/Desktop/Client-server/src/client_mgmt.c

src/CMakeFiles/server.dir/client_mgmt.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing C source to CMakeFiles/server.dir/client_mgmt.c.i"
	cd /home/point3/Desktop/Client-server/build/src && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/point3/Desktop/Client-server/src/client_mgmt.c > CMakeFiles/server.dir/client_mgmt.c.i

src/CMakeFiles/server.dir/client_mgmt.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling C source to assembly CMakeFiles/server.dir/client_mgmt.c.s"
	cd /home/point3/Desktop/Client-server/build/src && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/point3/Desktop/Client-server/src/client_mgmt.c -o CMakeFiles/server.dir/client_mgmt.c.s

# Object files for target server
server_OBJECTS = \
"CMakeFiles/server.dir/server.c.o" \
"CMakeFiles/server.dir/client_mgmt.c.o"

# External object files for target server
server_EXTERNAL_OBJECTS =

src/server: src/CMakeFiles/server.dir/server.c.o
src/server: src/CMakeFiles/server.dir/client_mgmt.c.o
src/server: src/CMakeFiles/server.dir/build.make
src/server: /usr/lib/x86_64-linux-gnu/libssl.so
src/server: /usr/lib/x86_64-linux-gnu/libcrypto.so
src/server: src/CMakeFiles/server.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --bold --progress-dir=/home/point3/Desktop/Client-server/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking C executable server"
	cd /home/point3/Desktop/Client-server/build/src && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/server.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/CMakeFiles/server.dir/build: src/server
.PHONY : src/CMakeFiles/server.dir/build

src/CMakeFiles/server.dir/clean:
	cd /home/point3/Desktop/Client-server/build/src && $(CMAKE_COMMAND) -P CMakeFiles/server.dir/cmake_clean.cmake
.PHONY : src/CMakeFiles/server.dir/clean

src/CMakeFiles/server.dir/depend:
	cd /home/point3/Desktop/Client-server/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/point3/Desktop/Client-server /home/point3/Desktop/Client-server/src /home/point3/Desktop/Client-server/build /home/point3/Desktop/Client-server/build/src /home/point3/Desktop/Client-server/build/src/CMakeFiles/server.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : src/CMakeFiles/server.dir/depend

