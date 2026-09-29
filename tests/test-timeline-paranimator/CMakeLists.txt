find_package(GTest CONFIG REQUIRED)

set(TIMELINE_PARANIMATOR_TEST_FIXTURES
    fixtures/invalid-schema.json
    fixtures/maxiter.json
    fixtures/multi-track.json
)

add_executable(test-timeline-paranimator
    ${TIMELINE_PARANIMATOR_TEST_FIXTURES}
    test.cpp
)
target_link_libraries(test-timeline-paranimator
    PRIVATE
        timeline-paranimator
        GTest::gtest_main
)
target_folder(test-timeline-paranimator "Tests")
source_group("Fixtures" FILES ${TIMELINE_PARANIMATOR_TEST_FIXTURES})

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixtures")
foreach(fixture IN LISTS TIMELINE_PARANIMATOR_TEST_FIXTURES)
    configure_file("${fixture}" "${fixture}" COPYONLY)
endforeach()

include(GoogleTest)
gtest_discover_tests(test-timeline-paranimator)
