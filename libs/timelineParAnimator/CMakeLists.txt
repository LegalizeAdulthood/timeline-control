find_package(nlohmann_json CONFIG REQUIRED)

add_library(timeline-paranimator
    include/timelineParAnimator/BeatKeysMapping.h
    include/timelineParAnimator/TimelineJson.h
    AttributeStrings.h
    AttributeStrings.cpp
    BeatKeysMapping.cpp
    ColorMap.h
    ColorMap.cpp
    NamedColors.h
    NamedColors.cpp
    ParameterCatalog.h
    ParameterCatalog.cpp
    TimelineJson.cpp
)

target_include_directories(timeline-paranimator
    PUBLIC include
    PRIVATE .
)
target_link_libraries(timeline-paranimator
    PUBLIC
        timeline-core
    PRIVATE
        nlohmann_json::nlohmann_json
)
target_folder(timeline-paranimator "Libraries")
