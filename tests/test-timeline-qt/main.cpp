// Copyright (c) 2026 Richard Thomson

#include <gtest/gtest.h>

#include <QApplication>

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", TIMELINE_QT_TEST_PLATFORM);
    QApplication app(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
