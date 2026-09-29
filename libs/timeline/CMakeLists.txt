add_library(timeline-core
    include/timeline/Document.h
    include/timeline/DisplayList.h
    include/timeline/Envelope.h
    include/timeline/Event.h
    include/timeline/Interval.h
    include/timeline/Lane.h
    include/timeline/Layout.h
    include/timeline/size_cast.h
    include/timeline/Time.h
    Document.cpp
    Envelope.cpp
    Event.cpp
    Interval.cpp
    Lane.cpp
    Layout.cpp
    Time.cpp
)

target_include_directories(timeline-core PUBLIC include)
target_folder(timeline-core "Libraries")
