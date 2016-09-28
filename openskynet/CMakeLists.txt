set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories(include)

set(HEADERS
        include/OpenSkyNet.h
        include/OpenSkyNetArgs.h
        )

set(SOURCES
        src/OpenSkyNet.cpp
        )

add_library(openskynet ${SOURCES} ${HEADERS})

# Initialize version.h
configure_file(src/version.h.in OpenSkyNetVersion.h)

find_package(DeepCore REQUIRED)
if (DeepCore_FOUND)
    message(Include Dirs: "${DEEPCORE_INCLUDE_DIRS}")
    include_directories(${DEEPCORE_INCLUDE_DIRS})
        message("DeepCore includes: ${DEEPCORE_INCLUDE_DIRS}")
    target_link_libraries(openskynet ${DEEPCORE_LIBRARIES})
endif()

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
find_package(Boost COMPONENTS program_options REQUIRED)
if(Boost_FOUND)
    target_link_libraries(openskynet ${Boost_PROGRAM_OPTIONS_LIBRARY})
endif()



target_link_libraries(openskynet hdf5_hl hdf5 glog gflags ippicv IlmImf cublas_static curand_static culibos libwebp openblas z pthread)


# Set up include directory for local builds
include_directories(${PROJECT_BINARY_DIR})

INSTALL(TARGETS openskynet
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )
