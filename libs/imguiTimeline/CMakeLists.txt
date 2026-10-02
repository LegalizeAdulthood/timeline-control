# Copyright (c) 2026 Richard Thomson

add_library(timeline-imgui
    include/imguiTimeline/TimelineControl.h
    include/imguiTimeline/TimelineRenderer.h
    TimelineControl.cpp
    TimelineRenderer.cpp
)
target_include_directories(timeline-imgui PUBLIC include)
target_link_libraries(timeline-imgui PUBLIC timeline-core imgui::imgui)
target_folder(timeline-imgui "Libraries")
