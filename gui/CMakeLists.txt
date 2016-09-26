cmake_minimum_required(VERSION 2.8)
project(gui)
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


# Tell CMake to create the helloworld executable
add_executable(gui main.cpp mainwindow.cpp)

find_package(DeepCore REQUIRED)
if (DeepCore_FOUND)
    message(Include Dirs: "${DEEPCORE_INCLUDE_DIRS}")
    include_directories(${DEEPCORE_INCLUDE_DIRS})
    target_link_libraries(gui ${DEEPCORE_LIBRARIES})
endif()
# Use the Widgets module from Qt 5.
link_directories(openskynet)
target_link_libraries(gui Qt5::Widgets)