# Copyright (c) 2026 Richard Thomson

add_library(timeline-qt include/qtTimeline/QTimelineWidget.h QTimelineWidget.cpp
    include/qtTimeline/Renderer.h Renderer.cpp)
set_target_properties(timeline-qt PROPERTIES AUTOMOC ON)
target_include_directories(timeline-qt PUBLIC include)
target_link_libraries(timeline-qt PUBLIC timeline-core Qt6::Widgets)
target_folder(timeline-qt "Libraries")

