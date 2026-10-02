add_subdirectory(test-timeline-core)
add_subdirectory(test-timeline-paranimator)
if(TIMELINE_CONTROL_WITH_WX)
    add_subdirectory(test-timeline-wx)
endif()

if(TIMELINE_CONTROL_WITH_IMGUI)
    add_subdirectory(test-timeline-imgui)
endif()
