include_directories(src)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5Widgets REQUIRED)

qt5_wrap_cpp(OSN_GUI_MOC
        src/MainWindow.h
        src/OSNProcessingThread.h
        src/progresswindow.h
        src/qdebugstream.h
        )

qt5_wrap_ui(OSN_GUI_UI
        src/MainWindow.ui
        src/progresswindow.ui
        )

set(SOURCES
        src/main.cpp
        src/MainWindow.cpp
        src/MainWindow.h
        src/OSNProcessingThread.cpp
        src/OSNProcessingThread.h
        src/progresswindow.cpp
        src/progresswindow.h
        src/qdebugstream.h
        ${OSN_GUI_MOC}
        ${OSN_GUI_UI}
        )

add_executable(OpenSpaceNet.gui ${SOURCES})

qt5_use_modules(OpenSpaceNet.gui Widgets)
target_link_libraries(OpenSpaceNet.gui OpenSpaceNet.common ${OSN_LINK_LIBRARIES})
set_target_properties(OpenSpaceNet.gui PROPERTIES OUTPUT_NAME OpenSpaceNet-gui)

INSTALL(TARGETS OpenSpaceNet.gui
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )
