// Copyright (c) 2026 Richard Thomson

#include <timeline/Layout.h>

#include <gtest/gtest.h>

#include <variant>

using namespace timeline;

namespace
{

Time at(Ticks ticks)
{
    return Time::from_ticks(ticks);
}

Duration lasting(Ticks ticks)
{
    return Duration::from_ticks(ticks);
}

} // namespace

TEST(Layout, emits_event_and_interval_primitives)
{
    auto lane = Lane("music", "Music events", "events", at(0), at(100));
    lane.add(Instant("beat-1", "beat", at(25)));
    lane.add(Interval("phrase-1", "phrase", at(40), at(60)));
    lane.add(Envelope("pulse-1", "pulse", at(70), lasting(10), lasting(10), lasting(10)));

    auto document = Document(1000);
    document.add_lane(std::move(lane));

    const auto layout = Layout(document, Viewport(400, 100, at(0), at(100)), LayoutMetrics(100, 20, 30, 4));
    const auto &primitives = layout.display_list().primitives();

    ASSERT_EQ(9U, primitives.size());
    EXPECT_EQ(StyleRole::RULER, std::get<Line>(primitives[0]).style);
    EXPECT_EQ(StyleRole::RULER_LABEL, std::get<Text>(primitives[1]).style);

    const auto &lane_background = std::get<Rectangle>(primitives[2]);
    EXPECT_EQ(100, lane_background.x);
    EXPECT_EQ(20, lane_background.y);
    EXPECT_EQ(300, lane_background.width);
    EXPECT_EQ(30, lane_background.height);
    EXPECT_EQ(StyleRole::LANE_BACKGROUND, lane_background.style);
    EXPECT_EQ("Music events", std::get<Text>(primitives[3]).value);

    const auto &instant = std::get<Line>(primitives[4]);
    EXPECT_EQ(175, instant.x1);
    EXPECT_EQ(StyleRole::INSTANT_MARKER, instant.style);

    const auto &interval = std::get<Rectangle>(primitives[5]);
    EXPECT_EQ(220, interval.x);
    EXPECT_EQ(60, interval.width);
    EXPECT_EQ(StyleRole::INTERVAL_SPAN, interval.style);

    EXPECT_EQ(StyleRole::ENVELOPE_ATTACK, std::get<Rectangle>(primitives[6]).style);
    EXPECT_EQ(StyleRole::ENVELOPE_SUSTAIN, std::get<Rectangle>(primitives[7]).style);
    EXPECT_EQ(StyleRole::ENVELOPE_DECAY, std::get<Rectangle>(primitives[8]).style);
}

TEST(Layout, emits_curve_polyline_sampled_at_frame_boundaries)
{
    auto lane = Lane("rms", "RMS", "curve", at(0), at(40));
    lane.add(
        Curve("rms", "rms", {{at(0), 0.0}, {at(20), 1.0}, {at(40), 0.0}}, "RMS", CurveInterpolation::LINEAR, 0.0, 1.0));

    auto document = Document(FrameGrid(Timebase(10), 4, 1, 1), 0, 0);
    document.add_lane(std::move(lane));

    const auto layout = Layout(document, Viewport(500, 100, at(0), at(40)), LayoutMetrics(100, 20, 30, 4));
    const auto &primitives = layout.display_list().primitives();

    ASSERT_EQ(5U, primitives.size());
    const auto &polyline = std::get<Polyline>(primitives[4]);
    ASSERT_EQ(4U, polyline.points.size());
    EXPECT_EQ(100, polyline.points[0].x);
    EXPECT_EQ(45, polyline.points[0].y);
    EXPECT_EQ(200, polyline.points[1].x);
    EXPECT_EQ(34, polyline.points[1].y);
    EXPECT_EQ(300, polyline.points[2].x);
    EXPECT_EQ(24, polyline.points[2].y);
    EXPECT_EQ(400, polyline.points[3].x);
    EXPECT_EQ(34, polyline.points[3].y);
    EXPECT_EQ(StyleRole::CURVE, polyline.style);
}
