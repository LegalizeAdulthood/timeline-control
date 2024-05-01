add_library(timeline-core
    include/timeline/Time.h
    Time.cpp
)

target_include_directories(timeline-core PUBLIC include)
