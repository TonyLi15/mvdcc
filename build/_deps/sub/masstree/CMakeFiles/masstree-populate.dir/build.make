# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.16

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
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
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/tonyli_15/serval/build/_deps/sub/masstree

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/tonyli_15/serval/build/_deps/sub/masstree

# Utility rule file for masstree-populate.

# Include the progress variables for this target.
include CMakeFiles/masstree-populate.dir/progress.make

CMakeFiles/masstree-populate: CMakeFiles/masstree-populate-complete


CMakeFiles/masstree-populate-complete: masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-install
CMakeFiles/masstree-populate-complete: masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-mkdir
CMakeFiles/masstree-populate-complete: masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-download
CMakeFiles/masstree-populate-complete: masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-update
CMakeFiles/masstree-populate-complete: masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-patch
CMakeFiles/masstree-populate-complete: masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-configure
CMakeFiles/masstree-populate-complete: masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-build
CMakeFiles/masstree-populate-complete: masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-install
CMakeFiles/masstree-populate-complete: masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-test
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/tonyli_15/serval/build/_deps/sub/masstree/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Completed 'masstree-populate'"
	/usr/bin/cmake -E make_directory /home/tonyli_15/serval/build/_deps/sub/masstree/CMakeFiles
	/usr/bin/cmake -E touch /home/tonyli_15/serval/build/_deps/sub/masstree/CMakeFiles/masstree-populate-complete
	/usr/bin/cmake -E touch /home/tonyli_15/serval/build/_deps/sub/masstree/masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-done

masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-install: masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-build
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/tonyli_15/serval/build/_deps/sub/masstree/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "No install step for 'masstree-populate'"
	cd /home/tonyli_15/serval/build/_deps/build/masstree && /usr/bin/cmake -E echo_append
	cd /home/tonyli_15/serval/build/_deps/build/masstree && /usr/bin/cmake -E touch /home/tonyli_15/serval/build/_deps/sub/masstree/masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-install

masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-mkdir:
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/tonyli_15/serval/build/_deps/sub/masstree/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Creating directories for 'masstree-populate'"
	/usr/bin/cmake -E make_directory /home/tonyli_15/serval/build/_deps/src/masstree
	/usr/bin/cmake -E make_directory /home/tonyli_15/serval/build/_deps/build/masstree
	/usr/bin/cmake -E make_directory /home/tonyli_15/serval/build/_deps/sub/masstree/masstree-populate-prefix
	/usr/bin/cmake -E make_directory /home/tonyli_15/serval/build/_deps/sub/masstree/masstree-populate-prefix/tmp
	/usr/bin/cmake -E make_directory /home/tonyli_15/serval/build/_deps/sub/masstree/masstree-populate-prefix/src/masstree-populate-stamp
	/usr/bin/cmake -E make_directory /home/tonyli_15/serval/build/_deps/sub/masstree/masstree-populate-prefix/src
	/usr/bin/cmake -E make_directory /home/tonyli_15/serval/build/_deps/sub/masstree/masstree-populate-prefix/src/masstree-populate-stamp
	/usr/bin/cmake -E touch /home/tonyli_15/serval/build/_deps/sub/masstree/masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-mkdir

masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-download: masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-gitinfo.txt
masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-download: masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-mkdir
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/tonyli_15/serval/build/_deps/sub/masstree/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Performing download step (git clone) for 'masstree-populate'"
	cd /home/tonyli_15/serval/build/_deps/src && /usr/bin/cmake -P /home/tonyli_15/serval/build/_deps/sub/masstree/masstree-populate-prefix/tmp/masstree-populate-gitclone.cmake
	cd /home/tonyli_15/serval/build/_deps/src && /usr/bin/cmake -E touch /home/tonyli_15/serval/build/_deps/sub/masstree/masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-download

masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-update: masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-download
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/tonyli_15/serval/build/_deps/sub/masstree/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Performing update step for 'masstree-populate'"
	cd /home/tonyli_15/serval/build/_deps/src/masstree && /usr/bin/cmake -P /home/tonyli_15/serval/build/_deps/sub/masstree/masstree-populate-prefix/tmp/masstree-populate-gitupdate.cmake

masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-patch: masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-download
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/tonyli_15/serval/build/_deps/sub/masstree/CMakeFiles --progress-num=$(CMAKE_PROGRESS_6) "No patch step for 'masstree-populate'"
	/usr/bin/cmake -E echo_append
	/usr/bin/cmake -E touch /home/tonyli_15/serval/build/_deps/sub/masstree/masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-patch

masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-configure: masstree-populate-prefix/tmp/masstree-populate-cfgcmd.txt
masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-configure: masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-update
masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-configure: masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-patch
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/tonyli_15/serval/build/_deps/sub/masstree/CMakeFiles --progress-num=$(CMAKE_PROGRESS_7) "No configure step for 'masstree-populate'"
	cd /home/tonyli_15/serval/build/_deps/build/masstree && /usr/bin/cmake -E echo_append
	cd /home/tonyli_15/serval/build/_deps/build/masstree && /usr/bin/cmake -E touch /home/tonyli_15/serval/build/_deps/sub/masstree/masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-configure

masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-build: masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-configure
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/tonyli_15/serval/build/_deps/sub/masstree/CMakeFiles --progress-num=$(CMAKE_PROGRESS_8) "No build step for 'masstree-populate'"
	cd /home/tonyli_15/serval/build/_deps/build/masstree && /usr/bin/cmake -E echo_append
	cd /home/tonyli_15/serval/build/_deps/build/masstree && /usr/bin/cmake -E touch /home/tonyli_15/serval/build/_deps/sub/masstree/masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-build

masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-test: masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-install
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/tonyli_15/serval/build/_deps/sub/masstree/CMakeFiles --progress-num=$(CMAKE_PROGRESS_9) "No test step for 'masstree-populate'"
	cd /home/tonyli_15/serval/build/_deps/build/masstree && /usr/bin/cmake -E echo_append
	cd /home/tonyli_15/serval/build/_deps/build/masstree && /usr/bin/cmake -E touch /home/tonyli_15/serval/build/_deps/sub/masstree/masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-test

masstree-populate: CMakeFiles/masstree-populate
masstree-populate: CMakeFiles/masstree-populate-complete
masstree-populate: masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-install
masstree-populate: masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-mkdir
masstree-populate: masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-download
masstree-populate: masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-update
masstree-populate: masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-patch
masstree-populate: masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-configure
masstree-populate: masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-build
masstree-populate: masstree-populate-prefix/src/masstree-populate-stamp/masstree-populate-test
masstree-populate: CMakeFiles/masstree-populate.dir/build.make

.PHONY : masstree-populate

# Rule to build all files generated by this target.
CMakeFiles/masstree-populate.dir/build: masstree-populate

.PHONY : CMakeFiles/masstree-populate.dir/build

CMakeFiles/masstree-populate.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/masstree-populate.dir/cmake_clean.cmake
.PHONY : CMakeFiles/masstree-populate.dir/clean

CMakeFiles/masstree-populate.dir/depend:
	cd /home/tonyli_15/serval/build/_deps/sub/masstree && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/tonyli_15/serval/build/_deps/sub/masstree /home/tonyli_15/serval/build/_deps/sub/masstree /home/tonyli_15/serval/build/_deps/sub/masstree /home/tonyli_15/serval/build/_deps/sub/masstree /home/tonyli_15/serval/build/_deps/sub/masstree/CMakeFiles/masstree-populate.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/masstree-populate.dir/depend

