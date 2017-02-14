cmake_policy(SET CMP0015 NEW)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
cmake_minimum_required(VERSION 3.5)
ENABLE_LANGUAGE(C)
project(OpenSpaceNet CXX)

add_subdirectory(openspacenet)
add_subdirectory(cli)
add_subdirectory(gui)
add_subdirectory(gui/tests)

