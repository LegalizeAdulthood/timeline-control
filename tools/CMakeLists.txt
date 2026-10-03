# Copyright (c) 2026 Richard Thomson

if(TIMELINE_CONTROL_WITH_WX)
    add_subdirectory(timeline-viewer)
endif()
if(TIMELINE_CONTROL_WITH_IMGUI)
    add_subdirectory(imgui-timeline-viewer)
endif()
