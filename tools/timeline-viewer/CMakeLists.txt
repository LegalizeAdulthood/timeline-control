function(configure_timeline_viewer target with_cairo)
    set(TIMELINE_CONTROL_WITH_CAIRO ${with_cairo})
    set(config_directory "${CMAKE_CURRENT_BINARY_DIR}/include/${target}")
    configure_file(config.h.in "${config_directory}/wxTimeline/config.h")
    target_sources(${target} PRIVATE "${config_directory}/wxTimeline/config.h")
    target_include_directories(${target} PRIVATE "${config_directory}")
endfunction()

add_executable(wx-timeline-viewer WIN32 main.cpp)
target_link_libraries(wx-timeline-viewer
    PRIVATE
        timeline-paranimator
        timeline-viewer-formatter
        timeline-wx
)
configure_timeline_viewer(wx-timeline-viewer OFF)
target_folder(wx-timeline-viewer "Tools")

if(TIMELINE_CONTROL_WITH_CAIRO)
    add_executable(cairo-timeline-viewer WIN32 main.cpp)
    target_link_libraries(cairo-timeline-viewer
        PRIVATE
            timeline-paranimator
            timeline-viewer-formatter
            timeline-wx
    )
    configure_timeline_viewer(cairo-timeline-viewer ON)
    target_folder(cairo-timeline-viewer "Tools")
endif()
