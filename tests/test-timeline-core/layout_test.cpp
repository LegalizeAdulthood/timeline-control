// Copyright (c) 2026 Richard Thomson

#include <timeline/Layout.h>
#include <timeline/size_cast.h>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>

using namespace timeline;

TEST(HitResult, formatsResolvedDocumentText)
{
    StringTableBuilder strings;
    const StringId lane_id = strings.intern("music");
    const StringId item_id = strings.intern("beat-1");
    DocumentBuilder builder(Document(100), std::move(strings).build());
    const Document document = std::move(builder).build();
    const HitResult hit{StyleRole::INSTANT_MARKER, DisplayId{lane_id, item_id}};

    const std::string text = to_string(document, hit);

    EXPECT_EQ("Hit lane: music\nHit item: beat-1", text);
}

TEST(Layout, samplesGeometricKeyframeSegmentsAtGridBoundaries)
{
    const FrameGrid grid(Timebase(100), 3, 10, 1);
    DocumentBuilder builder(Document(grid, 1, 2));
    Lane lane(
        builder.intern("zoom"), builder.intern("Zoom"), builder.intern("keyframes"), grid.offset(), grid.end_time());
    lane.add(Keyframe(builder.intern("first"), grid.frame_start(0), 1.0, KeyframeInterpolation::GEOMETRIC, {}));
    lane.add(Keyframe(builder.intern("last"), grid.frame_start(2), 9.0));
    builder.add_lane(std::move(lane));
    const Document document = std::move(builder).build();
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
            else if constexpr (std::is_same_v<Value, Rectangle> || std::is_same_v<Value, Marker> ||
                std::is_same_v<Value, Swatch>)
            {
                EXPECT_EQ(left.x, right.x);
                EXPECT_EQ(left.y, right.y);
                EXPECT_EQ(left.width, right.width);
                EXPECT_EQ(left.height, right.height);
                if constexpr (std::is_same_v<Value, Swatch>)
                {
                    EXPECT_EQ(left.color, right.color);
                }
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

TEST(Layout, emitsRulerAndEmptyLaneScaffolding)
{
    DocumentBuilder builder(Document(100));
    builder.add_lane(
        Lane(builder.intern("empty"), builder.intern("Empty lane"), builder.intern("events"), at(0), at(100)));
    const Document document = std::move(builder).build();

    const Layout layout(document, Viewport(400, 100, at(0), at(100)), LayoutMetrics(100, 20, 30, 4));
    const std::vector<Primitive> &primitives = layout.display_list().primitives();

    ASSERT_EQ(4U, primitives.size());
    EXPECT_TRUE(std::get<Line>(primitives[0]).id.item_id.empty());
    EXPECT_TRUE(std::get<Text>(primitives[1]).id.item_id.empty());
    EXPECT_EQ(document.lanes().front().id(), std::get<Rectangle>(primitives[2]).id.lane_id);
    EXPECT_TRUE(std::get<Rectangle>(primitives[2]).id.item_id.empty());
    EXPECT_EQ(document.lanes().front().id(), std::get<Text>(primitives[3]).id.lane_id);
    EXPECT_TRUE(std::get<Text>(primitives[3]).id.item_id.empty());
}

TEST(Layout, samplesAnalyticCurvesWithoutAFrameGrid)
{
    DocumentBuilder builder(Document(100));
    Lane lane(builder.intern("signal"), builder.intern("Signal"), builder.intern("curve"), at(0), at(100));
    lane.add(Curve(builder.intern("analytic"), builder.intern("signal"), at(0), at(100),
        [](Time time) { return static_cast<double>(time.ticks() * time.ticks()); }));
    builder.add_lane(std::move(lane));
    const Document document = std::move(builder).build();
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

TEST(Layout, emitsEventAndIntervalPrimitives)
{
    DocumentBuilder builder(Document(1000));
    Lane lane(builder.intern("music-events"), builder.intern("Music events"), builder.intern("events"), at(0), at(100));
    const StringId beat_id = builder.intern("beat-1");
    const StringId phrase_id = builder.intern("phrase-1");
    const StringId pulse_id = builder.intern("pulse-1");
    lane.add(Instant(beat_id, builder.intern("beat"), at(25)));
    lane.add(Interval(phrase_id, builder.intern("phrase"), at(40), at(60)));
    lane.add(Envelope(
        pulse_id, builder.intern("pulse"), at(70), lasting(10), lasting(10), lasting(10), {}, std::nullopt, {}));

    builder.add_lane(std::move(lane));
    const Document document = std::move(builder).build();

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
    EXPECT_EQ("Music events", layout.display_list().strings().lookup(std::get<Text>(primitives[3]).value));

    const auto &instant = std::get<Marker>(primitives[4]);
    EXPECT_EQ(174, instant.x);
    EXPECT_EQ(2, instant.width);
    EXPECT_EQ(StyleRole::INSTANT_MARKER, instant.style);
    EXPECT_EQ(document.lanes().front().id(), instant.id.lane_id);
    EXPECT_EQ(beat_id, instant.id.item_id);

    const auto &interval = std::get<Rectangle>(primitives[5]);
    EXPECT_EQ(220, interval.x);
    EXPECT_EQ(60, interval.width);
    EXPECT_EQ(StyleRole::INTERVAL_SPAN, interval.style);
    EXPECT_EQ(document.lanes().front().id(), interval.id.lane_id);
    EXPECT_EQ(phrase_id, interval.id.item_id);

    EXPECT_EQ(StyleRole::ENVELOPE_ATTACK, std::get<Rectangle>(primitives[6]).style);
    EXPECT_EQ(StyleRole::ENVELOPE_SUSTAIN, std::get<Rectangle>(primitives[7]).style);
    EXPECT_EQ(StyleRole::ENVELOPE_DECAY, std::get<Rectangle>(primitives[8]).style);
    EXPECT_EQ(pulse_id, std::get<Rectangle>(primitives[6]).id.item_id);
    EXPECT_EQ(pulse_id, std::get<Rectangle>(primitives[7]).id.item_id);
    EXPECT_EQ(pulse_id, std::get<Rectangle>(primitives[8]).id.item_id);
}

TEST(Layout, emitsCurvePolylineSampledAtFrameBoundaries)
{
    DocumentBuilder builder(Document(FrameGrid(Timebase(10), 4, 1, 1), 0, 0));
    const StringId rms_id = builder.intern("rms");
    Lane lane(rms_id, builder.intern("RMS"), builder.intern("curve"), at(0), at(40));
    lane.add(Curve(rms_id, builder.intern("rms"), {{at(0), 0.0}, {at(20), 1.0}, {at(40), 0.0}}, builder.intern("RMS"),
        CurveInterpolation::LINEAR, 0.0, 1.0, {}));

    builder.add_lane(std::move(lane));
    const Document document = std::move(builder).build();

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
    EXPECT_EQ(rms_id, polyline.id.lane_id);
    EXPECT_EQ(rms_id, polyline.id.item_id);
}

TEST(Layout, emitsKeyframeMarkersAndInterpolationSegments)
{
    DocumentBuilder builder(Document(FrameGrid(Timebase(10), 5, 1, 1), 1, 3));
    Lane lane(builder.intern("camera.zoom"), builder.intern("camera.zoom"), builder.intern("keyframes"), at(0), at(50));
    const StringId zoom_0_id = builder.intern("zoom-0");
    const StringId zoom_20_id = builder.intern("zoom-20");
    const StringId zoom_40_id = builder.intern("zoom-40");
    lane.add(Keyframe(zoom_0_id, at(0), 0.0, KeyframeInterpolation::LINEAR, {}));
    lane.add(Keyframe(zoom_20_id, at(20), 1.0, KeyframeInterpolation::HOLD, {}));
    lane.add(Keyframe(zoom_40_id, at(40), 0.0));

    builder.add_lane(std::move(lane));
    const Document document = std::move(builder).build();

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
    EXPECT_EQ(document.lanes().front().id(), linear.id.lane_id);
    EXPECT_EQ(zoom_0_id, linear.id.item_id);
    EXPECT_EQ(document.lanes().front().id(), hold.id.lane_id);
    EXPECT_EQ(zoom_20_id, hold.id.item_id);

    EXPECT_EQ(StyleRole::KEYFRAME_MARKER, std::get<Marker>(primitives[6]).style);
    EXPECT_EQ(zoom_0_id, std::get<Marker>(primitives[6]).id.item_id);
    EXPECT_EQ(StyleRole::KEYFRAME_MARKER, std::get<Marker>(primitives[7]).style);
    EXPECT_EQ(zoom_20_id, std::get<Marker>(primitives[7]).id.item_id);
    EXPECT_EQ(StyleRole::KEYFRAME_MARKER, std::get<Marker>(primitives[8]).style);
    EXPECT_EQ(zoom_40_id, std::get<Marker>(primitives[8]).id.item_id);
}

TEST(Layout, mapsHorizontalPositionsToTimelineTime)
{
    const Viewport viewport(300, 100, at(0), at(100));
    const LayoutMetrics metrics(100, 20, 30, 4);

    EXPECT_EQ(0, time_at_x(0, viewport, metrics).ticks());
    EXPECT_EQ(0, time_at_x(100, viewport, metrics).ticks());
    EXPECT_EQ(50, time_at_x(200, viewport, metrics).ticks());
    EXPECT_EQ(100, time_at_x(300, viewport, metrics).ticks());
    EXPECT_EQ(100, time_at_x(400, viewport, metrics).ticks());
}

TEST(Layout, computesVisibleFrameRange)
{
    const FrameGrid grid(Timebase(100), 10, 10, 1);
    const std::optional<FrameRange> range = visible_frame_range(grid, Viewport(300, 100, at(25), at(65)));

    ASSERT_TRUE(range.has_value());
    EXPECT_EQ(2, range->first());
    EXPECT_EQ(6, range->last());
}

TEST(Layout, mapsFramesToPixelsAndBack)
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

TEST(Layout, keepsLaneHeightsStableWhileScrolling)
{
    DocumentBuilder builder(Document(100));
    builder.add_lane(
        Lane(builder.intern("lane-a"), builder.intern("Lane A"), builder.intern("events"), at(0), at(100)));
    builder.add_lane(
        Lane(builder.intern("lane-b"), builder.intern("Lane B"), builder.intern("events"), at(0), at(100)));
    builder.add_lane(
        Lane(builder.intern("lane-c"), builder.intern("Lane C"), builder.intern("events"), at(0), at(100)));
    builder.add_lane(
        Lane(builder.intern("lane-d"), builder.intern("Lane D"), builder.intern("events"), at(0), at(100)));
    const Document document = std::move(builder).build();
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
    EXPECT_EQ("Lane B", layout.display_list().strings().lookup(std::get<Text>(primitives[3]).value));
    EXPECT_EQ(20, std::get<Rectangle>(primitives[4]).height);
    EXPECT_EQ("Lane C", layout.display_list().strings().lookup(std::get<Text>(primitives[5]).value));
}

TEST(Layout, navigatesExactRangesWithoutChangingDocumentData)
{
    DocumentBuilder builder(Document(100));
    builder.add_lane(Lane(builder.intern("lane"), builder.intern("Lane"), builder.intern("events"), at(0), at(100)));
    const Document document = std::move(builder).build();
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
    EXPECT_EQ(*document.strings().find("lane"), document.lanes()[0].id());
}

TEST(Navigation, revealsPositionsWithoutChangingZoomOrLane)
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

TEST(Layout, producesIdenticalGeometryForIdenticalMetrics)
{
    DocumentBuilder builder(Document(100));
    Lane lane(builder.intern("music"), builder.intern("Music"), builder.intern("events"), at(0), at(100));
    lane.add(Instant(builder.intern("beat"), builder.intern("beat"), at(50)));
    builder.add_lane(std::move(lane));
    const Document document = std::move(builder).build();
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

TEST(Layout, hitsRulerHeadersAndEmptyLaneBody)
{
    DocumentBuilder builder(Document(100));
    builder.add_lane(Lane(builder.intern("music"), builder.intern("Music"), builder.intern("events"), at(0), at(100)));
    const Document document = std::move(builder).build();
    const Layout layout(document, Viewport(400, 100, at(0), at(100)), LayoutMetrics(100, 20, 30, 4));

    const std::optional<HitResult> ruler = layout.hit_test(Point{150, 10}, 3);
    ASSERT_TRUE(ruler);
    EXPECT_EQ(StyleRole::RULER, ruler->style);
    EXPECT_TRUE(ruler->id.item_id.empty());
    const std::optional<HitResult> header = layout.hit_test(Point{10, 25}, 3);
    ASSERT_TRUE(header);
    EXPECT_EQ(StyleRole::LANE_LABEL, header->style);
    EXPECT_EQ(document.lanes().front().id(), header->id.lane_id);
    EXPECT_TRUE(header->id.item_id.empty());
    const std::optional<HitResult> body = layout.hit_test(Point{150, 25}, 3);
    ASSERT_TRUE(body);
    EXPECT_EQ(StyleRole::LANE_BACKGROUND, body->style);
    EXPECT_EQ(document.lanes().front().id(), body->id.lane_id);
}

TEST(Layout, hitsItemGeometryAndPreservesDisplayIds)
{
    DocumentBuilder builder(Document(100));
    Lane lane(builder.intern("music"), builder.intern("Music"), builder.intern("events"), at(0), at(100));
    const StringId beat_id = builder.intern("beat");
    lane.add(Instant(beat_id, builder.intern("beat"), at(25)));
    lane.add(Interval(builder.intern("phrase"), builder.intern("phrase"), at(40), at(60)));
    lane.add(Envelope(builder.intern("pulse"), builder.intern("pulse"), at(70), lasting(10), lasting(10), lasting(10),
        {}, std::nullopt, {}));
    builder.add_lane(std::move(lane));
    const Document document = std::move(builder).build();
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
    EXPECT_EQ(beat_id, nearby_marker->id.item_id);
    EXPECT_EQ(StyleRole::LANE_BACKGROUND, layout.hit_test(Point{171, 30}, 0)->style);
    EXPECT_EQ(StyleRole::LANE_BACKGROUND, layout.hit_test(Point{280, 30}, 0)->style);
}

TEST(Layout, prefersLastPaintedItemWhenMarkersOverlap)
{
    DocumentBuilder builder(Document(100));
    Lane lane(builder.intern("music"), builder.intern("Music"), builder.intern("events"), at(0), at(100));
    lane.add(Instant(builder.intern("first"), builder.intern("note"), at(25)));
    const StringId second_id = builder.intern("second");
    lane.add(Instant(second_id, builder.intern("effect"), at(25)));
    builder.add_lane(std::move(lane));
    const Document document = std::move(builder).build();
    const Layout layout(document, Viewport(400, 100, at(0), at(100)), LayoutMetrics(100, 20, 30, 4));

    const std::optional<HitResult> hit = layout.hit_test(Point{175, 30}, 3);
    ASSERT_TRUE(hit);
    EXPECT_EQ(second_id, hit->id.item_id);
}

TEST(Layout, hitsCurveSegmentsUsingHostTolerance)
{
    DocumentBuilder builder(Document(100));
    Lane lane(builder.intern("rms"), builder.intern("RMS"), builder.intern("curve"), at(0), at(100));
    const StringId signal_id = builder.intern("signal");
    lane.add(Curve(signal_id, builder.intern("rms"), {{at(0), 0.0}, {at(100), 1.0}}, builder.intern("RMS"),
        CurveInterpolation::LINEAR, 0.0, 1.0, {}));
    builder.add_lane(std::move(lane));
    const Document document = std::move(builder).build();
    const Layout layout(document, Viewport(400, 100, at(0), at(100)), LayoutMetrics(100, 20, 30, 4));

    const std::optional<HitResult> hit = layout.hit_test(Point{250, 36}, 3);
    ASSERT_TRUE(hit);
    EXPECT_EQ(StyleRole::CURVE, hit->style);
    EXPECT_EQ(document.lanes().front().id(), hit->id.lane_id);
    EXPECT_EQ(signal_id, hit->id.item_id);
    EXPECT_EQ(StyleRole::LANE_BACKGROUND, layout.hit_test(Point{250, 40}, 3)->style);
    EXPECT_THROW(layout.hit_test(Point{250, 36}, -1), std::invalid_argument);
}

TEST(Layout, keyframeMarkersTakePriorityOverInterpolationSegments)
{
    DocumentBuilder builder(Document(100));
    Lane lane(builder.intern("zoom"), builder.intern("Zoom"), builder.intern("keyframes"), at(0), at(100));
    const StringId start_id = builder.intern("start");
    const StringId end_id = builder.intern("end");
    lane.add(Keyframe(start_id, at(0), 0.0, KeyframeInterpolation::LINEAR, {}));
    lane.add(Keyframe(end_id, at(50), 1.0));
    builder.add_lane(std::move(lane));
    const Document document = std::move(builder).build();
    const Layout layout(document, Viewport(400, 100, at(0), at(100)), LayoutMetrics(100, 20, 30, 4));

    const std::optional<HitResult> marker = layout.hit_test(Point{250, 24}, 3);
    ASSERT_TRUE(marker);
    EXPECT_EQ(StyleRole::KEYFRAME_MARKER, marker->style);
    EXPECT_EQ(end_id, marker->id.item_id);
    const std::optional<HitResult> segment = layout.hit_test(Point{175, 35}, 3);
    ASSERT_TRUE(segment);
    EXPECT_EQ(StyleRole::KEYFRAME_SEGMENT, segment->style);
    EXPECT_EQ(start_id, segment->id.item_id);
}

TEST(Layout, doesNotHitOutsideViewportOrInUnusedRows)
{
    DocumentBuilder builder(Document(100));
    builder.add_lane(
        Lane(builder.intern("hidden"), builder.intern("Hidden"), builder.intern("events"), at(0), at(100)));
    builder.add_lane(
        Lane(builder.intern("visible"), builder.intern("Visible"), builder.intern("events"), at(0), at(100)));
    const Document document = std::move(builder).build();
    const Layout layout(document, Viewport(400, 100, at(0), at(100), 1), LayoutMetrics(100, 20, 30, 4));

    EXPECT_FALSE(layout.hit_test(Point{-1, 10}, 3));
    EXPECT_FALSE(layout.hit_test(Point{150, -1}, 3));
    EXPECT_FALSE(layout.hit_test(Point{400, 25}, 3));
    EXPECT_FALSE(layout.hit_test(Point{150, 100}, 3));
    EXPECT_FALSE(layout.hit_test(Point{150, 50}, 3));
    EXPECT_EQ(document.lanes()[1].id(), layout.hit_test(Point{10, 25}, 3)->id.lane_id);
}
