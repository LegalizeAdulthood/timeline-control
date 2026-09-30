// Copyright (c) 2026 Richard Thomson

#include <timeline/Layout.h>
#include <timeline/size_cast.h>

#include <gtest/gtest.h>

#include <stdexcept>
#include <type_traits>
#include <variant>

using namespace timeline;

TEST(Layout, samples_geometric_keyframe_segments_at_grid_boundaries)
{
    const FrameGrid grid(Timebase(100), 3, 10, 1);
    Document document(grid, 1, 2);
    Lane lane("zoom", "Zoom", "keyframes", grid.offset(), grid.end_time());
    lane.add(Keyframe("first", grid.frame_start(0), 1.0, KeyframeInterpolation::GEOMETRIC, {}));
    lane.add(Keyframe("last", grid.frame_start(2), 9.0));
    document.add_lane(std::move(lane));
    const Layout layout(document, Viewport(400, 100, grid.offset(), grid.end_time()), LayoutMetrics(100, 20, 30, 4));
    bool found = false;
    for (const Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<Polyline>(primitive))
        {
            const Polyline &line = std::get<Polyline>(primitive);
            if (line.style == StyleRole::KEYFRAME_SEGMENT)
            {
                found = true;
                ASSERT_EQ(3, size_cast(line.points));
                EXPECT_EQ(40, line.points[1].y);
            }
        }
    }
    EXPECT_TRUE(found);
}

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
                for (int index = 0; index < size_cast(left.points); ++index)
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
    Document document(100);
    document.add_lane(Lane("empty", "Empty lane", "events", at(0), at(100)));

    const Layout layout(document, Viewport(400, 100, at(0), at(100)), LayoutMetrics(100, 20, 30, 4));
    const std::vector<Primitive> &primitives = layout.display_list().primitives();

    ASSERT_EQ(4U, primitives.size());
    EXPECT_EQ("ruler", std::get<Line>(primitives[0]).id.item_id);
    EXPECT_EQ("ruler", std::get<Text>(primitives[1]).id.item_id);
    EXPECT_EQ("empty", std::get<Rectangle>(primitives[2]).id.lane_id);
    EXPECT_TRUE(std::get<Rectangle>(primitives[2]).id.item_id.empty());
    EXPECT_EQ("empty", std::get<Text>(primitives[3]).id.lane_id);
    EXPECT_TRUE(std::get<Text>(primitives[3]).id.item_id.empty());
}

TEST(Layout, samples_analytic_curves_without_a_frame_grid)
{
    Document document(100);
    Lane lane("signal", "Signal", "curve", at(0), at(100));
    lane.add(Curve("analytic", "signal", at(0), at(100),
        [](Time time) { return static_cast<double>(time.ticks() * time.ticks()); }));
    document.add_lane(std::move(lane));
    const Layout layout(document, Viewport(200, 100, at(0), at(100)), LayoutMetrics(100, 20, 30, 4));
    bool found = false;
    for (const Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<Polyline>(primitive))
        {
            const Polyline &line = std::get<Polyline>(primitive);
            if (line.style == StyleRole::CURVE)
            {
                found = true;
                ASSERT_EQ(101, size_cast(line.points));
                EXPECT_EQ(100, line.points.front().x);
                EXPECT_EQ(200, line.points.back().x);
                EXPECT_EQ(40, line.points[50].y);
            }
        }
    }
    EXPECT_TRUE(found);
}

