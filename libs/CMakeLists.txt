add_subdirectory(timeline)
add_subdirectory(timelineParAnimator)

if(TIMELINE_CONTROL_WITH_WX)
    add_subdirectory(wxTimeline)
endif()

if(TIMELINE_CONTROL_WITH_IMGUI)
    add_subdirectory(imguiTimeline)
endif()

if(TIMELINE_CONTROL_WITH_QT)
    add_subdirectory(qtTimeline)
endif()
