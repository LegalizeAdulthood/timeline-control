# Copyright (c) 2026 Richard Thomson

find_package(GTest CONFIG REQUIRED)

add_executable(test-timeline-viewer-io load_timeline_test.cpp write_snapshot_test.cpp)
target_link_libraries(test-timeline-viewer-io PRIVATE timeline-viewer-io GTest::gtest_main)
target_compile_definitions(test-timeline-viewer-io PRIVATE
    TIMELINE_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../test-timeline-paranimator/fixtures")
target_folder(test-timeline-viewer-io "Tests")

include(GoogleTest)
gtest_discover_tests(test-timeline-viewer-io)