TEST(Layout, emits_event_and_interval_primitives)
{
    Lane lane("music", "Music events", "events", at(0), at(100));
    lane.add(Instant("beat-1", "beat", at(25)));
    lane.add(Interval("phrase-1", "phrase", at(40), at(60)));
    lane.add(Envelope("pulse-1", "pulse", at(70), lasting(10), lasting(10), lasting(10), {}, std::nullopt, {}));

    Document document(1000);
    document.add_lane(std::move(lane));

    const Layout layout(document, Viewport(400, 100, at(0), at(100)), LayoutMetrics(100, 20, 30, 4));
    const std::vector<Primitive> &primitives = layout.display_list().primitives();

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
    Lane lane("rms", "RMS", "curve", at(0), at(40));
    lane.add(Curve(
        "rms", "rms", {{at(0), 0.0}, {at(20), 1.0}, {at(40), 0.0}}, "RMS", CurveInterpolation::LINEAR, 0.0, 1.0, {}));

    Document document(FrameGrid(Timebase(10), 4, 1, 1), 0, 0);
    document.add_lane(std::move(lane));

    const Layout layout(document, Viewport(500, 100, at(0), at(40)), LayoutMetrics(100, 20, 30, 4));
    const std::vector<Primitive> &primitives = layout.display_list().primitives();

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
    Lane lane("zoom", "camera.zoom", "keyframes", at(0), at(50));
    lane.add(Keyframe("zoom-0", at(0), 0.0, KeyframeInterpolation::LINEAR, {}));
    lane.add(Keyframe("zoom-20", at(20), 1.0, KeyframeInterpolation::HOLD, {}));
    lane.add(Keyframe("zoom-40", at(40), 0.0));

    Document document(FrameGrid(Timebase(10), 5, 1, 1), 1, 3);
    document.add_lane(std::move(lane));

    const Layout layout(document, Viewport(500, 100, at(0), at(50)), LayoutMetrics(100, 20, 30, 4));
    const std::vector<Primitive> &primitives = layout.display_list().primitives();

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
    const Viewport viewport(300, 100, at(0), at(100));
    const LayoutMetrics metrics(100, 20, 30, 4);

    EXPECT_EQ(0, time_at_x(0, viewport, metrics).ticks());
    EXPECT_EQ(0, time_at_x(100, viewport, metrics).ticks());
    EXPECT_EQ(50, time_at_x(200, viewport, metrics).ticks());
    EXPECT_EQ(100, time_at_x(300, viewport, metrics).ticks());
    EXPECT_EQ(100, time_at_x(400, viewport, metrics).ticks());
}

TEST(Layout, computes_visible_frame_range)
{
    const FrameGrid grid(Timebase(100), 10, 10, 1);
    const std::optional<FrameRange> range = visible_frame_range(grid, Viewport(300, 100, at(25), at(65)));

    ASSERT_TRUE(range.has_value());
    EXPECT_EQ(2, range->first());
    EXPECT_EQ(6, range->last());
}

TEST(Layout, maps_frames_to_pixels_and_back)
{
    const FrameGrid grid(Timebase(100), 10, 10, 1);
    const Viewport viewport(300, 100, at(20), at(60));
    const LayoutMetrics metrics(100, 20, 20, 4);

    EXPECT_EQ(100, frame_x(2, grid, viewport, metrics));
    EXPECT_EQ(200, frame_x(4, grid, viewport, metrics));
    EXPECT_EQ(300, frame_x(6, grid, viewport, metrics));
    EXPECT_EQ(3, *frame_at_x(150, grid, viewport, metrics));
    EXPECT_EQ(5, *frame_at_x(250, grid, viewport, metrics));
}

