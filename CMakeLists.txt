cmake_minimum_required(VERSION 2.8)

project(OpenSpaceNet)

# OpenSpaceNet version
set(OpenSpaceNet_VERSION_MAJOR 0)
set(OpenSpaceNet_VERSION_MINOR 3)
set(OpenSpaceNet_VERSION_PATCH 0)


cmake_policy(SET CMP0015 NEW)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -static-libgcc -static-libstdc++")

set(SOURCE_FILES
        src/main.cpp
        src/OpenSpaceNet.cpp
        src/OpenSpaceNetArgs.cpp
        )

set(HEADERS
        src/OpenSpaceNet.h
        src/OpenSpaceNetArgs.h
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
configure_file(src/version.h.in include/OpenSpaceNetVersion.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)


find_package(DeepCore REQUIRED)
if (DeepCore_FOUND)
    message(Include Dirs: "${DEEPCORE_INCLUDE_DIRS}")
    include_directories(${DEEPCORE_INCLUDE_DIRS})
    target_link_libraries(OpenSpaceNet ${DEEPCORE_LIBRARIES})
    list(APPEND CMAKE_MODULE_PATH ${DEEPCORE_CMAKE_DIR})
endif()


###########################################################################################
# Defines global Caffe_LINK flag, This flag is required to prevent linker from excluding
# some objects which are not addressed directly but are registered via static constructors
if(BUILD_SHARED_LIBS)
    set(Caffe_LINK caffe)
else()
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        set(Caffe_LINK -Wl,-force_load caffe)
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        set(Caffe_LINK -Wl,--whole-archive caffe -Wl,--no-whole-archive)
    endif()
endif()
find_package(Caffe REQUIRED)
if (Caffe_FOUND)
    include_directories(${Caffe_INCLUDE_DIRS})
    target_link_libraries(OpenSpaceNet ${Caffe_LINK})
endif()

find_package(OpenCV REQUIRED)
if (OpenCV_FOUND)
    include_directories(${OpenCV_INCLUDE_DIRS})
    target_link_libraries(OpenSpaceNet ${OpenCV_LIBS})
endif()

find_package(CUDA REQUIRED)
if (CUDA_FOUND)
    include_directories(${CUDA_INCLUDE_DIRS})
    target_link_libraries(OpenSpaceNet ${CUDA_LIBRARIES})
    if(DEEPCORE_STATIC AND UNIX)
        target_link_libraries(OpenSpaceNet culibos)
    endif()
endif()

find_package(GDAL REQUIRED)
if(GDAL_FOUND)
    include_directories(${GDAL_INCLUDE_DIR})
    target_link_libraries(OpenSpaceNet ${GDAL_LIBRARY})
endif()

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.59.0 COMPONENTS regex system filesystem timer coroutine context filesystem date_time program_options chrono thread REQUIRED)

if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    target_link_libraries(OpenSpaceNet ${Boost_LIBRARIES})
endif()

find_package(CURL REQUIRED)
if (CURL_FOUND)
    include_directories(${CURL_INCLUDE_DIRS})
    target_link_libraries(OpenSpaceNet ${CURL_LIBS})
endif()

find_package(LibLZMA)
if (LIBLZMA_FOUND)
    include_directories(${LIBLZMA_INCLUDE_DIRS})
    target_link_libraries(OpenSpaceNet ${LIBLZMA_LIBRARIES})
endif()

find_package(Jsoncpp REQUIRED)
if(JSONCPP_FOUND)
    include_directories(${JSONCPP_INCLUDE_DIRS})
    target_link_libraries(OpenSpaceNet ${JSONCPP_LIBRARIES})
endif()

target_link_libraries(OpenSpaceNet pthread)

INSTALL(TARGETS OpenSpaceNet
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )
