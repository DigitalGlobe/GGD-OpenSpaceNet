cmake_policy(SET CMP0015 NEW)
cmake_minimum_required(VERSION 3.5)

project(OpenSpaceNet CXX)

# Options for overriding the installation directories
set(INSTALL_LIB_DIR lib CACHE PATH "Installation directory for libraries")
set(INSTALL_BIN_DIR bin CACHE PATH "Installation directory for executables")
set(INSTALL_INCLUDE_DIR include CACHE PATH "Installation directory for header files")

#
# Set up OpenSpaceNet versioning
#
set(OpenSpaceNet_VERSION_MAJOR 0)
set(OpenSpaceNet_VERSION_MINOR 4)
set(OpenSpaceNet_VERSION_PATCH 0)

# Set the build number
if(DEFINED ENV{BUILD_NUMBER})
    set(OpenSpaceNet_VERSION_BUILD $ENV{BUILD_NUMBER})
else()
    set(OpenSpaceNet_VERSION_BUILD "SNAPSHOT")
endif()

#
# Common dependencies
#
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -static-libgcc -static-libstdc++")

if (NOT DEEPCORE_DEP_DIR)
    if(DEFINED ENV{DEEPCORE_DEP_DIR})
        set(DEEPCORE_DEP_DIR $ENV{DEEPCORE_DEP_DIR})
    else()
        set(DEEPCORE_DEP_DIR /opt/DeepCore)
    endif()
endif()


if(DEEPCORE_STATIC)
    set(CMAKE_CXX_FLAGS "-I ${DEEPCORE_DEP_DIR}/include -L${DEEPCORE_DEP_DIR}/lib -L/${DEEPCORE_DEP_DIR}/lib64 ${CMAKE_CXX_FLAGS}")
endif()

find_package(DeepCore REQUIRED)
if (DeepCore_FOUND)
    list(APPEND CMAKE_MODULE_PATH ${DEEPCORE_CMAKE_DIR})
endif()

#
# Add sub-projects
#
add_subdirectory(common)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/common)

add_subdirectory(cli)

#
# Package
#
if (UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_PACKAGE_NAME "OpenSpaceNet")
    set(CPACK_PACKAGE_VENDOR "DigitalGlobe")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "DeepCore Support <deepcore-support@digitalglobe.com>")
    set(CPACK_DEBIAN_PACKAGE_VERSION "${OpenSpaceNet_VERSION_MAJOR}.${OpenSpaceNet_VERSION_MINOR}.${OpenSpaceNet_VERSION_PATCH}+${OpenSpaceNet_VERSION_BUILD}")
    set(CPACK_PACKAGE_VERSION "${OpenSpaceNet_VERSION_MAJOR}.${OpenSpaceNet_VERSION_MINOR}.${OpenSpaceNet_VERSION_PATCH}+${OpenSpaceNet_VERSION_BUILD}")
    set(CPACK_PACKAGE_VERSION_MAJOR "${OpenSpaceNet_VERSION_MAJOR}")
    set(CPACK_PACKAGE_VERSION_MINOR "${OpenSpaceNet_VERSION_MINOR}")
    set(CPACK_PACKAGE_VERSION_PATCH "${OpenSpaceNet_VERSION_PATCH}")
    set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README.md")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
endif()

include(CPack)
