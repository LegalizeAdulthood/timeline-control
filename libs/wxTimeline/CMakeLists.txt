find_package(wxWidgets CONFIG REQUIRED)

add_library(timeline-wx
    include/wxTimeline/wxTimelineControl.h
    include/wxTimeline/wxTimelineRenderer.h
    wxTimelineControl.cpp
    wxTimelineRenderer.cpp
)
target_include_directories(timeline-wx PUBLIC include)
target_link_libraries(timeline-wx
    PUBLIC
        timeline-core
        wx::base
        wx::core
)
target_folder(timeline-wx "Libraries")

if(TIMELINE_CONTROL_WITH_CAIRO)
    find_program(PKG_CONFIG_EXECUTABLE NAMES pkgconf pkg-config
        HINTS "${VCPKG_INSTALLED_DIR}/${VCPKG_HOST_TRIPLET}/tools/pkgconf")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)
    target_sources(timeline-wx PRIVATE
        include/wxTimeline/wxCairoTimeline.h
        include/wxTimeline/wxCairoTimelineRenderer.h
        wxCairoTimeline.cpp
        wxCairoTimelineRenderer.cpp
    )
    target_link_libraries(timeline-wx PRIVATE PkgConfig::CAIRO)
endif()
