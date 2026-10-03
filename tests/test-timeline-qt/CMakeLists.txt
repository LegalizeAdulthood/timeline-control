# Copyright (c) 2026 Richard Thomson

find_package(GTest CONFIG REQUIRED)
add_executable(test-timeline-qt dependency_test.cpp)
target_link_libraries(test-timeline-qt PRIVATE Qt6::Widgets GTest::gtest_main)
target_folder(test-timeline-qt "Tests")
deploy_qt_plugins(test-timeline-qt QMinimalIntegrationPlugin)
include(GoogleTest)
gtest_discover_tests(test-timeline-qt
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=minimal")

find_package(Qt6 CONFIG REQUIRED COMPONENTS Test)
add_executable(test-timeline-qt-widget main.cpp control_test.cpp renderer_test.cpp viewer_test.cpp)
target_link_libraries(test-timeline-qt-widget PRIVATE qt-viewer-content Qt6::Test GTest::gtest)
target_compile_definitions(test-timeline-qt-widget PRIVATE
    TIMELINE_TEST_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/tests/test-timeline-paranimator/fixtures")
target_folder(test-timeline-qt-widget "Tests")
deploy_qt_plugins(test-timeline-qt-widget QMinimalIntegrationPlugin)
gtest_discover_tests(test-timeline-qt-widget
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=minimal")