TEST(Layout, keeps_lane_heights_stable_while_scrolling)
{
    Document document(100);
    document.add_lane(Lane("a", "Lane A", "events", at(0), at(100)));
    document.add_lane(Lane("b", "Lane B", "events", at(0), at(100)));
    document.add_lane(Lane("c", "Lane C", "events", at(0), at(100)));
    document.add_lane(Lane("d", "Lane D", "events", at(0), at(100)));
    const Viewport viewport(300, 75, at(0), at(100), 1);
    const LayoutMetrics metrics(100, 20, 20, 4);
    const Layout layout(document, viewport, metrics);
    const std::vector<Primitive> &primitives = layout.display_list().primitives();

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
    Document document(100);
    document.add_lane(Lane("lane", "Lane", "events", at(0), at(100)));
    Navigation navigation(at(0), at(100), 5);

    navigation.zoom_by(2.0, at(50));
    Viewport viewport = navigation.viewport(300, 100);
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

TEST(Navigation, reveals_positions_without_changing_zoom_or_lane)
{
    Navigation navigation(at(0), at(100), 5);
    navigation.zoom_by(2.0, at(50));
    navigation.scroll_to_lane(3, 2);
    navigation.reveal(at(80));
    Viewport viewport = navigation.viewport(300, 100);
    EXPECT_EQ(31, viewport.start().ticks());
    EXPECT_EQ(81, viewport.end().ticks());
    EXPECT_EQ(3, viewport.first_lane());
    EXPECT_DOUBLE_EQ(2.0, navigation.zoom_scale());

    navigation.reveal(at(40));
    EXPECT_EQ(31, navigation.viewport(300, 100).start().ticks());
    navigation.reveal(at(10));
    EXPECT_EQ(10, navigation.viewport(300, 100).start().ticks());
    navigation.reveal(at(-100));
    EXPECT_EQ(0, navigation.viewport(300, 100).start().ticks());
    navigation.reveal(at(200));
    viewport = navigation.viewport(300, 100);
    EXPECT_EQ(50, viewport.start().ticks());
    EXPECT_EQ(100, viewport.end().ticks());
}

TEST(Layout, produces_identical_geometry_for_identical_metrics)
{
    Lane lane("music", "Music", "events", at(0), at(100));
    lane.add(Instant("beat", "beat", at(50)));
    Document document(100);
    document.add_lane(std::move(lane));
    const Viewport viewport(320, 80, at(0), at(100));
    const LayoutMetrics metrics(100, 20, 30, 4);
    const Layout first(document, viewport, metrics);
    const Layout second(document, viewport, metrics);
    const std::vector<Primitive> &first_primitives = first.display_list().primitives();
    const std::vector<Primitive> &second_primitives = second.display_list().primitives();

    ASSERT_EQ(first_primitives.size(), second_primitives.size());
    for (int index = 0; index < size_cast(first_primitives); ++index)
    {
        expect_same_geometry(first_primitives[index], second_primitives[index]);
    }
}

TEST(Layout, hits_ruler_headers_and_empty_lane_body)
{
    Document document(100);
    document.add_lane(Lane("music", "Music", "events", at(0), at(100)));
    const Layout layout(document, Viewport(400, 100, at(0), at(100)), LayoutMetrics(100, 20, 30, 4));

    const std::optional<HitResult> ruler = layout.hit_test(Point{150, 10}, 3);
    ASSERT_TRUE(ruler);
    EXPECT_EQ(StyleRole::RULER, ruler->style);
    EXPECT_EQ("ruler", ruler->id.item_id);
    const std::optional<HitResult> header = layout.hit_test(Point{10, 25}, 3);
    ASSERT_TRUE(header);
    EXPECT_EQ(StyleRole::LANE_LABEL, header->style);
    EXPECT_EQ("music", header->id.lane_id);
    EXPECT_TRUE(header->id.item_id.empty());
    const std::optional<HitResult> body = layout.hit_test(Point{150, 25}, 3);
    ASSERT_TRUE(body);
    EXPECT_EQ(StyleRole::LANE_BACKGROUND, body->style);
    EXPECT_EQ("music", body->id.lane_id);
}

TEST(Layout, hits_item_geometry_and_preserves_display_ids)
{
    Lane lane("music", "Music", "events", at(0), at(100));
    lane.add(Instant("beat", "beat", at(25)));
    lane.add(Interval("phrase", "phrase", at(40), at(60)));
    lane.add(Envelope("pulse", "pulse", at(70), lasting(10), lasting(10), lasting(10), {}, std::nullopt, {}));
    Document document(100);
    document.add_lane(std::move(lane));
    const Layout layout(document, Viewport(400, 100, at(0), at(100)), LayoutMetrics(100, 20, 30, 4));

    for (const Primitive &primitive : layout.display_list().primitives())
    {
        std::visit(
            [&layout](const auto &value)
            {
                using Value = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, Rectangle> || std::is_same_v<Value, Marker>)
                {
                    if (!value.id.item_id.empty())
                    {
                        const std::optional<HitResult> hit = layout.hit_test(Point{value.x, value.y}, 0);
                        ASSERT_TRUE(hit);
                        EXPECT_EQ(value.style, hit->style);
                        EXPECT_EQ(value.id.lane_id, hit->id.lane_id);
                        EXPECT_EQ(value.id.item_id, hit->id.item_id);
                    }
                }
            },
            primitive);
    }
    const std::optional<HitResult> nearby_marker = layout.hit_test(Point{171, 30}, 3);
    ASSERT_TRUE(nearby_marker);
    EXPECT_EQ("beat", nearby_marker->id.item_id);
    EXPECT_EQ(StyleRole::LANE_BACKGROUND, layout.hit_test(Point{171, 30}, 0)->style);
    EXPECT_EQ(StyleRole::LANE_BACKGROUND, layout.hit_test(Point{280, 30}, 0)->style);
}

