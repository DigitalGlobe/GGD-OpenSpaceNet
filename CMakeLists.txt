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

find_package(DeepCore REQUIRED)
if (DeepCore_FOUND)
    include_directories(${DEEPCORE_INCLUDE_DIRS})
    list(APPEND CMAKE_MODULE_PATH ${DEEPCORE_CMAKE_DIR})
    list(APPEND OSN_LINK_LIBRARIES ${DEEPCORE_LIBRARIES})
endif()

find_package(Caffe REQUIRED)
if (Caffe_FOUND)
    if (DEEPCORE_STATIC)
        if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
            set(Caffe_LINK -Wl,-force_load caffe)
        elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
            set(Caffe_LINK -Wl,--whole-archive caffe -Wl,--no-whole-archive)
        endif()
    else()
        set(Caffe_LINK caffe)
    endif()
    include_directories(${Caffe_INCLUDE_DIRS})
    list(APPEND OSN_LINK_LIBRARIES ${Caffe_LINK})
endif()

find_package(Jsoncpp REQUIRED)
if(JSONCPP_FOUND)
    include_directories(${JSONCPP_INCLUDE_DIRS})
    list(APPEND OSN_LINK_LIBRARIES ${Caffe_LINK})
endif()

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
find_package(Boost COMPONENTS program_options REQUIRED)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    list(APPEND OSN_LINK_LIBRARIES ${Boost_PROGRAM_OPTIONS_LIBRARY})
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
