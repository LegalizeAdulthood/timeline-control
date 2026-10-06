# Copyright (c) 2026 Richard Thomson

add_library(timeline-viewer-io STATIC
    include/timelineViewer/load_timeline.h
    include/timelineViewer/write_snapshot.h
    load_timeline.cpp
    write_snapshot.cpp)
target_include_directories(timeline-viewer-io PUBLIC include)
target_link_libraries(timeline-viewer-io PUBLIC timeline-paranimator)
target_folder(timeline-viewer-io "Libraries")
