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
        timeline-viewer-io
        timeline-wx
)
configure_timeline_viewer(wx-timeline-viewer OFF)
target_folder(wx-timeline-viewer "Tools")

if(BUILD_TESTING)
    add_test(NAME WxViewerSmoke COMMAND wx-timeline-viewer --smoke-test
        "${PROJECT_SOURCE_DIR}/tests/test-timeline-paranimator/fixtures/extreme-normalized-vectors.json"
        "${PROJECT_SOURCE_DIR}/tests/test-timeline-paranimator/fixtures/beat-keys/rms.beat-keys.json"
        "${PROJECT_SOURCE_DIR}/tests/test-timeline-paranimator/fixtures/invalid-schema.json"
        "${CMAKE_CURRENT_BINARY_DIR}/wx-timeline-viewer-smoke.txt")
endif()

if(TIMELINE_CONTROL_WITH_CAIRO)
    add_executable(cairo-timeline-viewer WIN32 main.cpp)
    target_link_libraries(cairo-timeline-viewer
        PRIVATE
            timeline-paranimator
            timeline-viewer-formatter
            timeline-viewer-io
            timeline-wx
    )
    configure_timeline_viewer(cairo-timeline-viewer ON)
    target_folder(cairo-timeline-viewer "Tools")
    if(BUILD_TESTING)
        add_test(NAME CairoViewerSmoke COMMAND cairo-timeline-viewer --smoke-test
            "${PROJECT_SOURCE_DIR}/tests/test-timeline-paranimator/fixtures/extreme-normalized-vectors.json"
            "${PROJECT_SOURCE_DIR}/tests/test-timeline-paranimator/fixtures/beat-keys/rms.beat-keys.json"
            "${PROJECT_SOURCE_DIR}/tests/test-timeline-paranimator/fixtures/invalid-schema.json"
            "${CMAKE_CURRENT_BINARY_DIR}/cairo-timeline-viewer-smoke.txt")
    endif()
endif()
