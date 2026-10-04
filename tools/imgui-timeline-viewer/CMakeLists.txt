# Copyright (c) 2026 Richard Thomson

add_library(imgui-viewer-core STATIC Viewer.h Viewer.cpp)
target_include_directories(imgui-viewer-core PUBLIC .)
target_link_libraries(imgui-viewer-core PUBLIC timeline-imgui timeline-paranimator)
target_folder(imgui-viewer-core "Libraries")

find_package(SDL3 CONFIG REQUIRED)
add_executable(imgui-timeline-viewer WIN32 main.cpp)
target_link_libraries(imgui-timeline-viewer PRIVATE imgui-viewer-core SDL3::SDL3)
target_folder(imgui-timeline-viewer "Tools")

if(BUILD_TESTING)
    add_test(NAME ImGuiViewerSmoke COMMAND imgui-timeline-viewer --smoke-test
        "${PROJECT_SOURCE_DIR}/tests/test-timeline-paranimator/fixtures/extreme-normalized-vectors.json")
    set_tests_properties(ImGuiViewerSmoke PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software"
        TIMEOUT 30)
endif()
