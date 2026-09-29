add_executable(timeline-viewer WIN32 main.cpp)
target_link_libraries(timeline-viewer
    PRIVATE
        timeline-paranimator
        timeline-wx
)
target_folder(timeline-viewer "Tools")
