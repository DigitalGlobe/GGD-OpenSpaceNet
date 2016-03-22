cmake_policy(SET CMP0015 NEW)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fPIC")

set(SOURCE_FILES main.cpp)
include_directories(openskynet/include
    ${DEEPCORE_INCLUDE_DIR}
    )

link_directories(openskynet)
add_executable(OpenSkyNet ${SOURCE_FILES})
target_link_libraries(OpenSkyNet openskynet ${DEEPCORE_LIBRARIES} ${CURL_LIBRARY})

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.54.0 COMPONENTS program_options)

if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    target_link_libraries(OpenSkyNet  ${Boost_LIBRARIES})

endif()

set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /usr/local/FileGDB_API/lib)

INSTALL(TARGETS OpenSkyNet
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )
