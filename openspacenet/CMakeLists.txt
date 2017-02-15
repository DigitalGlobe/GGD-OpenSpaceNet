set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

# OpenSpaceNet version
set(OpenSpaceNet_VERSION_MAJOR 0)
set(OpenSpaceNet_VERSION_MINOR 4)
set(OpenSpaceNet_VERSION_PATCH 0)

include_directories(include)

set(HEADERS
        include/OpenSpaceNet.h
        include/OpenSpaceNetArgs.h
        )

set(SOURCES
        src/OpenSpaceNet.cpp
        )

add_library(openspacenet ${SOURCES} ${HEADERS})

# Set the build number
if(DEFINED ENV{BUILD_NUMBER})
    set(OpenSpaceNet_VERSION_BUILD $ENV{BUILD_NUMBER})
else()
    set(OpenSpaceNet_VERSION_BUILD "SNAPSHOT")
endif()

# Initialize version.h
configure_file(src/version.h.in OpenSpaceNetVersion.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

find_package(DeepCore REQUIRED)
if (DeepCore_FOUND)
    message(Include Dirs: "${DEEPCORE_INCLUDE_DIRS}")
    include_directories(${DEEPCORE_INCLUDE_DIRS})
        message("DeepCore includes: ${DEEPCORE_INCLUDE_DIRS}")
    target_link_libraries(openspacenet ${DEEPCORE_LIBRARIES})
endif()

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
find_package(Boost COMPONENTS program_options REQUIRED)
if(Boost_FOUND)
    target_link_libraries(openspacenet ${Boost_PROGRAM_OPTIONS_LIBRARY})
endif()



target_link_libraries(openspacenet hdf5_hl hdf5 glog gflags ippicv IlmImf cublas_static curand_static culibos libwebp openblas z pthread)


# Set up include directory for local builds
include_directories(${PROJECT_BINARY_DIR})

INSTALL(TARGETS openspacenet
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )

if (UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_PACKAGE_NAME "OpenSpaceNet")
    set(CPACK_PACKAGE_VENDOR "DigitalGlobe")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "DeepCore Support <deepcore-support@digitalglobe.com>")
    set(CPACK_DEBIAN_PACKAGE_VERSION "${OpenSpaceNet_VERSION_MAJOR}.${OpenSpaceNet_VERSION_MINOR}.${OpenSpaceNet_VERSION_PATCH}+${OpenSpaceNet_VERSION_BUILD}")
    set(CPACK_PACKAGE_VERSION "${OpenSpaceNet_VERSION_MAJOR}.${OpenSpaceNet_VERSION_MINOR}.${OpenSpaceNet_VERSION_PATCH}+${OpenSpaceNet_VERSION_BUILD}")
    set(CPACK_PACKAGE_VERSION_MAJOR "${OpenSpaceNet_VERSION_MAJOR}")
    set(CPACK_PACKAGE_VERSION_MINOR "${OpenSpaceNet_VERSION_MINOR}")
    set(CPACK_PACKAGE_VERSION_PATCH "${OpenSpaceNet_VERSION_PATCH}")
    set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README.md")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
endif()
include(CPack)
