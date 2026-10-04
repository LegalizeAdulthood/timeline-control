# Copyright (c) 2026 Richard Thomson

find_package(GTest CONFIG REQUIRED)

add_executable(test-timeline-imgui dependency_test.cpp palette_test.cpp control_test.cpp viewer_test.cpp)
target_link_libraries(test-timeline-imgui PRIVATE imgui-viewer-core GTest::gtest_main)
target_compile_definitions(test-timeline-imgui PRIVATE
    TIMELINE_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../test-timeline-paranimator/fixtures")
target_folder(test-timeline-imgui "Tests")

include(GoogleTest)
gtest_discover_tests(test-timeline-imgui)
