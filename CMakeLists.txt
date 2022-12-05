
cmake_minimum_required (VERSION 3.15)

project (coproto VERSION 0.3.0)


# make cache variables for install destinations
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Set a default build type for single-configuration
# CMake generators if no build type is set.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
	SET(CMAKE_BUILD_TYPE Release)
endif()

set(COPROTO_BUILD ON)

include(cmake/preamble.cmake)
include(cmake/buildOptions.cmake)
include(cmake/coprotoFindDeps.cmake)

add_subdirectory ("coproto")
add_subdirectory ("tests")
add_subdirectory ("frontend")