cmake_policy(SET CMP0015 NEW)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -static-libgcc -static-libstdc++")

set(SOURCE_FILES main.cpp src/OpenSkyNet.cpp)
set(HEADERS include/OpenSkyNet.h include/OpenSkyNetArgs.h)
include_directories(include)
add_executable(OpenSkyNet ${SOURCE_FILES} ${HEADERS})

add_definitions(-DUSE_OPENCV)

# DeepCore version
set(OpenSkyNet_VERSION_MAJOR 0)
set(OpenSkyNet_VERSION_MINOR 2)

# Set the build number
if(DEFINED ENV{BUILD_NUMBER})
    set(OpenSkyNet_VERSION_BUILD $ENV{BUILD_NUMBER})
else()
    set(OpenSkyNet_VERSION_BUILD "SNAPSHOT")
endif()

# Initialize version.h
configure_file(version.h.in include/version.h)

# Set up include directory for local builds
include_directories(${PROJECT_BINARY_DIR}/App/include)


find_package(DeepCore REQUIRED)
if (DeepCore_FOUND)
    message(Include Dirs: "${DEEPCORE_INCLUDE_DIRS}")
    include_directories(${DEEPCORE_INCLUDE_DIRS})
    target_link_libraries(OpenSkyNet ${DEEPCORE_LIBRARIES})
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
    target_link_libraries(OpenSkyNet ${Caffe_LINK})
endif()

find_package(OpenCV REQUIRED)
if (OpenCV_FOUND)
    include_directories(${OpenCV_INCLUDE_DIRS})
    target_link_libraries(OpenSkyNet ${OpenCV_LIBS})
endif()

find_package(CUDA REQUIRED)
if (CUDA_FOUND)
    include_directories(${CUDA_INCLUDE_DIRS})
    target_link_libraries(OpenSkyNet ${CUDA_LIBRARIES})
    target_link_libraries(OpenSkyNet libculibos.a)
endif()

find_package(GDAL REQUIRED)
if(GDAL_FOUND)
    include_directories(${GDAL_INCLUDE_DIR})
    target_link_libraries(OpenSkyNet ${GDAL_LIBRARY})
    target_link_libraries(OpenSkyNet libtiff.a)
    target_link_libraries(OpenSkyNet libpng.a)
    target_link_libraries(OpenSkyNet libz.a)
    target_link_libraries(OpenSkyNet libhdf5.a)
    target_link_libraries(OpenSkyNet libjpeg.a)
    target_link_libraries(OpenSkyNet libturbojpeg.a)
    target_link_libraries(OpenSkyNet /opt/DeepCore/pgsql/lib/libpq.a)
    target_link_libraries(OpenSkyNet /opt/DeepCore/FileGDB_API-64/lib/libFileGDBAPI.so)
    target_link_libraries(OpenSkyNet /opt/DeepCore/FileGDB_API-64/lib/libfgdbunixrtl.so)
    target_link_libraries(OpenSkyNet libcurl.a)
    target_link_libraries(OpenSkyNet libssl.a)
    target_link_libraries(OpenSkyNet libcrypto.a)
    target_link_libraries(OpenSkyNet libproj.a)
    target_link_libraries(OpenSkyNet liblzma.a)
endif()

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.59.0 COMPONENTS regex system filesystem timer coroutine context filesystem date_time program_options chrono thread REQUIRED)

if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    target_link_libraries(OpenSkyNet ${Boost_LIBRARIES})
endif()

find_package(CURL REQUIRED)
if (CURL_FOUND)
    include_directories(${CURL_INCLUDE_DIRS})
    target_link_libraries(OpenSkyNet ${CURL_LIBS})
endif()

include_directories(/opt/DeepCore/include/curlpp)
target_link_libraries(OpenSkyNet /opt/DeepCore/lib/libcurlpp.a)

find_package(LibLZMA)
if (LIBLZMA_FOUND)
    include_directories(${LIBLZMA_INCLUDE_DIRS})
    target_link_libraries(OpenSkyNet ${LIBLZMA_LIBRARIES})
endif()

target_link_libraries(OpenSkyNet pthread)

INSTALL(TARGETS OpenSkyNet
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )