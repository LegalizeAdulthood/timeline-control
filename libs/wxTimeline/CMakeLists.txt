find_package(wxWidgets CONFIG REQUIRED)

add_library(timeline-wx
    include/wxTimeline/wxTimelineControl.h
    include/wxTimeline/wxTimelineRenderer.h
    wxTimelineControl.cpp
    wxTimelineRenderer.cpp
)
target_include_directories(timeline-wx PUBLIC include)
target_link_libraries(timeline-wx
    PUBLIC
        timeline-core
        wx::base
        wx::core
)
target_folder(timeline-wx "Libraries")
