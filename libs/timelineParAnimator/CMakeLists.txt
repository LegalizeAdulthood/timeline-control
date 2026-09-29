add_library(timeline-paranimator
    include/timelineParAnimator/TimelineJson.h
    TimelineJson.cpp
)

target_include_directories(timeline-paranimator PUBLIC include)
target_link_libraries(timeline-paranimator PUBLIC timeline-core)
target_folder(timeline-paranimator "Libraries")
