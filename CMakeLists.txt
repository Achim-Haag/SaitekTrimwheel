# https://learn.microsoft.com/en-us/shows/visual-studio-code/cpp-in-vs-code-building-your-code-with-cmake
#
# After calling Visual Studio Community 2022 for building the executables for either "Debug" or "Release",
# we copy the corresponding executables into our source folder
# For the "Debug" build, we rename the executable and the MS Debugger Symbol File (PDB) with a filename suffix of "_debug"
#

message(STATUS ">>> Start preparing environment")

# minimum CMake version
CMAKE_MINIMUM_REQUIRED(VERSION 3.31)

# set the project name, version and language C/C++
PROJECT(SaitekTrimwheel VERSION 1.0 LANGUAGES C CXX)

# Enable function cmake_print_variables (but printed only on save to CMakeLists.txt !)
include(CMakePrintHelpers)
cmake_print_variables(CMAKE_CURRENT_BINARY_DIR CMAKE_CONFIGURATION_TYPES)

# allow CMake's own testing environment
include(CTest)
enable_testing()

# stuff that should happen not on Linux 
message(STATUS ">>> Prepare for Microsoft Visual C/C++")

# set variables for Windows Microsoft Visual C/C++ environment
# MSVC creates .exe in subfolders "release" or "debug"
set(MyExeExt ".exe")
set(MyPdbExt ".pdb")
set(MyExeOutpath "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CONFIGURATION_TYPES}")
set(MySubmodules "$<TARGET_OBJECTS:getopt>")
cmake_print_variables( MyExeOutpath )
#
if (CMAKE_CONFIGURATION_TYPES STREQUAL "Release")
	set(MyFileSuffix "")
elseif(CMAKE_CONFIGURATION_TYPES STREQUAL "Debug")
	set(MyFileSuffix "_debug")
else()
	cmake_print_variables(CMAKE_CONFIGURATION_TYPES)
	message( FATAL_ERROR "CMAKE_CONFIGURATION_TYPES not 'release' or 'debug'")
endif()
cmake_print_variables( MyExeExt MySubmodules MyFileSuffix)

# MSVC-only: compile submodule getopts.c to .obj if changed
add_library(getopt OBJECT getopt.c)
target_compile_definitions(getopt PUBLIC GETOPT)

# compile main program if main program or submodule word.c (Linux) or words.c/getopts.c (MSVC) have been changed
# important: although my source name contains a date, the name of the resulting .exe (=target) is without this date
message(STATUS ">>> Define executable generation")
add_executable(SaitekTrimwheel SaitekTrimwheel.cpp)
target_link_libraries(SaitekTrimwheel ${MySubmodules} ${CMAKE_SOURCE_DIR}/GameInput.lib)
set_property(TARGET SaitekTrimwheel PROPERTY CXX_STANDARD 17)

# for debug and release build: copy the executable to the source folder
# if debug then add "_debug" to filename
message(STATUS ">>> Define copy of executable/pdb-file (Debug-only) to source folder")
add_custom_command(	TARGET SaitekTrimwheel POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy 
		${MyExeOutpath}/SaitekTrimwheel${MyExeExt}
		${CMAKE_SOURCE_DIR}/SaitekTrimwheel${MyFileSuffix}${MyExeExt}
	)
add_custom_command(TARGET SaitekTrimwheel POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --cyan
		"Copying EXE ${MyExeOutpath}/SaitekTrimwheel${MyExeExt} to ${CMAKE_SOURCE_DIR}/SaitekTrimwheel${MyFileSuffix}${MyExeExt}")

# only for debug build: copy the pdb (Visual Debugger symbol file) too
# and add "_debug" to filename
if (CMAKE_CONFIGURATION_TYPES STREQUAL "Debug")
	add_custom_command(	TARGET SaitekTrimwheel POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy 
			${MyExeOutpath}/SaitekTrimwheel${MyPdbExt}
			${CMAKE_SOURCE_DIR}/SaitekTrimwheel${MyFileSuffix}${MyPdbExt}
		)
	add_custom_command(TARGET SaitekTrimwheel POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --cyan
			"Copying PDB ${MyExeOutpath}/SaitekTrimwheel${MyPdbExt} to ${CMAKE_SOURCE_DIR}/SaitekTrimwheel${MyFileSuffix}${MyPdbExt}")
endif()

#
message(STATUS ">>> CMake preparing finished")
# end
						