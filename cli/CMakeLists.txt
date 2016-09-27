cmake_policy(SET CMP0015 NEW)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
project(openskynet-cli)

# OpenSkyNet version
set(OpenSkyNet_VERSION_MAJOR 0)
set(OpenSkyNet_VERSION_MINOR 3)


cmake_policy(SET CMP0015 NEW)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -static-libgcc -static-libstdc++")

set(SOURCE_FILES
        src/main.cpp
        )


add_executable(openskynet-cli ${SOURCE_FILES})

add_definitions(-DUSE_OPENCV)

# Set the build number
if(DEFINED ENV{BUILD_NUMBER})
    set(OpenSkyNet_VERSION_BUILD $ENV{BUILD_NUMBER})
else()
    set(OpenSkyNet_VERSION_BUILD "SNAPSHOT")
endif()

find_package(DeepCore REQUIRED)
if (DeepCore_FOUND)
    include_directories(${DEEPCORE_INCLUDE_DIRS})
    target_link_libraries(openskynet-cli ${DEEPCORE_LIBRARIES})
endif()

target_link_libraries(openskynet-cli hdf5_hl hdf5 glog gflags ippicv IlmImf cublas_static curand_static culibos libwebp openblas z pthread)

INSTALL(TARGETS openskynet-cli
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )

include_directories(../openskynet/include)
link_directories(openskynet)
target_link_libraries(openskynet-cli openskynet)
# Set up include directory for local builds
include_directories(${PROJECT_BINARY_DIR})
