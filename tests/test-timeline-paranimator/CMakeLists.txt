find_package(GTest CONFIG REQUIRED)

set(TIMELINE_PARANIMATOR_TEST_FIXTURES
    fixtures/beat-keys/adapter.beat-keys.json
    fixtures/beat-keys/gold-write-rms-keyframes.json
    fixtures/beat-keys/gold-write-row-pulses.json
    fixtures/beat-keys/timeline-diagnostics.json
    fixtures/beat-keys/timeline-events.json
    fixtures/invalid-schema.json
    fixtures/maxiter.json
    fixtures/multi-track.json
    fixtures/par-beatdown/gold-write-timeline-clock.json
    fixtures/par-beatdown/gold-write-windowed-features.json
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

foreach(fixture IN LISTS TIMELINE_PARANIMATOR_TEST_FIXTURES)
    get_filename_component(fixture_directory "${fixture}" DIRECTORY)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${fixture_directory}")
    configure_file("${fixture}" "${fixture}" COPYONLY)
endforeach()

include(GoogleTest)
gtest_discover_tests(test-timeline-paranimator)
