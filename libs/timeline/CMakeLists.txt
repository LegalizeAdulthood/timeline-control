add_library(timeline-core
    include/timeline/Attributes.h
    include/timeline/Curve.h
    include/timeline/Document.h
    include/timeline/DisplayList.h
    include/timeline/Envelope.h
    include/timeline/Event.h
    include/timeline/Interval.h
    include/timeline/Interaction.h
    include/timeline/Keyframe.h
    include/timeline/Lane.h
    include/timeline/Layout.h
    include/timeline/Query.h
    include/timeline/Palette.h
    include/timeline/Snapshot.h
    include/timeline/size_cast.h
    include/timeline/StringTable.h
    include/timeline/Time.h
    Attributes.cpp
    Curve.cpp
    DisplayList.cpp
    Document.cpp
    Envelope.cpp
    Event.cpp
    Interval.cpp
    Interaction.cpp
    Keyframe.cpp
    Lane.cpp
    Layout.cpp
    Query.cpp
    Palette.cpp
    Snapshot.cpp
    StringTable.cpp
    Time.cpp
)

target_include_directories(timeline-core PUBLIC include)
target_folder(timeline-core "Libraries")
