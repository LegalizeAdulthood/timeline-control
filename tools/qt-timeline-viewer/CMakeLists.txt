# Copyright (c) 2026 Richard Thomson

add_library(qt-viewer-core STATIC Viewer.h Viewer.cpp)
target_include_directories(qt-viewer-core PUBLIC .)
target_link_libraries(qt-viewer-core PUBLIC
    timeline-qt
    timeline-paranimator
    timeline-viewer-formatter
    timeline-viewer-io)
target_folder(qt-viewer-core "Libraries")
qt_add_executable(qt-timeline-viewer WIN32 main.cpp)
target_link_libraries(qt-timeline-viewer PRIVATE qt-viewer-core)
target_folder(qt-timeline-viewer "Tools")
if(WIN32)
    deploy_qt_plugins(qt-timeline-viewer QWindowsIntegrationPlugin)
elseif(APPLE)
    deploy_qt_plugins(qt-timeline-viewer QCocoaIntegrationPlugin)
else()
    deploy_qt_plugins(qt-timeline-viewer QXcbIntegrationPlugin)
endif()

