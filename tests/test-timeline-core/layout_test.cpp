// Copyright (c) 2026 Richard Thomson

#include <timeline/Layout.h>
#include <timeline/size_cast.h>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

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

template <typename Value>
const Value &primitive_with_style(const Layout &layout, StyleRole style)
{
    for (const Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<Value>(primitive))
        {
            const Value &value = std::get<Value>(primitive);
            if (value.style == style)
            {
                return value;
            }
        }
    }
    throw std::logic_error("layout does not contain the requested primitive");
}

template <typename Value>
std::vector<Value> primitives_with_style(const Layout &layout, StyleRole style)
{
    std::vector<Value> result;
    for (const Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<Value>(primitive))
        {
            const Value &value = std::get<Value>(primitive);
            if (value.style == style)
            {
                result.push_back(value);
            }
        }
    }
    return result;
}

Document empty_lane_document()
{
    DocumentBuilder builder(Document(100));
    builder.add_lane(
        Lane(builder.intern("empty"), builder.intern("Empty lane"), builder.intern("events"), at(0), at(100)));
    return std::move(builder).build();
}

Document event_document()
{
    DocumentBuilder builder(Document(1000));
    Lane lane(builder.intern("music-events"), builder.intern("Music events"), builder.intern("events"), at(0), at(100));
    lane.add(Instant(builder.intern("beat-1"), builder.intern("beat"), at(25)));
    lane.add(Interval(builder.intern("phrase-1"), builder.intern("phrase"), at(40), at(60)));
    lane.add(Envelope(builder.intern("pulse-1"), builder.intern("pulse"), at(70), lasting(10), lasting(10), lasting(10),
        {}, std::nullopt, {}));
    builder.add_lane(std::move(lane));
    return std::move(builder).build();
}

Document curve_document()
{
    DocumentBuilder builder(Document(FrameGrid(Timebase(10), 4, 1, 1), 0, 0));
    const StringId rms_id = builder.intern("rms");
    Lane lane(rms_id, builder.intern("RMS"), builder.intern("curve"), at(0), at(40));
    lane.add(Curve(rms_id, builder.intern("rms"), {{at(0), 0.0}, {at(20), 1.0}, {at(40), 0.0}}, builder.intern("RMS"),
        CurveInterpolation::LINEAR, 0.0, 1.0, {}));
    builder.add_lane(std::move(lane));
    return std::move(builder).build();
}

Document keyframe_document()
{
    DocumentBuilder builder(Document(FrameGrid(Timebase(10), 5, 1, 1), 1, 3));
    Lane lane(builder.intern("camera.zoom"), builder.intern("camera.zoom"), builder.intern("keyframes"), at(0), at(50));
    lane.add(Keyframe(builder.intern("zoom-0"), at(0), 0.0, KeyframeInterpolation::LINEAR, {}));
    lane.add(Keyframe(builder.intern("zoom-20"), at(20), 1.0, KeyframeInterpolation::HOLD, {}));
    lane.add(Keyframe(builder.intern("zoom-40"), at(40), 0.0));
    builder.add_lane(std::move(lane));
    return std::move(builder).build();
}

Document scrolled_lane_document()
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
    return std::move(builder).build();
}

Document hit_curve_document()
{
    DocumentBuilder builder(Document(100));
    Lane lane(builder.intern("rms"), builder.intern("RMS"), builder.intern("curve"), at(0), at(100));
    lane.add(Curve(builder.intern("signal"), builder.intern("rms"), {{at(0), 0.0}, {at(100), 1.0}},
        builder.intern("RMS"), CurveInterpolation::LINEAR, 0.0, 1.0, {}));
    builder.add_lane(std::move(lane));
    return std::move(builder).build();
}

