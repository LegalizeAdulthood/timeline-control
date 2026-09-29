// Copyright (c) 2026 Richard Thomson

#include <timeline/Layout.h>
#include <timeline/size_cast.h>

#include <gtest/gtest.h>

#include <type_traits>
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

void expect_same_geometry(const Primitive &lhs, const Primitive &rhs)
{
    ASSERT_EQ(lhs.index(), rhs.index());
    std::visit(
        [&rhs](const auto &left)
        {
            using Value = std::decay_t<decltype(left)>;
            const auto &right = std::get<Value>(rhs);
            if constexpr (std::is_same_v<Value, Line>)
            {
                EXPECT_EQ(left.x1, right.x1);
                EXPECT_EQ(left.y1, right.y1);
                EXPECT_EQ(left.x2, right.x2);
                EXPECT_EQ(left.y2, right.y2);
            }
            else if constexpr (std::is_same_v<Value, Rectangle> || std::is_same_v<Value, Marker>)
            {
                EXPECT_EQ(left.x, right.x);
                EXPECT_EQ(left.y, right.y);
                EXPECT_EQ(left.width, right.width);
                EXPECT_EQ(left.height, right.height);
            }
            else if constexpr (std::is_same_v<Value, Text>)
            {
                EXPECT_EQ(left.x, right.x);
                EXPECT_EQ(left.y, right.y);
            }
            else
            {
                ASSERT_EQ(left.points.size(), right.points.size());
                for (auto index = 0; index < size_cast(left.points); ++index)
                {
                    EXPECT_EQ(left.points[index].x, right.points[index].x);
                    EXPECT_EQ(left.points[index].y, right.points[index].y);
                }
            }
            EXPECT_EQ(left.style, right.style);
            EXPECT_EQ(left.id.lane_id, right.id.lane_id);
            EXPECT_EQ(left.id.item_id, right.id.item_id);
        },
        lhs);
}

} // namespace

TEST(Layout, emits_ruler_and_empty_lane_scaffolding)
{
    auto document = Document(100);
    document.add_lane(Lane("empty", "Empty lane", "events", at(0), at(100)));

    const auto layout = Layout(document, Viewport(400, 100, at(0), at(100)), LayoutMetrics(100, 20, 30, 4));
    const auto &primitives = layout.display_list().primitives();

    ASSERT_EQ(4U, primitives.size());
    EXPECT_EQ("ruler", std::get<Line>(primitives[0]).id.item_id);
    EXPECT_EQ("ruler", std::get<Text>(primitives[1]).id.item_id);
    EXPECT_EQ("empty", std::get<Rectangle>(primitives[2]).id.lane_id);
    EXPECT_TRUE(std::get<Rectangle>(primitives[2]).id.item_id.empty());
    EXPECT_EQ("empty", std::get<Text>(primitives[3]).id.lane_id);
    EXPECT_TRUE(std::get<Text>(primitives[3]).id.item_id.empty());
}

TEST(Layout, emits_event_and_interval_primitives)
{
    auto lane = Lane("music", "Music events", "events", at(0), at(100));
    lane.add(Instant("beat-1", "beat", at(25)));
    lane.add(Interval("phrase-1", "phrase", at(40), at(60)));
    lane.add(Envelope("pulse-1", "pulse", at(70), lasting(10), lasting(10), lasting(10), {}, std::nullopt, {}));

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

    const auto &instant = std::get<Marker>(primitives[4]);
    EXPECT_EQ(174, instant.x);
    EXPECT_EQ(2, instant.width);
    EXPECT_EQ(StyleRole::INSTANT_MARKER, instant.style);
    EXPECT_EQ("music", instant.id.lane_id);
    EXPECT_EQ("beat-1", instant.id.item_id);

    const auto &interval = std::get<Rectangle>(primitives[5]);
    EXPECT_EQ(220, interval.x);
    EXPECT_EQ(60, interval.width);
    EXPECT_EQ(StyleRole::INTERVAL_SPAN, interval.style);
    EXPECT_EQ("music", interval.id.lane_id);
    EXPECT_EQ("phrase-1", interval.id.item_id);

    EXPECT_EQ(StyleRole::ENVELOPE_ATTACK, std::get<Rectangle>(primitives[6]).style);
    EXPECT_EQ(StyleRole::ENVELOPE_SUSTAIN, std::get<Rectangle>(primitives[7]).style);
    EXPECT_EQ(StyleRole::ENVELOPE_DECAY, std::get<Rectangle>(primitives[8]).style);
    EXPECT_EQ("pulse-1", std::get<Rectangle>(primitives[6]).id.item_id);
    EXPECT_EQ("pulse-1", std::get<Rectangle>(primitives[7]).id.item_id);
    EXPECT_EQ("pulse-1", std::get<Rectangle>(primitives[8]).id.item_id);
}

