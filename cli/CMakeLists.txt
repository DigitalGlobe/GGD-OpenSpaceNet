cmake_minimum_required(VERSION 3.5)

project(OpenSkyNetCLI)

# OpenSkyNet version
set(OpenSkyNet_VERSION_MAJOR 0)
set(OpenSkyNet_VERSION_MINOR 3)


cmake_policy(SET CMP0015 NEW)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -static-libstdc++")

set(SOURCE_FILES
        main.cpp
        ParseCLIArgs.cpp
        )

set(HEADERS
        ParseCLIArgs.h
        )

add_executable(OpenSkyNetCLI ${SOURCE_FILES} ${HEADERS})

add_definitions(-DUSE_OPENCV)

# Set the build number
if(DEFINED ENV{BUILD_NUMBER})
    set(OpenSkyNet_VERSION_BUILD $ENV{BUILD_NUMBER})
else()
    set(OpenSkyNet_VERSION_BUILD "SNAPSHOT")
endif()

# Initialize version.h
configure_file(version.h.in OpenSkyNetVersion.h)

find_package(DeepCore REQUIRED)
if (DeepCore_FOUND)
    message(Include Dirs: "${DEEPCORE_INCLUDE_DIRS}")
    include_directories(${DEEPCORE_INCLUDE_DIRS})
    target_link_libraries(OpenSkyNetCLI ${DEEPCORE_LIBRARIES})
endif()

link_directories(libopenskynet)
target_link_libraries(OpenSkyNetCLI pthread libopenskynet)

INSTALL(TARGETS OpenSkyNetCLI
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )
