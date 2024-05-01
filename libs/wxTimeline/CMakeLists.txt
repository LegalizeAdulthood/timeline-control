add_library(timeline-wx
    include/wxTimeline/wxTimelineControl.h
    wxTimelineControl.cpp
)
target_include_directories(timeline-wx PUBLIC include)
target_folder(timeline-wx "Libraries")
