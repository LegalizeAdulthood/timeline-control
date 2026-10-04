add_executable(timeline-viewer WIN32 main.cpp)
target_link_libraries(timeline-viewer
    PRIVATE
        timeline-paranimator
        timeline-wx
)
target_folder(timeline-viewer "Tools")

if(TIMELINE_CONTROL_WITH_CAIRO)
    add_executable(cairo-timeline-viewer WIN32 main.cpp)
    target_link_libraries(cairo-timeline-viewer
        PRIVATE
            timeline-paranimator
            timeline-wx
    )
    target_compile_definitions(cairo-timeline-viewer PRIVATE TIMELINE_CONTROL_WITH_CAIRO)
    target_folder(cairo-timeline-viewer "Tools")
endif()
