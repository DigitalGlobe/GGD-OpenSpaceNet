cmake_policy(SET CMP0015 NEW)
cmake_minimum_required(VERSION 3.5)

project(OpenSpaceNet C CXX)

# Options for overriding the installation directories
set(INSTALL_LIB_DIR lib CACHE PATH "Installation directory for libraries")
set(INSTALL_BIN_DIR bin CACHE PATH "Installation directory for executables")
set(INSTALL_INCLUDE_DIR include CACHE PATH "Installation directory for header files")

#
# Set up OpenSpaceNet versioning
#
set(OpenSpaceNet_VERSION_MAJOR 1)
set(OpenSpaceNet_VERSION_MINOR 2)
set(OpenSpaceNet_VERSION_PATCH 1)

# Set the build number
# Set the build number
if(NOT DEEPCORE_RELEASE)
    if(DEFINED ENV{BUILD_NUMBER})
        set(OpenSpaceNet_VERSION_BUILD $ENV{BUILD_NUMBER})
    else()
        set(OpenSpaceNet_VERSION_BUILD "SNAPSHOT")
    endif()

    set(OPENSPACENET_VERSION "${OpenSpaceNet_VERSION_MAJOR}.${OpenSpaceNet_VERSION_MINOR}.${OpenSpaceNet_VERSION_PATCH}+${OpenSpaceNet_VERSION_BUILD}")
else()
    set(OPENSPACENET_VERSION "${OpenSpaceNet_VERSION_MAJOR}.${OpenSpaceNet_VERSION_MINOR}.${OpenSpaceNet_VERSION_PATCH}")
    if(${DEEPCORE_RC})
        set(OPENSPACENET_VERSION "${OPENSPACENET_VERSION}-rc${DEEPCORE_RC}")
    endif()
endif()

#
# Common dependencies
#
set(CMAKE_CXX_STANDARD 11)  

if(DEEPCORE_STATIC)
    set(CMAKE_PREFIX_PATH ${DEEPCORE_DEP_DIR})
    set(CMAKE_EXE_LINKER_FLAGS=-static)
    add_definitions(-DCUDA_HOME=${DEEPCORE_DEP_DIR})
    add_definitions(-DCUDA_TOOLKIT_ROOT_DIR=${DEEPCORE_DEP_DIR})
    #setup DeepCore dependency dir.  This will mostly be used in static mode.
    if (NOT DEEPCORE_DEP_DIR)
        if(DEFINED ENV{DEEPCORE_DEP_DIR})
            set(DEEPCORE_DEP_DIR $ENV{DEEPCORE_DEP_DIR})
        else()
            set(DEEPCORE_DEP_DIR /opt/DeepCore)
        endif()
    endif()
    set(CMAKE_CXX_FLAGS "-I ${DEEPCORE_DEP_DIR}/include -L${DEEPCORE_DEP_DIR}/lib -L${DEEPCORE_DEP_DIR}/lib64 ${CMAKE_CXX_FLAGS}")    
endif()

# From the CaffeConfig.cmake file
#   Caffe and this config file depends on opencv,
#   so put `find_package(OpenCV)` before searching Caffe
#   via `find_package(Caffe)`. All other lib/includes
#   dependencies are hard coded in the file
find_package(OpenCV)
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

find_package(CUDA REQUIRED)
if (CUDA_FOUND)
    include_directories(${CUDA_INCLUDE_DIRS})
    list(APPEND OSN_LINK_LIBRARIES ${CUDA_LIBRARIES})
endif()

find_package(Jsoncpp REQUIRED)
if(JSONCPP_FOUND)
    include_directories(${JSONCPP_INCLUDE_DIRS})
    list(APPEND OSN_LINK_LIBRARIES ${JSONCPP_LIBRARIES})
endif()

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
find_package(Boost COMPONENTS program_options REQUIRED)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    list(APPEND OSN_LINK_LIBRARIES ${Boost_PROGRAM_OPTIONS_LIBRARY})
endif()

find_package(Threads)
list(APPEND OSN_LINK_LIBRARIES ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS})

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
    set(CPACK_DEBIAN_PACKAGE_VERSION "${OpenSpaceNet_VERSION}")
    set(CPACK_PACKAGE_VERSION "${OpenSpaceNet_VERSION}")
    set(CPACK_PACKAGE_VERSION_MAJOR "${OpenSpaceNet_VERSION_MAJOR}")
    set(CPACK_PACKAGE_VERSION_MINOR "${OpenSpaceNet_VERSION_MINOR}")
    set(CPACK_PACKAGE_VERSION_PATCH "${OpenSpaceNet_VERSION_PATCH}")
    set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README.md")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
endif()

include(CPack)
