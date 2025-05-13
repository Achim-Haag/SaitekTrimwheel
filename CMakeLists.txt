# https://learn.microsoft.com/en-us/shows/visual-studio-code/cpp-in-vs-code-building-your-code-with-cmake

message(STATUS ">>> Start")

# Enable function cmake_print_variables (but printed only on save to CMakeLists.txt !)
include(CMakePrintHelpers)
cmake_print_variables(CMAKE_CURRENT_BINARY_DIR CMAKE_BUILD_TYPE)

# minimum CMake version
CMAKE_MINIMUM_REQUIRED(VERSION 3.31)

# set the project name, version and language C/C++
PROJECT(SaitekTrimwheel VERSION 1.0 LANGUAGES C CXX)

# allow CMake's own testing environment
include(CTest)
enable_testing()

# stuff that should happen not on Linux 
message(STATUS ">>> Windows Microsoft Visual C/C++")

# set variables for Windows Microsoft Visual C/C++ environment
# external pgm getopt.c has to be added as no intrinsic in MSVC
set(MyExeOutpath "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}")
set(MyExeSuffix  ".exe")
cmake_print_variables(MyExeOutpath MyExeSuffix)

# MSVC-only: compile submodule getopts.c to .obj if changed
add_library(getopt OBJECT getopt.c)
target_compile_definitions(getopt PUBLIC GETOPT)

# compile main program if main program or submodule word.c (Linux) or words.c/getopts.c (MSVC) have been changed
# important: although my source name contains a date, the name of the resulting .exe (=target) is without this date
message(STATUS ">>> Generating executable")
add_executable(SaitekTrimwheel SaitekTrimwheel.cpp)
target_link_libraries(SaitekTrimwheel ${MySubmodules})
set_property(TARGET SaitekTrimwheel PROPERTY CXX_STANDARD 17)

# copy executable to source folder
message(STATUS ">>> Copy executable to source folder")
add_custom_command(	TARGET SaitekTrimwheel POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${MyExeOutpath}/SaitekTrimwheel${MyExeSuffix}
		${CMAKE_SOURCE_DIR}
	)
add_custom_command(TARGET SaitekTrimwheel POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --cyan
		"${MyExeOutpath}/SaitekTrimwheel${MyExeSuffix} copied to ${CMAKE_SOURCE_DIR}")

message(STATUS ">>> CMake finished")
# end
						