TEST(Layout, prefers_last_painted_item_when_markers_overlap)
{
    Lane lane("music", "Music", "events", at(0), at(100));
    lane.add(Instant("first", "note", at(25)));
    lane.add(Instant("second", "effect", at(25)));
    Document document(100);
    document.add_lane(std::move(lane));
    const Layout layout(document, Viewport(400, 100, at(0), at(100)), LayoutMetrics(100, 20, 30, 4));

    const std::optional<HitResult> hit = layout.hit_test(Point{175, 30}, 3);
    ASSERT_TRUE(hit);
    EXPECT_EQ("second", hit->id.item_id);
}

TEST(Layout, hits_curve_segments_using_host_tolerance)
{
    Lane lane("rms", "RMS", "curve", at(0), at(100));
    lane.add(Curve("signal", "rms", {{at(0), 0.0}, {at(100), 1.0}}, "RMS", CurveInterpolation::LINEAR, 0.0, 1.0, {}));
    Document document(100);
    document.add_lane(std::move(lane));
    const Layout layout(document, Viewport(400, 100, at(0), at(100)), LayoutMetrics(100, 20, 30, 4));

    const std::optional<HitResult> hit = layout.hit_test(Point{250, 36}, 3);
    ASSERT_TRUE(hit);
    EXPECT_EQ(StyleRole::CURVE, hit->style);
    EXPECT_EQ("rms", hit->id.lane_id);
    EXPECT_EQ("signal", hit->id.item_id);
    EXPECT_EQ(StyleRole::LANE_BACKGROUND, layout.hit_test(Point{250, 40}, 3)->style);
    EXPECT_THROW(layout.hit_test(Point{250, 36}, -1), std::invalid_argument);
}

TEST(Layout, keyframe_markers_take_priority_over_interpolation_segments)
{
    Lane lane("zoom", "Zoom", "keyframes", at(0), at(100));
    lane.add(Keyframe("start", at(0), 0.0, KeyframeInterpolation::LINEAR, {}));
    lane.add(Keyframe("end", at(50), 1.0));
    Document document(100);
    document.add_lane(std::move(lane));
    const Layout layout(document, Viewport(400, 100, at(0), at(100)), LayoutMetrics(100, 20, 30, 4));

    const std::optional<HitResult> marker = layout.hit_test(Point{250, 24}, 3);
    ASSERT_TRUE(marker);
    EXPECT_EQ(StyleRole::KEYFRAME_MARKER, marker->style);
    EXPECT_EQ("end", marker->id.item_id);
    const std::optional<HitResult> segment = layout.hit_test(Point{175, 35}, 3);
    ASSERT_TRUE(segment);
    EXPECT_EQ(StyleRole::KEYFRAME_SEGMENT, segment->style);
    EXPECT_EQ("start", segment->id.item_id);
}

TEST(Layout, does_not_hit_outside_viewport_or_in_unused_rows)
{
    Document document(100);
    document.add_lane(Lane("hidden", "Hidden", "events", at(0), at(100)));
    document.add_lane(Lane("visible", "Visible", "events", at(0), at(100)));
    const Layout layout(document, Viewport(400, 100, at(0), at(100), 1), LayoutMetrics(100, 20, 30, 4));

    EXPECT_FALSE(layout.hit_test(Point{-1, 10}, 3));
    EXPECT_FALSE(layout.hit_test(Point{150, -1}, 3));
    EXPECT_FALSE(layout.hit_test(Point{400, 25}, 3));
    EXPECT_FALSE(layout.hit_test(Point{150, 100}, 3));
    EXPECT_FALSE(layout.hit_test(Point{150, 50}, 3));
    EXPECT_EQ("visible", layout.hit_test(Point{10, 25}, 3)->id.lane_id);
}
