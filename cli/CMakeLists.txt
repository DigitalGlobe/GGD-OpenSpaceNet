include_directories(src)

set(SOURCES
        src/main.cpp
        src/CliProcessor.cpp
        src/CliProcessor.h
        )

add_executable(OpenSpaceNet.cli ${SOURCES})
set_target_properties(OpenSpaceNet.cli PROPERTIES OUTPUT_NAME OpenSpaceNet)

target_link_libraries(OpenSpaceNet.cli OpenSpaceNet.common)

find_package(DeepCore REQUIRED)
if (DeepCore_FOUND)
    include_directories(${DEEPCORE_INCLUDE_DIRS})
    target_link_libraries(OpenSpaceNet.cli ${DEEPCORE_LIBRARIES})
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
    target_link_libraries(OpenSpaceNet.cli ${Caffe_LINK})
endif()


find_package(OpenCV REQUIRED)
if (OpenCV_FOUND)
    include_directories(${OpenCV_INCLUDE_DIRS})
    target_link_libraries(OpenSpaceNet.cli  ${OpenCV_LIBS})
endif()



set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
find_package(Boost COMPONENTS program_options REQUIRED)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    target_link_libraries(OpenSpaceNet.cli ${Boost_PROGRAM_OPTIONS_LIBRARY})
endif()

INSTALL(TARGETS OpenSpaceNet.cli
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )

