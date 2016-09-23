include_directories(include)

set(SOURCE_FILES
        OpenSkyNet.cpp
        )

set(HEADERS
        OpenSkyNetArgs.h
        OpenSkyNet.h
        )

add_library(openskynet ${SOURCE_FILES} ${HEADERS})

find_package(DeepCore REQUIRED)
if (DeepCore_FOUND)
    message(Include Dirs: "${DEEPCORE_INCLUDE_DIRS}")
    include_directories(${DEEPCORE_INCLUDE_DIRS})
    target_link_libraries(openskynet ${DEEPCORE_LIBRARIES})
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
    target_link_libraries(openskynet ${Caffe_LINK})
endif()

find_package(OpenCV REQUIRED)
if (OpenCV_FOUND)
    include_directories(${OpenCV_INCLUDE_DIRS})
    target_link_libraries(openskynet ${OpenCV_LIBS})
endif()

find_package(CUDA REQUIRED)
if (CUDA_FOUND)
    include_directories(${CUDA_INCLUDE_DIRS})
    target_link_libraries(openskynet ${CUDA_LIBRARIES})
    target_link_libraries(openskynet libculibos.a)
endif()

find_package(GDAL REQUIRED)
if(GDAL_FOUND)
    include_directories(${GDAL_INCLUDE_DIR})
    target_link_libraries(openskynet ${GDAL_LIBRARY})
    target_link_libraries(openskynet libtiff.a)
    target_link_libraries(openskynet libpng.a)
    target_link_libraries(openskynet libz.a)
    target_link_libraries(openskynet libhdf5.a)
    target_link_libraries(openskynet libjpeg.a)
    target_link_libraries(openskynet libturbojpeg.a)
    target_link_libraries(openskynet /opt/DeepCore/pgsql/lib/libpq.a)
    target_link_libraries(openskynet libcurl.a)
    target_link_libraries(openskynet libssl.a)
    target_link_libraries(openskynet libcrypto.a)
    target_link_libraries(openskynet libproj.a)
    target_link_libraries(openskynet liblzma.a)
endif()

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.59.0 COMPONENTS regex system filesystem timer coroutine context filesystem date_time program_options chrono thread REQUIRED)

if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    target_link_libraries(openskynet ${Boost_LIBRARIES})
endif()

find_package(CURL REQUIRED)
if (CURL_FOUND)
    include_directories(${CURL_INCLUDE_DIRS})
    target_link_libraries(openskynet ${CURL_LIBS})
endif()

find_package(LibLZMA)
if (LIBLZMA_FOUND)
    include_directories(${LIBLZMA_INCLUDE_DIRS})
    target_link_libraries(openskynet ${LIBLZMA_LIBRARIES})
endif()

install_public_headers(openskynet ${HEADERS})

install(TARGETS openskynet
        EXPORT OpenSkynetTargets
        ARCHIVE DESTINATION "${INSTALL_LIB_DIR}/OpenSkyNet" COMPONENT shlib
        COMPONENT dev
        )
