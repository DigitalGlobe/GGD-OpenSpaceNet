cmake_minimum_required(VERSION 2.8)

project(OpenSkyNet)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

find_package(DeepCore REQUIRED)

add_subdirectory(openskynet)
add_subdirectory(App)


