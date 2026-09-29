include(GoogleTest)

find_package(GTest CONFIG REQUIRED)

add_executable(test-timeline-core
    curve_test.cpp
    keyframe_test.cpp
    test.cpp
    lane_test.cpp
    layout_test.cpp
)
target_link_libraries(test-timeline-core PUBLIC timeline-core GTest::gtest_main)
target_folder(test-timeline-core "Tests")

gtest_discover_tests(test-timeline-core)
