add_library(timeline-core
    include/timeline/Document.h
    include/timeline/Time.h
    Document.cpp
    Time.cpp
)

target_include_directories(timeline-core PUBLIC include)
target_folder(timeline-core "Libraries")
