cmake_minimum_required(VERSION 2.8)
project(OpenSpaceNet-gui)
cmake_policy(SET CMP0015 NEW)
# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run uic automatically when needed
set(CMAKE_AUTOUIC ON)
# Instruct CMake to run moc automatically when needed
set(CMAKE_AUTOMOC ON)
set(CMAKE_PREFIX_PATH $ENV{HOME}/Qt5.7.0/5.7/gcc_64)
# Find the QtWidgets library
find_package(Qt5Widgets REQUIRED)
if (Qt5Widgets_FOUND)
    message(Include Dirs: "${QT5WIDGETS_INCLUDE_DIRS}")
endif()

set(SOURCE_FILES
        src/main.cpp
        src/mainwindow.cpp
        src/processthread.cpp
        src/progresswindow.cpp
        )

set(HEADERS
        include/mainwindow.h
        include/processthread.h
        include/progresswindow.h
        include/qdebugstream.h
        )

add_executable(OpenSpaceNet-gui ${SOURCE_FILES} ${HEADERS})

find_package(DeepCore REQUIRED)
if (DeepCore_FOUND)
    message(Include Dirs: "${DEEPCORE_INCLUDE_DIRS}")
    include_directories(${DEEPCORE_INCLUDE_DIRS})
    target_link_libraries(OpenSpaceNet-gui ${DEEPCORE_LIBRARIES})
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
    target_link_libraries(OpenSpaceNet-gui ${Caffe_LINK})
endif()

INSTALL(TARGETS OpenSpaceNet-gui
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )


# Use the Widgets module from Qt 5.
include_directories(../openspacenet/include)
include_directories(include)
link_directories(openspacenet)
target_link_libraries(OpenSpaceNet-gui Qt5::Widgets openspacenet)