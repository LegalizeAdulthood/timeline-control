# Copyright (c) 2026 Richard Thomson

add_library(timeline-viewer-formatter STATIC
    include/timelineViewer/format_inspector.h
    format_inspector.cpp)
target_include_directories(timeline-viewer-formatter PUBLIC include)
target_link_libraries(timeline-viewer-formatter PUBLIC timeline-paranimator)
target_folder(timeline-viewer-formatter "Libraries")
