# Copyright (c) 2026 Richard Thomson

find_package(GTest CONFIG REQUIRED)

add_executable(test-timeline-wx palette_test.cpp)
target_link_libraries(test-timeline-wx PRIVATE timeline-wx GTest::gtest_main)
target_folder(test-timeline-wx "Tests")

include(GoogleTest)
gtest_discover_tests(test-timeline-wx)
