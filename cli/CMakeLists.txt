cmake_minimum_required(VERSION 2.8)
project(openspacenet-cli)

# OpenSpaceNet version
set(OpenSpaceNet_VERSION_MAJOR 0)
set(OpenSpaceNet_VERSION_MINOR 4)
set(OpenSpaceNet_VERSION_PATCH 0)


cmake_policy(SET CMP0015 NEW)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -static-libgcc -static-libstdc++")

set(SOURCE_FILES
        src/main.cpp
        src/ParseCLIArgs.cpp
        )

set(HEADERS
        include/ParseCLIArgs.h
        )


add_executable(openspacenet-cli ${SOURCE_FILES} ${HEADERS})

add_definitions(-DUSE_OPENCV)

# Set the build number
if(DEFINED ENV{BUILD_NUMBER})
    set(OpenSpaceNet_VERSION_BUILD $ENV{BUILD_NUMBER})
else()
    set(OpenSpaceNet_VERSION_BUILD "SNAPSHOT")
endif()

# Initialize version.h
configure_file(src/version.h.in OpenSpaceNetVersion.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)

find_package(DeepCore REQUIRED)
if (DeepCore_FOUND)
    include_directories(${DEEPCORE_INCLUDE_DIRS})
    target_link_libraries(openspacenet-cli ${DEEPCORE_LIBRARIES})
    list(APPEND CMAKE_MODULE_PATH ${DEEPCORE_CMAKE_DIR})
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
    target_link_libraries(openspacenet-cli ${Caffe_LINK})
endif()

target_link_libraries(openspacenet-cli hdf5_hl hdf5 glog gflags ippicv IlmImf cublas_static curand_static culibos libwebp openblas z pthread)

INSTALL(TARGETS openspacenet-cli
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )

include_directories(../openspacenet/include)
include_directories(include)
link_directories(openspacenet)
target_link_libraries(openspacenet-cli openspacenet)
# Set up include directory for local builds
include_directories(${PROJECT_BINARY_DIR})

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
    set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/../README.md")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
endif()
include(CPack)