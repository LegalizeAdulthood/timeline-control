// Copyright (c) 2026 Richard Thomson

#include <gtest/gtest.h>
#include <QApplication>
#include <QImage>
#include <QPalette>
#include <QWidget>

TEST(QtDependency, initializes_widgets_and_paints_without_a_desktop)
{
    ASSERT_EQ(nullptr, QApplication::instance());
    int argc = 1;
    char name[] = "test-timeline-qt";
    char *argv[]{name, nullptr};
    {
        QApplication app(argc, argv);
        EXPECT_EQ(&app, QApplication::instance());
        QWidget widget;
        widget.resize(32, 24);
        QPalette palette = widget.palette();
        const QColor color(25, 140, 230);
        palette.setColor(QPalette::Window, color);
        widget.setPalette(palette);
        widget.setAutoFillBackground(true);
        QImage image(widget.size(), QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        widget.render(&image);
        EXPECT_EQ(color, image.pixelColor(16, 12));
    }
    EXPECT_EQ(nullptr, QApplication::instance());
}
