add_subdirectory(timeline)
add_subdirectory(timelineParAnimator)

if(TIMELINE_CONTROL_WITH_WX)
    add_subdirectory(wxTimeline)
endif()
