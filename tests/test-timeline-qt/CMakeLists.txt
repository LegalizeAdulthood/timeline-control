# Copyright (c) 2026 Richard Thomson

find_package(GTest CONFIG REQUIRED)
add_executable(test-timeline-qt dependency_test.cpp)
target_link_libraries(test-timeline-qt PRIVATE Qt6::Widgets GTest::gtest_main)
target_folder(test-timeline-qt "Tests")
deploy_qt_plugins(test-timeline-qt QMinimalIntegrationPlugin)
include(GoogleTest)
gtest_discover_tests(test-timeline-qt
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=minimal")