TEST(Layout, emits_curve_polyline_sampled_at_frame_boundaries)
{
    auto lane = Lane("rms", "RMS", "curve", at(0), at(40));
    lane.add(Curve(
        "rms", "rms", {{at(0), 0.0}, {at(20), 1.0}, {at(40), 0.0}}, "RMS", CurveInterpolation::LINEAR, 0.0, 1.0, {}));

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
    EXPECT_EQ("rms", polyline.id.lane_id);
    EXPECT_EQ("rms", polyline.id.item_id);
}

TEST(Layout, emits_keyframe_markers_and_interpolation_segments)
{
    auto lane = Lane("zoom", "camera.zoom", "keyframes", at(0), at(50));
    lane.add(Keyframe("zoom-0", at(0), 0.0, KeyframeInterpolation::LINEAR, {}));
    lane.add(Keyframe("zoom-20", at(20), 1.0, KeyframeInterpolation::HOLD, {}));
    lane.add(Keyframe("zoom-40", at(40), 0.0));

    auto document = Document(FrameGrid(Timebase(10), 5, 1, 1), 1, 3);
    document.add_lane(std::move(lane));

    const auto layout = Layout(document, Viewport(500, 100, at(0), at(50)), LayoutMetrics(100, 20, 30, 4));
    const auto &primitives = layout.display_list().primitives();

    ASSERT_EQ(9U, primitives.size());
    const auto &linear = std::get<Polyline>(primitives[4]);
    ASSERT_EQ(2U, linear.points.size());
    EXPECT_EQ(100, linear.points[0].x);
    EXPECT_EQ(45, linear.points[0].y);
    EXPECT_EQ(260, linear.points[1].x);
    EXPECT_EQ(24, linear.points[1].y);
    EXPECT_EQ(StyleRole::KEYFRAME_SEGMENT, linear.style);

    const auto &hold = std::get<Polyline>(primitives[5]);
    ASSERT_EQ(3U, hold.points.size());
    EXPECT_EQ(260, hold.points[0].x);
    EXPECT_EQ(24, hold.points[0].y);
    EXPECT_EQ(420, hold.points[1].x);
    EXPECT_EQ(24, hold.points[1].y);
    EXPECT_EQ(420, hold.points[2].x);
    EXPECT_EQ(45, hold.points[2].y);
    EXPECT_EQ(StyleRole::KEYFRAME_SEGMENT, hold.style);
    EXPECT_EQ("zoom", linear.id.lane_id);
    EXPECT_EQ("zoom-0", linear.id.item_id);
    EXPECT_EQ("zoom", hold.id.lane_id);
    EXPECT_EQ("zoom-20", hold.id.item_id);

    EXPECT_EQ(StyleRole::KEYFRAME_MARKER, std::get<Marker>(primitives[6]).style);
    EXPECT_EQ("zoom-0", std::get<Marker>(primitives[6]).id.item_id);
    EXPECT_EQ(StyleRole::KEYFRAME_MARKER, std::get<Marker>(primitives[7]).style);
    EXPECT_EQ("zoom-20", std::get<Marker>(primitives[7]).id.item_id);
    EXPECT_EQ(StyleRole::KEYFRAME_MARKER, std::get<Marker>(primitives[8]).style);
    EXPECT_EQ("zoom-40", std::get<Marker>(primitives[8]).id.item_id);
}

TEST(Layout, maps_horizontal_positions_to_timeline_time)
{
    const auto viewport = Viewport(300, 100, at(0), at(100));
    const auto metrics = LayoutMetrics(100, 20, 30, 4);

    EXPECT_EQ(0, time_at_x(0, viewport, metrics).ticks());
    EXPECT_EQ(0, time_at_x(100, viewport, metrics).ticks());
    EXPECT_EQ(50, time_at_x(200, viewport, metrics).ticks());
    EXPECT_EQ(100, time_at_x(300, viewport, metrics).ticks());
    EXPECT_EQ(100, time_at_x(400, viewport, metrics).ticks());
}

TEST(Layout, computes_visible_frame_range)
{
    const auto grid = FrameGrid(Timebase(100), 10, 10, 1);
    const auto range = visible_frame_range(grid, Viewport(300, 100, at(25), at(65)));

    ASSERT_TRUE(range.has_value());
    EXPECT_EQ(2, range->first());
    EXPECT_EQ(6, range->last());
}

