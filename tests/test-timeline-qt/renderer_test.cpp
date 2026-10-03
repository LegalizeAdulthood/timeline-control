// Copyright (c) 2026 Richard Thomson

#include <gtest/gtest.h>
#include <QImage>
#include <qtTimeline/Renderer.h>

using timeline_qt::draw_display_list;
using timeline_qt::style_color;

TEST(QtRenderer, resolves_theme_and_focus_roles)
{
    QPalette palette;
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::Text, Qt::black);
    palette.setColor(QPalette::Highlight, Qt::blue);
    EXPECT_EQ(QColor(Qt::blue), style_color(timeline::StyleRole::SELECTED_ITEM, palette, true));
    EXPECT_NE(style_color(timeline::StyleRole::SELECTED_ITEM, palette, true),
        style_color(timeline::StyleRole::SELECTED_ITEM, palette, false));
    const QColor light = style_color(timeline::StyleRole::KEYFRAME_SEGMENT, palette, true);
    palette.setColor(QPalette::Base, Qt::black);
    palette.setColor(QPalette::Text, Qt::white);
    EXPECT_EQ(QColor(Qt::white), style_color(timeline::StyleRole::LANE_LABEL, palette, true));
    EXPECT_NE(light, style_color(timeline::StyleRole::KEYFRAME_SEGMENT, palette, true));
}

TEST(QtRenderer, draws_all_primitive_types_and_restores_painter_state)
{
    QImage image(160, 80, QImage::Format_ARGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    const QPen original(Qt::magenta, 3);
    painter.setPen(original);
    painter.setClipRect(0, 0, 140, 80);
    const QRegion clip = painter.clipRegion();
    timeline::DisplayList list;
    list.add(timeline::Line{10, 10, 30, 10, timeline::StyleRole::RULER, {}});
    list.add(timeline::Rectangle{40, 5, 10, 10, timeline::StyleRole::INTERVAL_SPAN, {}});
    list.add(timeline::Marker{60, 5, 5, 10, timeline::StyleRole::INSTANT_MARKER, {}});
    list.add(timeline::Polyline{{{80, 5}, {90, 15}, {100, 5}}, timeline::StyleRole::CURVE, {}});
    list.add(timeline::Text{110, 5, "X", timeline::StyleRole::RULER_LABEL, {}});
    draw_display_list(painter, list, QPalette{}, true, 100);
    EXPECT_EQ(original, painter.pen());
    EXPECT_EQ(clip, painter.clipRegion());
    painter.end();
    EXPECT_NE(QColor(Qt::white), image.pixelColor(20, 10));
    EXPECT_NE(QColor(Qt::white), image.pixelColor(45, 10));
    EXPECT_NE(QColor(Qt::white), image.pixelColor(62, 10));
    EXPECT_NE(QColor(Qt::white), image.pixelColor(90, 15));
}

TEST(QtRenderer, preserves_source_rgb_and_painter_clipping)
{
    QImage image(80, 60, QImage::Format_ARGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setClipRect(0, 0, 40, 60);
    timeline::DisplayList list;
    list.add(timeline::Swatch{10, 10, 50, 20, timeline::RgbColor(25, 140, 230), timeline::StyleRole::PALETTE, {}});
    list.add(timeline::Text{0, 35, "A very long lane label", timeline::StyleRole::LANE_LABEL, {}});
    draw_display_list(painter, list, QPalette{}, true, 20);
    painter.end();
    EXPECT_EQ(QColor(25, 140, 230), image.pixelColor(20, 15));
    EXPECT_EQ(QColor(Qt::white), image.pixelColor(50, 15));
    for (int x = 21; x < image.width(); ++x)
    {
        EXPECT_EQ(QColor(Qt::white), image.pixelColor(x, 45));
    }
}
