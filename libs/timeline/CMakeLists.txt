add_library(timeline-core
    include/timeline/Curve.h
    include/timeline/Document.h
    include/timeline/DisplayList.h
    include/timeline/Envelope.h
    include/timeline/Event.h
    include/timeline/Interval.h
    include/timeline/Keyframe.h
    include/timeline/Lane.h
    include/timeline/Layout.h
    include/timeline/Query.h
    include/timeline/size_cast.h
    include/timeline/Time.h
    Curve.cpp
    Document.cpp
    Envelope.cpp
    Event.cpp
    Interval.cpp
    Keyframe.cpp
    Lane.cpp
    Layout.cpp
    Query.cpp
    Time.cpp
)

target_include_directories(timeline-core PUBLIC include)
target_folder(timeline-core "Libraries")
