find_package(GTest CONFIG REQUIRED)

add_executable(test-timeline-paranimator test.cpp)
target_link_libraries(test-timeline-paranimator
    PRIVATE
        timeline-paranimator
        GTest::gtest_main
)
target_folder(test-timeline-paranimator "Tests")

include(GoogleTest)
gtest_discover_tests(test-timeline-paranimator)
