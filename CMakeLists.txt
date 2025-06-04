# https://learn.microsoft.com/en-us/shows/visual-studio-code/cpp-in-vs-code-building-your-code-with-cmake
#
# After calling Visual Studio Community 2022 for building the executables for either "Debug" or "Release",
# we copy the corresponding executables into our source folder
# For the "Debug" build, we rename the executable and the MS Debugger Symbol File (PDB) with a filename suffix of "_debug"
#
# CMake knows 3 stages:
# - Configure stage : cmake runs and processes this file
# - Build stage : the generator (here Visual Studio 2022) is called
# - Install stage (if INSTALL target has been specified)
# 
message(STATUS ">>> Start preparing CMake environment")
# minimum CMake version
CMAKE_MINIMUM_REQUIRED(VERSION 3.31)

# set the project name, version and language C/C++
PROJECT(SaitekTrimwheel VERSION 1.0 LANGUAGES C CXX)

# Enable function cmake_print_variables (but printed only on save to CMakeLists.txt !)
include(CMakePrintHelpers)
# allow CMake's own testing environment
include(CTest)
enable_testing()

# Dummy TARGET for dependencies to run cmake_echo_color (messages in build phase) before the build beginns
add_custom_target(myBuildMsgs)

message(STATUS ">>> Prepare for Microsoft Visual C/C++")
# set variables for Windows Microsoft Visual C/C++ environment
# print variables - executes only in config stage !
cmake_print_variables(CMAKE_CONFIGURATION_TYPES CMAKE_CURRENT_BINARY_DIR)
# MSVC creates .exe in subfolders "release" or "debug"
set(MyExeExt ".exe")
set(MyExeOutpath "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CONFIGURATION_TYPES}")
set(MySubmodules "$<TARGET_OBJECTS:getopt>")
# Set variables dependent on selected build environmen in CMAKE_CONFIGURATION_TYPES
#
if (CMAKE_CONFIGURATION_TYPES STREQUAL "Release")
	set(MyFileSuffix "")
	unset(MyPdbExt)
elseif(CMAKE_CONFIGURATION_TYPES STREQUAL "Debug")
	set(MyFileSuffix "_debug")
	set(MyPdbExt ".pdb")
else()
	cmake_print_variables(CMAKE_CONFIGURATION_TYPES)
	message( FATAL_ERROR "CMAKE_CONFIGURATION_TYPES not 'release' or 'debug'")
endif()
# print variables - executes only in config stage !
cmake_print_variables( MyExeOutpath )
cmake_print_variables( MyExeExt MyPdbExt MyFileSuffix )
cmake_print_variables( MySubmodules )

# Print variable at build stage before generator is running
add_custom_command(TARGET myBuildMsgs PRE_BUILD
		COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --cyan
			"Build starting for environment '${CMAKE_CONFIGURATION_TYPES}'")

#
message(STATUS ">>> Prepare generator MS Visual C/C++")
# MSVC-only: compile submodule getopts.c to .obj if changed
message(STATUS ">>> Define external subfunction getopt")
add_library(getopt OBJECT getopt.c)
target_compile_definitions(getopt PUBLIC GETOPT)

# compile main program if main program or submodule word.c (Linux) or words.c/getopts.c (MSVC) have been changed
# important: although my source name contains a date, the name of the resulting .exe (=target) is without this date
message(STATUS ">>> Define main program ")
add_executable(SaitekTrimwheel SaitekTrimwheel.cpp)
target_link_libraries(SaitekTrimwheel ${MySubmodules} ${CMAKE_SOURCE_DIR}/GameInput.lib)
set_property(TARGET SaitekTrimwheel PROPERTY CXX_STANDARD 17)

# trick to print cmake_echo_color msgs in build stage before the build will be done
message(STATUS ">>> Add dummy dependencies for CMake build echoes")
add_dependencies(SaitekTrimwheel myBuildMsgs)
add_dependencies(getopt myBuildMsgs)

# for debug and release build: copy the executable to the source folder
# if debug then add "_debug" to filename
message(STATUS ">>> Define copy of executable/pdb-file (Debug-only) to source folder")
add_custom_command(TARGET SaitekTrimwheel POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy 
		${MyExeOutpath}/SaitekTrimwheel${MyExeExt}
		${CMAKE_SOURCE_DIR}/SaitekTrimwheel${MyFileSuffix}${MyExeExt}
	COMMENT "Copy EXE SaitekTrimwheel${MyExeExt} to ${CMAKE_SOURCE_DIR}/SaitekTrimwheel${MyFileSuffix}${MyExeExt}"
	)
##add_custom_command(TARGET SaitekTrimwheel POST_BUILD
##	COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --cyan
##		"Copying EXE ${MyExeOutpath}/SaitekTrimwheel${MyExeExt} to ${CMAKE_SOURCE_DIR}/SaitekTrimwheel${MyFileSuffix}${MyExeExt}")

# only for debug build
if (CMAKE_CONFIGURATION_TYPES STREQUAL "Debug")
# copy the pdb (Visual Debugger symbol file) too and add "_debug" to filename
	add_custom_command(TARGET SaitekTrimwheel POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy 
			${MyExeOutpath}/SaitekTrimwheel${MyPdbExt}
			${CMAKE_SOURCE_DIR}/SaitekTrimwheel${MyFileSuffix}${MyPdbExt}
		COMMENT "Copy PDB SaitekTrimwheel${MyPdbExt} to ${CMAKE_SOURCE_DIR}/SaitekTrimwheel${MyFileSuffix}${MyPdbExt}"
		)
endif()

#
message(STATUS ">>> CMake preparing finished")
# end
						