TEST(Layout, maps_frames_to_pixels_and_back)
{
    const auto grid = FrameGrid(Timebase(100), 10, 10, 1);
    const auto viewport = Viewport(300, 100, at(20), at(60));
    const auto metrics = LayoutMetrics(100, 20, 20, 4);

    EXPECT_EQ(100, frame_x(2, grid, viewport, metrics));
    EXPECT_EQ(200, frame_x(4, grid, viewport, metrics));
    EXPECT_EQ(300, frame_x(6, grid, viewport, metrics));
    EXPECT_EQ(3, *frame_at_x(150, grid, viewport, metrics));
    EXPECT_EQ(5, *frame_at_x(250, grid, viewport, metrics));
}

TEST(Layout, keeps_lane_heights_stable_while_scrolling)
{
    auto document = Document(100);
    document.add_lane(Lane("a", "Lane A", "events", at(0), at(100)));
    document.add_lane(Lane("b", "Lane B", "events", at(0), at(100)));
    document.add_lane(Lane("c", "Lane C", "events", at(0), at(100)));
    document.add_lane(Lane("d", "Lane D", "events", at(0), at(100)));
    const auto viewport = Viewport(300, 75, at(0), at(100), 1);
    const auto metrics = LayoutMetrics(100, 20, 20, 4);
    const auto layout = Layout(document, viewport, metrics);
    const auto &primitives = layout.display_list().primitives();

    EXPECT_EQ(2, visible_lane_count(viewport, metrics));
    EXPECT_EQ(20, *lane_y(1, viewport, metrics));
    EXPECT_EQ(40, *lane_y(2, viewport, metrics));
    EXPECT_FALSE(lane_y(3, viewport, metrics).has_value());
    EXPECT_EQ(1, *lane_at_y(20, document.lane_count(), viewport, metrics));
    EXPECT_EQ(2, *lane_at_y(59, document.lane_count(), viewport, metrics));
    EXPECT_FALSE(lane_at_y(60, document.lane_count(), viewport, metrics).has_value());

    ASSERT_EQ(6U, primitives.size());
    EXPECT_EQ(20, std::get<Rectangle>(primitives[2]).height);
    EXPECT_EQ("Lane B", std::get<Text>(primitives[3]).value);
    EXPECT_EQ(20, std::get<Rectangle>(primitives[4]).height);
    EXPECT_EQ("Lane C", std::get<Text>(primitives[5]).value);
}

TEST(Layout, navigates_exact_ranges_without_changing_document_data)
{
    auto document = Document(100);
    document.add_lane(Lane("lane", "Lane", "events", at(0), at(100)));
    auto navigation = Navigation(at(0), at(100), 5);

    navigation.zoom_by(2.0, at(50));
    auto viewport = navigation.viewport(300, 100);
    EXPECT_EQ(25, viewport.start().ticks());
    EXPECT_EQ(75, viewport.end().ticks());
    EXPECT_DOUBLE_EQ(2.0, navigation.zoom_scale());

    navigation.scroll_to(at(90));
    viewport = navigation.viewport(300, 100);
    EXPECT_EQ(50, viewport.start().ticks());
    EXPECT_EQ(100, viewport.end().ticks());
    EXPECT_EQ(50, navigation.horizontal_offset().ticks());

    navigation.scroll_to_fraction(0.0);
    navigation.scroll_to_lane(99, 2);
    viewport = navigation.viewport(300, 100);
    EXPECT_EQ(0, viewport.start().ticks());
    EXPECT_EQ(3, viewport.first_lane());
    EXPECT_EQ(1, document.lane_count());
    EXPECT_EQ("lane", document.lanes()[0].id());
}

TEST(Layout, produces_identical_geometry_for_identical_metrics)
{
    auto lane = Lane("music", "Music", "events", at(0), at(100));
    lane.add(Instant("beat", "beat", at(50)));
    auto document = Document(100);
    document.add_lane(std::move(lane));
    const auto viewport = Viewport(320, 80, at(0), at(100));
    const auto metrics = LayoutMetrics(100, 20, 30, 4);
    const auto first = Layout(document, viewport, metrics);
    const auto second = Layout(document, viewport, metrics);
    const auto &first_primitives = first.display_list().primitives();
    const auto &second_primitives = second.display_list().primitives();

    ASSERT_EQ(first_primitives.size(), second_primitives.size());
    for (auto index = 0; index < size_cast(first_primitives); ++index)
    {
        expect_same_geometry(first_primitives[index], second_primitives[index]);
    }
}
