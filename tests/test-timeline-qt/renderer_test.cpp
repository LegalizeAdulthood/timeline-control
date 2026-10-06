// Copyright (c) 2026 Richard Thomson

#include <qtTimeline/Renderer.h>

#include <gtest/gtest.h>

#include <QImage>

using timeline_qt::draw_display_list;
using timeline_qt::style_color;

TEST(QtRenderer, mapsTimelineRolesToPaletteRoles)
{
    QPalette palette;
    palette.setColor(QPalette::Text, QColor(10, 20, 30));
    palette.setColor(QPalette::Mid, QColor(20, 30, 40));
    palette.setColor(QPalette::AlternateBase, QColor(30, 40, 50));
    palette.setColor(QPalette::Accent, QColor(40, 50, 60));
    palette.setColor(QPalette::Link, QColor(50, 60, 70));
    palette.setColor(QPalette::LinkVisited, QColor(60, 70, 80));

    const QColor label = style_color(timeline::StyleRole::LANE_LABEL, palette, true);
    const QColor ruler = style_color(timeline::StyleRole::RULER, palette, true);
    const QColor lane = style_color(timeline::StyleRole::LANE_BACKGROUND, palette, true);
    const QColor marker = style_color(timeline::StyleRole::INSTANT_MARKER, palette, true);
    const QColor interval = style_color(timeline::StyleRole::INTERVAL_SPAN, palette, true);
    const QColor decay = style_color(timeline::StyleRole::ENVELOPE_DECAY, palette, true);

    EXPECT_EQ(QColor(10, 20, 30), label);
    EXPECT_EQ(QColor(20, 30, 40), ruler);
    EXPECT_EQ(QColor(30, 40, 50), lane);
    EXPECT_EQ(QColor(40, 50, 60), marker);
    EXPECT_EQ(QColor(50, 60, 70), interval);
    EXPECT_EQ(QColor(60, 70, 80), decay);
}

TEST(QtRenderer, mapsSelectionAccordingToFocus)
{
    QPalette palette;
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::Text, Qt::black);
    palette.setColor(QPalette::Highlight, Qt::blue);

    const QColor focused = style_color(timeline::StyleRole::SELECTED_ITEM, palette, true);
    const QColor inactive = style_color(timeline::StyleRole::SELECTED_ITEM, palette, false);

    EXPECT_EQ(QColor(Qt::blue), focused);
    EXPECT_NE(focused, inactive);
}

TEST(QtRenderer, usesApplicationStyleColor)
{
    timeline_qt::StyleColors colors;
    colors.emplace(timeline::StyleRole::CURVE, QColor(12, 34, 56));

    const QColor actual = style_color(timeline::StyleRole::CURVE, QPalette{}, colors, false);

    EXPECT_EQ(QColor(12, 34, 56), actual);
}

TEST(QtRenderer, drawsAllPrimitiveTypesAndRestoresPainterState)
{
    QImage image(160, 80, QImage::Format_ARGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    const QPen original(Qt::magenta, 3);
    painter.setPen(original);
    painter.setClipRect(0, 0, 140, 80);
    const QRegion clip = painter.clipRegion();
    timeline::StringTableBuilder strings;
    const timeline::StringId text = strings.intern("X");
    timeline::DisplayList list(std::move(strings).build());
    list.add(timeline::Line{10, 10, 30, 10, timeline::StyleRole::RULER, {}});
    list.add(timeline::Rectangle{40, 5, 10, 10, timeline::StyleRole::INTERVAL_SPAN, {}});
    list.add(timeline::Marker{60, 5, 5, 10, timeline::StyleRole::INSTANT_MARKER, {}});
    list.add(timeline::Polyline{{{80, 5}, {90, 15}, {100, 5}}, timeline::StyleRole::CURVE, {}});
    list.add(timeline::Text{110, 5, text, timeline::StyleRole::RULER_LABEL, {}});

    draw_display_list(painter, list, QPalette{}, true, 100);

    EXPECT_EQ(original, painter.pen());
    EXPECT_EQ(clip, painter.clipRegion());
    painter.end();
    EXPECT_NE(QColor(Qt::white), image.pixelColor(20, 10));
    EXPECT_NE(QColor(Qt::white), image.pixelColor(45, 10));
    EXPECT_NE(QColor(Qt::white), image.pixelColor(62, 10));
    EXPECT_NE(QColor(Qt::white), image.pixelColor(90, 15));
}

TEST(QtRenderer, preservesSourceRgbAndPainterClipping)
{
    QImage image(80, 60, QImage::Format_ARGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setClipRect(0, 0, 40, 60);
    timeline::StringTableBuilder strings;
    const timeline::StringId text = strings.intern("A very long lane label");
    timeline::DisplayList list(std::move(strings).build());
    list.add(timeline::Swatch{10, 10, 50, 20, timeline::RgbColor(25, 140, 230), timeline::StyleRole::PALETTE, {}});
    list.add(timeline::Text{0, 35, text, timeline::StyleRole::LANE_LABEL, {}});

    draw_display_list(painter, list, QPalette{}, true, 20);
    painter.end();

    EXPECT_EQ(QColor(25, 140, 230), image.pixelColor(20, 15));
    EXPECT_EQ(QColor(Qt::white), image.pixelColor(50, 15));
    for (int x = 21; x < image.width(); ++x)
    {
        EXPECT_EQ(QColor(Qt::white), image.pixelColor(x, 45));
    }
}
