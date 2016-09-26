cmake_minimum_required(VERSION 3.5)
ENABLE_LANGUAGE(C)
project(OpenSkyNet CXX)

include(cmake/InstallHeaders.cmake)

# OpenSkyNet version
set(OpenSkyNet_VERSION_MAJOR 0)
set(OpenSkyNet_VERSION_MINOR 3)

cmake_policy(SET CMP0015 NEW)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -static-libstdc++")

add_definitions(-DUSE_OPENCV)

# Set the build number
if(DEFINED ENV{BUILD_NUMBER})
    set(OpenSkyNet_VERSION_BUILD $ENV{BUILD_NUMBER})
else()
    set(OpenSkyNet_VERSION_BUILD "SNAPSHOT")
endif()

# Initialize version.h
configure_file(version.h.in OpenSkyNetVersion.h)

include_directories(${CMAKE_BINARY_DIR}/include)

# Offer the user the choice of overriding the installation directories
set(INSTALL_LIB_DIR lib CACHE PATH "Installation directory for libraries")
set(INSTALL_BIN_DIR bin CACHE PATH "Installation directory for executables")
set(INSTALL_INCLUDE_DIR include CACHE PATH
        "Installation directory for header files")

# Set up include directory for local builds
include_directories(${PROJECT_BINARY_DIR})

include_directories()
add_subdirectory(openskynet)
add_subdirectory(gui)
add_subdirectory(cli)


# Add all targets to the build-tree export set
export(TARGETS openskynet OpenSkyNetCLI gui
        FILE "${PROJECT_BINARY_DIR}/OpenSkyNetTargets.cmake")
