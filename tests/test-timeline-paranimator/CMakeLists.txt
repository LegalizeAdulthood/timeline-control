find_package(GTest CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

set(TIMELINE_PARANIMATOR_TEST_FIXTURES
    fixtures/empty-animation.json
    fixtures/input/core-catalog.json
    fixtures/maxiter-step.json
    fixtures/single-layer.json
    fixtures/inside-hold.json
    fixtures/path-generators.json
    fixtures/gold-path-generators.par
    fixtures/partial-paths.json
    fixtures/single-frame-path.json
    fixtures/ellipse-path.json
    fixtures/gold-ellipse-path.par
    fixtures/circle-path.json
    fixtures/partial-planar-paths.json
    fixtures/lissajous-path.json
    fixtures/gold-lissajous-path.par
    fixtures/lissajous-phase.json
    fixtures/partial-lissajous-paths.json
    fixtures/spiral-path.json
    fixtures/gold-spiral-path.par
    fixtures/spiral-variants.json
    fixtures/partial-spiral-paths.json
    fixtures/bezier-path.json
    fixtures/gold-bezier-path.par
    fixtures/bezier-tuples.json
    fixtures/partial-bezier-paths.json
    fixtures/single-frame-bezier.json
    fixtures/input/path-catalog.json
    fixtures/partial-animation.json
    fixtures/invalid-animation.json
    fixtures/missing-catalog.json
    fixtures/beat-keys/adapter.beat-keys.json
    fixtures/beat-keys/rms.beat-keys.json
    fixtures/beat-keys/fractional-frame.beat-keys.json
    fixtures/beat-keys/fractional-frame.music.json
    fixtures/beat-keys/offset-pulses.beat-keys.json
    fixtures/beat-keys/peak.beat-keys.json
    fixtures/beat-keys/row-pulses.beat-keys.json
    fixtures/beat-keys/note-pulses.beat-keys.json
    fixtures/beat-keys/invalid-binding.beat-keys.json
    fixtures/beat-keys/missing-input.beat-keys.json
    fixtures/beat-keys/timeline-features.json
    fixtures/beat-keys/gold-write-note-pulses.json
    fixtures/beat-keys/gold-write-peak-keyframes.json
    fixtures/beat-keys/gold-write-rms-keyframes.json
    fixtures/beat-keys/gold-write-row-pulses.json
    fixtures/beat-keys/timeline-diagnostics.json
    fixtures/beat-keys/timeline-events.json
    fixtures/invalid-schema.json
    fixtures/maxiter.json
    fixtures/multi-track.json
    fixtures/par-beatdown/gold-write-timeline-clock.json
    fixtures/par-beatdown/gold-write-windowed-features.json
    fixtures/par-beatdown/invalid-metadata.json
    fixtures/par-beatdown/invalid-shape.json
    fixtures/par-beatdown/mixed-events-and-features.json
    fixtures/par-beatdown/partial.music.json
    fixtures/par-beatdown/song.music.json
    fixtures/snapshots/empty.txt
    fixtures/snapshots/events.txt
    fixtures/snapshots/curve.txt
    fixtures/snapshots/keyframes.txt
)

add_executable(test-timeline-paranimator
    ${TIMELINE_PARANIMATOR_TEST_FIXTURES}
    test.cpp
    mapping_test.cpp
    animation_test.cpp
    snapshot_test.cpp
)
target_link_libraries(test-timeline-paranimator
    PRIVATE
        timeline-paranimator
        nlohmann_json::nlohmann_json
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
