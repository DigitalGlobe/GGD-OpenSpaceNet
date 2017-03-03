include_directories(src)

set(SOURCES
        src/main.cpp
        src/CliArgsParser.cpp
        src/CliArgsParser.h
        )

add_executable(OpenSpaceNet.cli ${SOURCES})
target_link_libraries(OpenSpaceNet.cli OpenSpaceNet.common ${OSN_LINK_LIBRARIES})
set_target_properties(OpenSpaceNet.cli PROPERTIES OUTPUT_NAME OpenSpaceNet)

INSTALL(TARGETS OpenSpaceNet.cli
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )

