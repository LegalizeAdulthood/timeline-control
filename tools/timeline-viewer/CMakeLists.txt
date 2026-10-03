add_executable(timeline-viewer WIN32 main.cpp)
target_link_libraries(timeline-viewer
    PRIVATE
        timeline-paranimator
        timeline-wx
)
target_folder(timeline-viewer "Tools")

if(TIMELINE_CONTROL_WITH_CAIRO)
    target_compile_definitions(timeline-viewer PRIVATE TIMELINE_CONTROL_WITH_CAIRO)
endif()
