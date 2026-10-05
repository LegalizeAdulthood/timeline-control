add_subdirectory(test-timeline-core)
add_subdirectory(test-timeline-paranimator)
add_subdirectory(test-timeline-viewer-formatter)
if(TIMELINE_CONTROL_WITH_WX)
    add_subdirectory(test-timeline-wx)
endif()

if(TIMELINE_CONTROL_WITH_IMGUI)
    add_subdirectory(test-timeline-imgui)
endif()

if(TIMELINE_CONTROL_WITH_QT)
    add_subdirectory(test-timeline-qt)
endif()
