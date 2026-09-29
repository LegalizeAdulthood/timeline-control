find_package(nlohmann_json CONFIG REQUIRED)

add_library(timeline-paranimator
    include/timelineParAnimator/TimelineJson.h
    TimelineJson.cpp
)

target_include_directories(timeline-paranimator PUBLIC include)
target_link_libraries(timeline-paranimator
    PUBLIC
        timeline-core
    PRIVATE
        nlohmann_json::nlohmann_json
)
target_folder(timeline-paranimator "Libraries")
