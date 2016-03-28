cmake_policy(SET CMP0015 NEW)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fPIC")

include_directories(include
    ${DEEPCORE_INCLUDE_DIR}
    )

add_library(openskynet SHARED 
    src/libopenskynet.cpp 
    src/WorkItem.cpp
    )

find_package(CUDA REQUIRED)
if (CUDA_FOUND)
    include_directories(${CUDA_INCLUDE_DIRS})
    target_link_libraries(openskynet ${CUDA_LIBRARIES})
endif()

find_package(OpenCV REQUIRED)
if (OpenCV_FOUND)
    include_directories(${OpenCV_INCLUDE_DIRS})
    target_link_libraries(openskynet ${OpenCV_LIBS})
endif()

find_package(Caffe)
if (Caffe_FOUND)
    include_directories(${Caffe_INCLUDE_DIRS})
    target_link_libraries(openskynet ${Caffe_LIBRARIES})
endif()




find_package(GDAL REQUIRED)
if(GDAL_FOUND)
    include_directories(${GDAL_INCLUDE_DIR})
    target_link_libraries(openskynet ${GDAL_LIBRARY})
endif()

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.54.0 REQUIRED COMPONENTS timer system filesystem thread date_time)

if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    target_link_libraries(openskynet ${Boost_LIBRARIES})

endif()

find_package(CURL REQUIRED)
if (CURL_FOUND)
    include_directories(${CURL_INCLUDE_DIRS})
    target_link_libraries(openskynet ${CURL_LIBS})
endif()

include_directories(/usr/local/include/curlpp)
target_link_libraries(openskynet /usr/local/lib/libcurlpp.so)


ENABLE_TESTING()

INSTALL(TARGETS openskynet
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )
