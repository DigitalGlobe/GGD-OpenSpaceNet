cmake_minimum_required(VERSION 2.8)
project(tests)
enable_testing()
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

find_package(Qt5Test REQUIRED)

# Tell CMake to create the helloworld executable
add_executable(tests localFileValidRun_test.cpp)

add_test(local_file_test local_file_test)

find_package(DeepCore REQUIRED)
if (DeepCore_FOUND)
    message(Include Dirs: "${DEEPCORE_INCLUDE_DIRS}")
    include_directories(${DEEPCORE_INCLUDE_DIRS})
    target_link_libraries(tests ${DEEPCORE_LIBRARIES})
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
    target_link_libraries(tests ${Caffe_LINK})
endif()


INSTALL(TARGETS tests
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )


# Use the Widgets module from Qt 5.
include_directories(../../openspacenet/include)
include_directories(..)
link_directories(openspacenet)
target_link_libraries(tests Qt5::Widgets openspacenet Qt5::Test)