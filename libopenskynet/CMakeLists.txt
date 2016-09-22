include_directories(include)

set(SOURCE_FILES
        OpenSkyNet.cpp
        )

set(HEADERS
        OpenSkyNetArgs.h
        OpenSkyNet.h
        )

add_library(libopenskynet ${SOURCE_FILES} ${HEADERS})

find_package(DeepCore REQUIRED)
if (DeepCore_FOUND)
    message(Include Dirs: "${DEEPCORE_INCLUDE_DIRS}")
    include_directories(${DEEPCORE_INCLUDE_DIRS})
    target_link_libraries(libopenskynet ${DEEPCORE_LIBRARIES})
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
    target_link_libraries(libopenskynet ${Caffe_LINK})
endif()

find_package(OpenCV REQUIRED)
if (OpenCV_FOUND)
    include_directories(${OpenCV_INCLUDE_DIRS})
    target_link_libraries(libopenskynet ${OpenCV_LIBS})
endif()

find_package(CUDA REQUIRED)
if (CUDA_FOUND)
    include_directories(${CUDA_INCLUDE_DIRS})
    target_link_libraries(libopenskynet ${CUDA_LIBRARIES})
    target_link_libraries(libopenskynet libculibos.a)
endif()

find_package(GDAL REQUIRED)
if(GDAL_FOUND)
    include_directories(${GDAL_INCLUDE_DIR})
    target_link_libraries(libopenskynet ${GDAL_LIBRARY})
    target_link_libraries(libopenskynet libtiff.a)
    target_link_libraries(libopenskynet libpng.a)
    target_link_libraries(libopenskynet libz.a)
    target_link_libraries(libopenskynet libhdf5.a)
    target_link_libraries(libopenskynet libjpeg.a)
    target_link_libraries(libopenskynet libturbojpeg.a)
    target_link_libraries(libopenskynet /opt/DeepCore/pgsql/lib/libpq.a)
    target_link_libraries(libopenskynet libcurl.a)
    target_link_libraries(libopenskynet libssl.a)
    target_link_libraries(libopenskynet libcrypto.a)
    target_link_libraries(libopenskynet libproj.a)
    target_link_libraries(libopenskynet liblzma.a)
endif()

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.59.0 COMPONENTS regex system filesystem timer coroutine context filesystem date_time program_options chrono thread REQUIRED)

if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    target_link_libraries(libopenskynet ${Boost_LIBRARIES})
endif()

find_package(CURL REQUIRED)
if (CURL_FOUND)
    include_directories(${CURL_INCLUDE_DIRS})
    target_link_libraries(libopenskynet ${CURL_LIBS})
endif()

find_package(LibLZMA)
if (LIBLZMA_FOUND)
    include_directories(${LIBLZMA_INCLUDE_DIRS})
    target_link_libraries(libopenskynet ${LIBLZMA_LIBRARIES})
endif()

install_public_headers(libopenskynet ${HEADERS})

install(TARGETS libopenskynet
        EXPORT OpenSkynetTargets
        ARCHIVE DESTINATION "${INSTALL_LIB_DIR}/OpenSkyNet" COMPONENT shlib
        COMPONENT dev
        )
