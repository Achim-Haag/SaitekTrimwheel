# https://learn.microsoft.com/en-us/shows/visual-studio-code/cpp-in-vs-code-building-your-code-with-cmake

message(STATUS ">>> Start preparing environment")

# Enable function cmake_print_variables (but printed only on save to CMakeLists.txt !)
include(CMakePrintHelpers)
cmake_print_variables(CMAKE_CURRENT_BINARY_DIR CMAKE_CONFIGURATION_TYPES)

# minimum CMake version
CMAKE_MINIMUM_REQUIRED(VERSION 3.31)

# set the project name, version and language C/C++
PROJECT(SaitekTrimwheel VERSION 1.0 LANGUAGES C CXX)

# allow CMake's own testing environment
include(CTest)
enable_testing()

# stuff that should happen not on Linux 
message(STATUS ">>> Prepare for Microsoft Visual C/C++")

# set variables for Windows Microsoft Visual C/C++ environment
# MSVC creates .exe in subfolders "release" or "debug"
set(MyExeOutpath "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CONFIGURATION_TYPES}")
set(MyExeSuffix  ".exe")
set(MySubmodules "$<TARGET_OBJECTS:getopt>")
cmake_print_variables(MyExeOutpath MyExeSuffix MySubmodules)

# MSVC-only: compile submodule getopts.c to .obj if changed
add_library(getopt OBJECT getopt.c)
target_compile_definitions(getopt PUBLIC GETOPT)

# compile main program if main program or submodule word.c (Linux) or words.c/getopts.c (MSVC) have been changed
# important: although my source name contains a date, the name of the resulting .exe (=target) is without this date
message(STATUS ">>> Define executable generation")
add_executable(SaitekTrimwheel SaitekTrimwheel.cpp)
target_link_libraries(SaitekTrimwheel ${MySubmodules} ${CMAKE_SOURCE_DIR}/GameInput.lib)
set_property(TARGET SaitekTrimwheel PROPERTY CXX_STANDARD 17)

# copy executable to source folder
message(STATUS ">>> Define copy of executable to source folder")
add_custom_command(	TARGET SaitekTrimwheel POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${MyExeOutpath}/SaitekTrimwheel${MyExeSuffix}
		${CMAKE_SOURCE_DIR}
	)
add_custom_command(TARGET SaitekTrimwheel POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --cyan
		"${MyExeOutpath}/SaitekTrimwheel${MyExeSuffix} copied to ${CMAKE_SOURCE_DIR}")

message(STATUS ">>> CMake preparing finished")
# end
						