# Copyright (c) 2026 Richard Thomson

find_package(GTest CONFIG REQUIRED)

add_executable(test-timeline-wx palette_test.cpp)
target_link_libraries(test-timeline-wx PRIVATE timeline-wx GTest::gtest_main)
target_folder(test-timeline-wx "Tests")

include(GoogleTest)
gtest_discover_tests(test-timeline-wx)

if(TIMELINE_CONTROL_WITH_CAIRO)
    add_executable(test-timeline-cairo cairo_test.cpp cairo_main.cpp)
    target_link_libraries(test-timeline-cairo PRIVATE timeline-wx timeline-paranimator GTest::gtest)
    target_compile_definitions(test-timeline-cairo PRIVATE
        TIMELINE_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../test-timeline-paranimator/fixtures")
    target_folder(test-timeline-cairo "Tests")
    gtest_discover_tests(test-timeline-cairo DISCOVERY_MODE PRE_TEST)
endif()
