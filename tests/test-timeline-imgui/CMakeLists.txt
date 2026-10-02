# Copyright (c) 2026 Richard Thomson

find_package(GTest CONFIG REQUIRED)

add_executable(test-timeline-imgui dependency_test.cpp)
target_link_libraries(test-timeline-imgui PRIVATE imgui::imgui GTest::gtest_main)
target_folder(test-timeline-imgui "Tests")

include(GoogleTest)
gtest_discover_tests(test-timeline-imgui)
