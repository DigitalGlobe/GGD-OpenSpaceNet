set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories(include)

set(HEADERS
        include/OpenSpaceNet.h
        include/OpenSpaceNetArgs.h
        )

set(SOURCES
        src/OpenSpaceNet.cpp
        )

add_library(openspacenet ${SOURCES} ${HEADERS})

# Initialize version.h
configure_file(src/version.h.in OpenSpaceNetVersion.h)

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