void expect_same_geometry(const Primitive &lhs, const Primitive &rhs)
{
    ASSERT_EQ(lhs.index(), rhs.index());
    std::visit(
        [&rhs](const auto &left)
        {
            using Value = std::decay_t<decltype(left)>;
            const Value &right = std::get<Value>(rhs);
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

/// Empty-lane document rendered with the canonical layout geometry.
///
class EmptyLaneLayoutTest : public testing::Test
{
protected:
    const Document m_document{empty_lane_document()};
    const Viewport m_viewport{400, 100, at(0), at(100)};
    const LayoutMetrics m_metrics{100, 20, 30, 4};
    const Layout m_layout{m_document, m_viewport, m_metrics};
};

/// Event document rendered with the canonical layout geometry.
///
class EventLayoutTest : public testing::Test
{
protected:
    const Document m_document{event_document()};
    const Viewport m_viewport{400, 100, at(0), at(100)};
    const LayoutMetrics m_metrics{100, 20, 30, 4};
    const Layout m_layout{m_document, m_viewport, m_metrics};
};

/// Keyframe document rendered with the canonical layout geometry.
///
class KeyframeLayoutTest : public testing::Test
{
protected:
    const Document m_document{keyframe_document()};
    const Viewport m_viewport{500, 100, at(0), at(50)};
    const LayoutMetrics m_metrics{100, 20, 30, 4};
    const Layout m_layout{m_document, m_viewport, m_metrics};
};

} // namespace

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
    const Polyline &line = primitive_with_style<Polyline>(layout, StyleRole::KEYFRAME_SEGMENT);

    ASSERT_EQ(3, size_cast(line.points));
    EXPECT_EQ(40, line.points[1].y);
}

TEST_F(EmptyLaneLayoutTest, emitsRuler)
{
    const Line &ruler = primitive_with_style<Line>(m_layout, StyleRole::RULER);

    EXPECT_TRUE(ruler.id.lane_id.empty());
    EXPECT_TRUE(ruler.id.item_id.empty());
}

TEST_F(EmptyLaneLayoutTest, emitsRulerLabel)
{
    const Text &label = primitive_with_style<Text>(m_layout, StyleRole::RULER_LABEL);

    EXPECT_EQ("Time", m_layout.display_list().strings().lookup(label.value));
    EXPECT_TRUE(label.id.lane_id.empty());
    EXPECT_TRUE(label.id.item_id.empty());
}

TEST_F(EmptyLaneLayoutTest, emitsEmptyLaneBackground)
{
    const Rectangle &background = primitive_with_style<Rectangle>(m_layout, StyleRole::LANE_BACKGROUND);

    EXPECT_EQ(100, background.x);
    EXPECT_EQ(20, background.y);
    EXPECT_EQ(300, background.width);
    EXPECT_EQ(30, background.height);
    EXPECT_EQ(m_document.lanes().front().id(), background.id.lane_id);
    EXPECT_TRUE(background.id.item_id.empty());
}

TEST_F(EmptyLaneLayoutTest, emitsEmptyLaneLabel)
{
    const Text &label = primitive_with_style<Text>(m_layout, StyleRole::LANE_LABEL);

    EXPECT_EQ("Empty lane", m_layout.display_list().strings().lookup(label.value));
    EXPECT_EQ(m_document.lanes().front().id(), label.id.lane_id);
    EXPECT_TRUE(label.id.item_id.empty());
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
    const Polyline &line = primitive_with_style<Polyline>(layout, StyleRole::CURVE);

    ASSERT_EQ(101, size_cast(line.points));
    EXPECT_EQ(100, line.points.front().x);
    EXPECT_EQ(200, line.points.back().x);
    EXPECT_EQ(40, line.points[50].y);
}

TEST_F(EventLayoutTest, emitsInstantMarker)
{
    const StringId beat_id = *m_document.strings().find("beat-1");

    const Marker &marker = primitive_with_style<Marker>(m_layout, StyleRole::INSTANT_MARKER);

    EXPECT_EQ(174, marker.x);
    EXPECT_EQ(2, marker.width);
    EXPECT_EQ(m_document.lanes().front().id(), marker.id.lane_id);
    EXPECT_EQ(beat_id, marker.id.item_id);
}

TEST_F(EventLayoutTest, emitsIntervalSpan)
{
    const StringId phrase_id = *m_document.strings().find("phrase-1");

    const Rectangle &interval = primitive_with_style<Rectangle>(m_layout, StyleRole::INTERVAL_SPAN);

    EXPECT_EQ(220, interval.x);
    EXPECT_EQ(60, interval.width);
    EXPECT_EQ(m_document.lanes().front().id(), interval.id.lane_id);
    EXPECT_EQ(phrase_id, interval.id.item_id);
}

TEST_F(EventLayoutTest, emitsEnvelopePhases)
{
    const StringId pulse_id = *m_document.strings().find("pulse-1");

    const Rectangle &attack = primitive_with_style<Rectangle>(m_layout, StyleRole::ENVELOPE_ATTACK);
    const Rectangle &sustain = primitive_with_style<Rectangle>(m_layout, StyleRole::ENVELOPE_SUSTAIN);
    const Rectangle &decay = primitive_with_style<Rectangle>(m_layout, StyleRole::ENVELOPE_DECAY);

    EXPECT_EQ(pulse_id, attack.id.item_id);
    EXPECT_EQ(pulse_id, sustain.id.item_id);
    EXPECT_EQ(pulse_id, decay.id.item_id);
}

TEST(Layout, emitsCurvePolylineSampledAtFrameBoundaries)
{
    const Document document = curve_document();
    const StringId rms_id = *document.strings().find("rms");

    const Layout layout(document, Viewport(500, 100, at(0), at(40)), LayoutMetrics(100, 20, 30, 4));
    const Polyline &polyline = primitive_with_style<Polyline>(layout, StyleRole::CURVE);

    ASSERT_EQ(4, size_cast(polyline.points));
    EXPECT_EQ(100, polyline.points[0].x);
    EXPECT_EQ(45, polyline.points[0].y);
    EXPECT_EQ(200, polyline.points[1].x);
    EXPECT_EQ(34, polyline.points[1].y);
    EXPECT_EQ(300, polyline.points[2].x);
    EXPECT_EQ(24, polyline.points[2].y);
    EXPECT_EQ(400, polyline.points[3].x);
    EXPECT_EQ(34, polyline.points[3].y);
    EXPECT_EQ(rms_id, polyline.id.lane_id);
    EXPECT_EQ(rms_id, polyline.id.item_id);
}

TEST_F(KeyframeLayoutTest, emitsLinearKeyframeSegment)
{
    const StringId start_id = *m_document.strings().find("zoom-0");

    const std::vector<Polyline> segments = primitives_with_style<Polyline>(m_layout, StyleRole::KEYFRAME_SEGMENT);

    ASSERT_EQ(2, size_cast(segments));
    ASSERT_EQ(2, size_cast(segments[0].points));
    EXPECT_EQ(100, segments[0].points[0].x);
    EXPECT_EQ(45, segments[0].points[0].y);
    EXPECT_EQ(260, segments[0].points[1].x);
    EXPECT_EQ(24, segments[0].points[1].y);
    EXPECT_EQ(m_document.lanes().front().id(), segments[0].id.lane_id);
    EXPECT_EQ(start_id, segments[0].id.item_id);
}

TEST_F(KeyframeLayoutTest, emitsHeldKeyframeSegment)
{
    const StringId held_id = *m_document.strings().find("zoom-20");

    const std::vector<Polyline> segments = primitives_with_style<Polyline>(m_layout, StyleRole::KEYFRAME_SEGMENT);

    ASSERT_EQ(2, size_cast(segments));
    ASSERT_EQ(3, size_cast(segments[1].points));
    EXPECT_EQ(260, segments[1].points[0].x);
    EXPECT_EQ(24, segments[1].points[0].y);
    EXPECT_EQ(420, segments[1].points[1].x);
    EXPECT_EQ(24, segments[1].points[1].y);
    EXPECT_EQ(420, segments[1].points[2].x);
    EXPECT_EQ(45, segments[1].points[2].y);
    EXPECT_EQ(m_document.lanes().front().id(), segments[1].id.lane_id);
    EXPECT_EQ(held_id, segments[1].id.item_id);
}

TEST_F(KeyframeLayoutTest, emitsKeyframeMarkers)
{
    const std::vector<Marker> markers = primitives_with_style<Marker>(m_layout, StyleRole::KEYFRAME_MARKER);

    ASSERT_EQ(3, size_cast(markers));
    EXPECT_EQ(*m_document.strings().find("zoom-0"), markers[0].id.item_id);
    EXPECT_EQ(*m_document.strings().find("zoom-20"), markers[1].id.item_id);
    EXPECT_EQ(*m_document.strings().find("zoom-40"), markers[2].id.item_id);
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

    ASSERT_TRUE(range);
    EXPECT_EQ(2, range->first());
    EXPECT_EQ(6, range->last());
}

TEST(Layout, mapsFramesToPixels)
{
    const FrameGrid grid(Timebase(100), 10, 10, 1);
    const Viewport viewport(300, 100, at(20), at(60));
    const LayoutMetrics metrics(100, 20, 20, 4);

    EXPECT_EQ(100, frame_x(2, grid, viewport, metrics));
    EXPECT_EQ(200, frame_x(4, grid, viewport, metrics));
    EXPECT_EQ(300, frame_x(6, grid, viewport, metrics));
}

TEST(Layout, mapsPixelsToFrames)
{
    const FrameGrid grid(Timebase(100), 10, 10, 1);
    const Viewport viewport(300, 100, at(20), at(60));
    const LayoutMetrics metrics(100, 20, 20, 4);

    EXPECT_EQ(3, *frame_at_x(150, grid, viewport, metrics));
    EXPECT_EQ(5, *frame_at_x(250, grid, viewport, metrics));
}

TEST(Layout, countsVisibleLanes)
{
    const Viewport viewport(300, 75, at(0), at(100), 1);
    const LayoutMetrics metrics(100, 20, 20, 4);

    EXPECT_EQ(2, visible_lane_count(viewport, metrics));
}

TEST(Layout, mapsLaneIndexesToY)
{
    const Viewport viewport(300, 75, at(0), at(100), 1);
    const LayoutMetrics metrics(100, 20, 20, 4);

    EXPECT_EQ(20, *lane_y(1, viewport, metrics));
    EXPECT_EQ(40, *lane_y(2, viewport, metrics));
    EXPECT_FALSE(lane_y(3, viewport, metrics));
}

TEST(Layout, mapsYToLaneIndexes)
{
    const Viewport viewport(300, 75, at(0), at(100), 1);
    const LayoutMetrics metrics(100, 20, 20, 4);

    EXPECT_EQ(1, *lane_at_y(20, 4, viewport, metrics));
    EXPECT_EQ(2, *lane_at_y(59, 4, viewport, metrics));
    EXPECT_FALSE(lane_at_y(60, 4, viewport, metrics));
}

TEST(Layout, rendersStableScrolledLaneHeights)
{
    const Document document = scrolled_lane_document();
    const Viewport viewport(300, 75, at(0), at(100), 1);
    const LayoutMetrics metrics(100, 20, 20, 4);

    const Layout layout(document, viewport, metrics);
    const std::vector<Rectangle> backgrounds = primitives_with_style<Rectangle>(layout, StyleRole::LANE_BACKGROUND);

    ASSERT_EQ(2, size_cast(backgrounds));
    EXPECT_EQ(20, backgrounds[0].height);
    EXPECT_EQ(20, backgrounds[1].height);
}

TEST(Layout, rendersScrolledLaneLabels)
{
    const Document document = scrolled_lane_document();
    const Viewport viewport(300, 75, at(0), at(100), 1);
    const LayoutMetrics metrics(100, 20, 20, 4);

    const Layout layout(document, viewport, metrics);
    const std::vector<Text> labels = primitives_with_style<Text>(layout, StyleRole::LANE_LABEL);

    ASSERT_EQ(2, size_cast(labels));
    EXPECT_EQ("Lane B", layout.display_list().strings().lookup(labels[0].value));
    EXPECT_EQ("Lane C", layout.display_list().strings().lookup(labels[1].value));
}

TEST(Navigation, zoomsAroundAnchor)
{
    Navigation navigation(at(0), at(100), 5);

    navigation.zoom_by(2.0, at(50));
    const Viewport viewport = navigation.viewport(300, 100);

    EXPECT_EQ(25, viewport.start().ticks());
    EXPECT_EQ(75, viewport.end().ticks());
    EXPECT_DOUBLE_EQ(2.0, navigation.zoom_scale());
}

TEST(Navigation, clampsAbsoluteScrollToContent)
{
    Navigation navigation(at(0), at(100), 5);
    navigation.zoom_by(2.0, at(50));

    navigation.scroll_to(at(90));
    const Viewport viewport = navigation.viewport(300, 100);

    EXPECT_EQ(50, viewport.start().ticks());
    EXPECT_EQ(100, viewport.end().ticks());
    EXPECT_EQ(50, navigation.horizontal_offset().ticks());
}

TEST(Navigation, scrollsToFraction)
{
    Navigation navigation(at(0), at(100), 5);
    navigation.zoom_by(2.0, at(50));
    navigation.scroll_to(at(90));

    navigation.scroll_to_fraction(0.0);
    const Viewport viewport = navigation.viewport(300, 100);

    EXPECT_EQ(0, viewport.start().ticks());
    EXPECT_EQ(50, viewport.end().ticks());
}

TEST(Navigation, clampsFirstLaneToVisibleRange)
{
    Navigation navigation(at(0), at(100), 5);

    navigation.scroll_to_lane(99, 2);
    const Viewport viewport = navigation.viewport(300, 100);

    EXPECT_EQ(3, viewport.first_lane());
}

TEST(Navigation, doesNotChangeDocumentData)
{
    const Document document = empty_lane_document();
    const StringId lane_id = document.lanes().front().id();
    Navigation navigation(at(0), at(100), 5);

    navigation.zoom_by(2.0, at(50));
    navigation.scroll_to(at(90));
    navigation.scroll_to_lane(3, 2);

    EXPECT_EQ(1, document.lane_count());
    EXPECT_EQ(lane_id, document.lanes().front().id());
}

TEST(Navigation, revealsPositionToTheRight)
{
    Navigation navigation(at(0), at(100), 5);
    navigation.zoom_by(2.0, at(50));

    navigation.reveal(at(80));
    const Viewport viewport = navigation.viewport(300, 100);

    EXPECT_EQ(31, viewport.start().ticks());
    EXPECT_EQ(81, viewport.end().ticks());
}

TEST(Navigation, keepsVisiblePositionInView)
{
    Navigation navigation(at(0), at(100), 5);
    navigation.zoom_by(2.0, at(50));
    navigation.reveal(at(80));

    navigation.reveal(at(40));
    const Viewport viewport = navigation.viewport(300, 100);

    EXPECT_EQ(31, viewport.start().ticks());
    EXPECT_EQ(81, viewport.end().ticks());
}

TEST(Navigation, revealsPositionToTheLeft)
{
    Navigation navigation(at(0), at(100), 5);
    navigation.zoom_by(2.0, at(50));
    navigation.reveal(at(80));

    navigation.reveal(at(10));
    const Viewport viewport = navigation.viewport(300, 100);

    EXPECT_EQ(10, viewport.start().ticks());
    EXPECT_EQ(60, viewport.end().ticks());
}

TEST(Navigation, clampsRevealBeforeContent)
{
    Navigation navigation(at(0), at(100), 5);
    navigation.zoom_by(2.0, at(50));

    navigation.reveal(at(-100));
    const Viewport viewport = navigation.viewport(300, 100);

    EXPECT_EQ(0, viewport.start().ticks());
    EXPECT_EQ(50, viewport.end().ticks());
}

TEST(Navigation, clampsRevealAfterContent)
{
    Navigation navigation(at(0), at(100), 5);
    navigation.zoom_by(2.0, at(50));

    navigation.reveal(at(200));
    const Viewport viewport = navigation.viewport(300, 100);

    EXPECT_EQ(50, viewport.start().ticks());
    EXPECT_EQ(100, viewport.end().ticks());
}

TEST(Navigation, revealPreservesZoomAndLane)
{
    Navigation navigation(at(0), at(100), 5);
    navigation.zoom_by(2.0, at(50));
    navigation.scroll_to_lane(3, 2);

    navigation.reveal(at(80));
    const Viewport viewport = navigation.viewport(300, 100);

    EXPECT_EQ(3, viewport.first_lane());
    EXPECT_DOUBLE_EQ(2.0, navigation.zoom_scale());
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

TEST_F(EmptyLaneLayoutTest, hitsRuler)
{
    const std::optional<HitResult> hit = m_layout.hit_test(Point{150, 10}, 3);

    ASSERT_TRUE(hit);
    EXPECT_EQ(StyleRole::RULER, hit->style);
    EXPECT_TRUE(hit->id.item_id.empty());
}

TEST_F(EmptyLaneLayoutTest, hitsLaneHeader)
{
    const std::optional<HitResult> hit = m_layout.hit_test(Point{10, 25}, 3);

    ASSERT_TRUE(hit);
    EXPECT_EQ(StyleRole::LANE_LABEL, hit->style);
    EXPECT_EQ(m_document.lanes().front().id(), hit->id.lane_id);
    EXPECT_TRUE(hit->id.item_id.empty());
}

TEST_F(EmptyLaneLayoutTest, hitsEmptyLaneBody)
{
    const std::optional<HitResult> hit = m_layout.hit_test(Point{150, 25}, 3);

    ASSERT_TRUE(hit);
    EXPECT_EQ(StyleRole::LANE_BACKGROUND, hit->style);
    EXPECT_EQ(m_document.lanes().front().id(), hit->id.lane_id);
}

TEST_F(EventLayoutTest, hitsInstantMarkerAndPreservesDisplayId)
{
    const Marker &marker = primitive_with_style<Marker>(m_layout, StyleRole::INSTANT_MARKER);

    const std::optional<HitResult> hit = m_layout.hit_test(Point{marker.x, marker.y}, 0);

    ASSERT_TRUE(hit);
    EXPECT_EQ(marker.style, hit->style);
    EXPECT_EQ(marker.id.lane_id, hit->id.lane_id);
    EXPECT_EQ(marker.id.item_id, hit->id.item_id);
}

TEST_F(EventLayoutTest, hitsIntervalAndPreservesDisplayId)
{
    const Rectangle &interval = primitive_with_style<Rectangle>(m_layout, StyleRole::INTERVAL_SPAN);

    const std::optional<HitResult> hit = m_layout.hit_test(Point{interval.x, interval.y}, 0);

    ASSERT_TRUE(hit);
    EXPECT_EQ(interval.style, hit->style);
    EXPECT_EQ(interval.id.lane_id, hit->id.lane_id);
    EXPECT_EQ(interval.id.item_id, hit->id.item_id);
}

TEST_F(EventLayoutTest, hitsEnvelopePhasesAndPreservesDisplayId)
{
    const std::vector<StyleRole> styles{
        StyleRole::ENVELOPE_ATTACK, StyleRole::ENVELOPE_SUSTAIN, StyleRole::ENVELOPE_DECAY};

    for (StyleRole style : styles)
    {
        const Rectangle &phase = primitive_with_style<Rectangle>(m_layout, style);
        const std::optional<HitResult> hit = m_layout.hit_test(Point{phase.x, phase.y}, 0);
        ASSERT_TRUE(hit);
        EXPECT_EQ(phase.style, hit->style);
        EXPECT_EQ(phase.id.lane_id, hit->id.lane_id);
        EXPECT_EQ(phase.id.item_id, hit->id.item_id);
    }
}

TEST_F(EventLayoutTest, usesHostToleranceForMarkerHits)
{
    const std::optional<HitResult> hit = m_layout.hit_test(Point{171, 30}, 3);

    ASSERT_TRUE(hit);
    EXPECT_EQ(*m_document.strings().find("beat-1"), hit->id.item_id);
}

TEST_F(EventLayoutTest, missesMarkerWithoutTolerance)
{
    const std::optional<HitResult> hit = m_layout.hit_test(Point{171, 30}, 0);

    ASSERT_TRUE(hit);
    EXPECT_EQ(StyleRole::LANE_BACKGROUND, hit->style);
}

TEST_F(EventLayoutTest, hitsLaneBackgroundBetweenItems)
{
    const std::optional<HitResult> hit = m_layout.hit_test(Point{280, 30}, 0);

    ASSERT_TRUE(hit);
    EXPECT_EQ(StyleRole::LANE_BACKGROUND, hit->style);
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

TEST(Layout, hitsCurveUsingHostTolerance)
{
    const Document document = hit_curve_document();
    const StringId signal_id = *document.strings().find("signal");
    const Layout layout(document, Viewport(400, 100, at(0), at(100)), LayoutMetrics(100, 20, 30, 4));

    const std::optional<HitResult> hit = layout.hit_test(Point{250, 36}, 3);

    ASSERT_TRUE(hit);
    EXPECT_EQ(StyleRole::CURVE, hit->style);
    EXPECT_EQ(document.lanes().front().id(), hit->id.lane_id);
    EXPECT_EQ(signal_id, hit->id.item_id);
}

TEST(Layout, missesCurveOutsideTolerance)
{
    const Document document = hit_curve_document();
    const Layout layout(document, Viewport(400, 100, at(0), at(100)), LayoutMetrics(100, 20, 30, 4));

    const std::optional<HitResult> hit = layout.hit_test(Point{250, 40}, 3);

    ASSERT_TRUE(hit);
    EXPECT_EQ(StyleRole::LANE_BACKGROUND, hit->style);
}

TEST(Layout, rejectsNegativeHitTolerance)
{
    const Document document = hit_curve_document();
    const Layout layout(document, Viewport(400, 100, at(0), at(100)), LayoutMetrics(100, 20, 30, 4));

    EXPECT_THROW(layout.hit_test(Point{250, 36}, -1), std::invalid_argument);
}

TEST(Layout, keyframeMarkerTakesPriorityOverSegment)
{
    DocumentBuilder builder(Document(100));
    Lane lane(builder.intern("zoom"), builder.intern("Zoom"), builder.intern("keyframes"), at(0), at(100));
    lane.add(Keyframe(builder.intern("start"), at(0), 0.0, KeyframeInterpolation::LINEAR, {}));
    const StringId end_id = builder.intern("end");
    lane.add(Keyframe(end_id, at(50), 1.0));
    builder.add_lane(std::move(lane));
    const Document document = std::move(builder).build();
    const Layout layout(document, Viewport(400, 100, at(0), at(100)), LayoutMetrics(100, 20, 30, 4));

    const std::optional<HitResult> hit = layout.hit_test(Point{250, 24}, 3);

    ASSERT_TRUE(hit);
    EXPECT_EQ(StyleRole::KEYFRAME_MARKER, hit->style);
    EXPECT_EQ(end_id, hit->id.item_id);
}

TEST(Layout, hitsKeyframeInterpolationSegment)
{
    DocumentBuilder builder(Document(100));
    Lane lane(builder.intern("zoom"), builder.intern("Zoom"), builder.intern("keyframes"), at(0), at(100));
    const StringId start_id = builder.intern("start");
    lane.add(Keyframe(start_id, at(0), 0.0, KeyframeInterpolation::LINEAR, {}));
    lane.add(Keyframe(builder.intern("end"), at(50), 1.0));
    builder.add_lane(std::move(lane));
    const Document document = std::move(builder).build();
    const Layout layout(document, Viewport(400, 100, at(0), at(100)), LayoutMetrics(100, 20, 30, 4));

    const std::optional<HitResult> hit = layout.hit_test(Point{175, 35}, 3);

    ASSERT_TRUE(hit);
    EXPECT_EQ(StyleRole::KEYFRAME_SEGMENT, hit->style);
    EXPECT_EQ(start_id, hit->id.item_id);
}

TEST_F(EmptyLaneLayoutTest, doesNotHitOutsideViewport)
{
    EXPECT_FALSE(m_layout.hit_test(Point{-1, 10}, 3));
    EXPECT_FALSE(m_layout.hit_test(Point{150, -1}, 3));
    EXPECT_FALSE(m_layout.hit_test(Point{400, 25}, 3));
    EXPECT_FALSE(m_layout.hit_test(Point{150, 100}, 3));
}

TEST_F(EmptyLaneLayoutTest, doesNotHitUnusedRows)
{
    EXPECT_FALSE(m_layout.hit_test(Point{150, 50}, 3));
}

TEST(Layout, hitsOnlyVisibleLaneAfterScrolling)
{
    DocumentBuilder builder(Document(100));
    builder.add_lane(
        Lane(builder.intern("hidden"), builder.intern("Hidden"), builder.intern("events"), at(0), at(100)));
    builder.add_lane(
        Lane(builder.intern("visible"), builder.intern("Visible"), builder.intern("events"), at(0), at(100)));
    const Document document = std::move(builder).build();
    const Layout layout(document, Viewport(400, 100, at(0), at(100), 1), LayoutMetrics(100, 20, 30, 4));

    const std::optional<HitResult> hit = layout.hit_test(Point{10, 25}, 3);

    ASSERT_TRUE(hit);
    EXPECT_EQ(document.lanes()[1].id(), hit->id.lane_id);
}
