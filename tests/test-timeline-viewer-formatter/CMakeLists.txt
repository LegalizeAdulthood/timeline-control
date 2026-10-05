# Copyright (c) 2026 Richard Thomson

find_package(GTest CONFIG REQUIRED)

add_executable(test-timeline-viewer-formatter format_inspector_test.cpp)
target_link_libraries(test-timeline-viewer-formatter PRIVATE timeline-viewer-formatter GTest::gtest_main)
target_compile_definitions(test-timeline-viewer-formatter PRIVATE
    TIMELINE_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../test-timeline-paranimator/fixtures")
target_folder(test-timeline-viewer-formatter "Tests")

include(GoogleTest)
gtest_discover_tests(test-timeline-viewer-formatter)
