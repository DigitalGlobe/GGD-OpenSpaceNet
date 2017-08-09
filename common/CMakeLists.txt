include(InstallHeaders)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories(include)

set(HEADERS
        include/OpenSpaceNet.h
        include/OpenSpaceNetArgs.h
        )

set(SOURCES
        src/OpenSpaceNet.cpp
        )

add_library(OpenSpaceNet.common ${SOURCES} ${HEADERS})
set_target_properties(OpenSpaceNet.common PROPERTIES OUTPUT_NAME openspacenet)
find_package(DeepCore REQUIRED)
if (DeepCore_FOUND)
    include_directories(${DEEPCORE_INCLUDE_DIRS})
    target_link_libraries(OpenSpaceNet.common ${DEEPCORE_LIBRARIES})
endif()

configure_file(version.h.in ${CMAKE_CURRENT_BINARY_DIR}/OpenSpaceNetVersion.h)
install_public_headers(${HEADERS})

INSTALL(TARGETS OpenSpaceNet.common
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )
