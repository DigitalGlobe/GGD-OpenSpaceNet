cmake_minimum_required(VERSION 2.8)
project(OpenSpaceNet)

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
        src/ParseCLIArgs.h
        )


add_executable(OpenSpaceNet ${SOURCE_FILES} ${HEADERS})

add_definitions(-DUSE_OPENCV)

# Set the build number
if(DEFINED ENV{BUILD_NUMBER})
    set(OpenSpaceNet_VERSION_BUILD $ENV{BUILD_NUMBER})
else()
    set(OpenSpaceNet_VERSION_BUILD "SNAPSHOT")
endif()

# Initialize version.h
configure_file(src/version.h.in OpenSpaceNetVersion.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

find_package(DeepCore REQUIRED)
if (DeepCore_FOUND)
    include_directories(${DEEPCORE_INCLUDE_DIRS})
    target_link_libraries(OpenSpaceNet ${DEEPCORE_LIBRARIES})
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
    target_link_libraries(OpenSpaceNet ${Caffe_LINK})
endif()

target_link_libraries(OpenSpaceNet hdf5_hl hdf5 glog gflags ippicv IlmImf cublas_static curand_static culibos libwebp openblas z pthread)

INSTALL(TARGETS OpenSpaceNet
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )

include_directories(../common/include)
include_directories(include)
link_directories(openspacenet)
target_link_libraries(OpenSpaceNet openspacenet)
# Set up include directory for local builds
include_directories(${PROJECT_BINARY_DIR})
