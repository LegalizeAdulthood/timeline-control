// Copyright (c) 2026 Richard Thomson

#include <timeline/Keyframe.h>
#include <timeline/Lane.h>
#include <timeline/Layout.h>
#include <timeline/Query.h>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <stdexcept>

using namespace timeline;

namespace
{

Time at(Ticks ticks)
{
    return Time::from_ticks(ticks);
}

} // namespace

TEST(Keyframe, storesNumericValueAndInterpolation)
{
    const Keyframe keyframe(
        "zoom-1", at(10), 1.5, KeyframeInterpolation::LINEAR, {{"operation", "replace"}, {"source", "music.rms"}});

    EXPECT_EQ("zoom-1", keyframe.id());
    EXPECT_EQ(10, keyframe.time().ticks());
    EXPECT_DOUBLE_EQ(1.5, keyframe.value());
    EXPECT_EQ(KeyframeInterpolation::LINEAR, keyframe.interpolation());
    EXPECT_EQ("replace", keyframe.attributes().at("operation"));
}

TEST(Lane, ownsOptionalKeyframeEvaluationAndRejectsInvalidRecipes)
{
    Lane empty("empty", "Empty", "keyframes", at(0), at(60));
    EXPECT_THROW(empty.set_keyframe_evaluator([](Time) { return std::optional<double>(1); }), std::invalid_argument);
    Lane lane("recipe", "Recipe", "keyframes", at(0), at(60));
    lane.add(Keyframe("key-10", at(10), 2));
    lane.add(Keyframe("key-40", at(40), 4));
    EXPECT_FALSE(lane.has_keyframe_evaluator());
    const Lane authored = lane;
    lane.set_keyframe_evaluator(
        [authored](Time time) -> std::optional<double>
        {
            if (at(20) <= time && time < at(40))
            {
                return std::nullopt;
            }
            return authored.evaluate_keyframes(time);
        });
    EXPECT_TRUE(lane.has_keyframe_evaluator());
    EXPECT_EQ(2, lane.item_count());
    EXPECT_FALSE(lane.evaluate_keyframes(at(30)));
    const Lane copy = lane;
    EXPECT_DOUBLE_EQ(2, *copy.evaluate_keyframes(at(0)));
    EXPECT_FALSE(copy.evaluate_keyframes(at(30)));
    EXPECT_THROW(lane.set_keyframe_evaluator(KeyframeEvaluator{}), std::invalid_argument);
    EXPECT_FALSE(lane.evaluate_keyframes(at(30)));
    lane.set_keyframe_evaluator([](Time) { return std::optional<double>(std::numeric_limits<double>::infinity()); });
    EXPECT_THROW(lane.evaluate_keyframes(at(0)), std::invalid_argument);
}

TEST(Lane, ownsOutputRulesWithoutChangingAuthoredSamples)
{
    Lane empty("empty", "Empty", "keyframes", at(0), at(60));
    EXPECT_FALSE(empty.evaluate_keyframe_output(at(0)));
    EXPECT_THROW(empty.set_keyframe_output_evaluator([](Time, double value) { return value; }), std::invalid_argument);
    Lane lane("output", "Output", "keyframes", at(0), at(60));
    lane.add(Keyframe("first", at(0), -3, KeyframeInterpolation::LINEAR, {}));
    lane.add(Keyframe("last", at(40), 0));
    EXPECT_FALSE(lane.evaluate_keyframe_output(at(20)));
    const double scale = 2;
    lane.set_keyframe_output_evaluator([scale](Time, double value) { return std::round(value * scale) / scale; });
    EXPECT_DOUBLE_EQ(-1.5, *lane.evaluate_keyframes(at(20)));
    EXPECT_DOUBLE_EQ(-1.5, *lane.evaluate_keyframe_output(at(20)));
    EXPECT_THROW(lane.set_keyframe_output_evaluator(KeyframeOutputEvaluator{}), std::invalid_argument);
    EXPECT_DOUBLE_EQ(-1.5, *lane.evaluate_keyframe_output(at(20)));
    const Lane copy = lane;
    lane.set_keyframe_output_evaluator([](Time, double) { return std::numeric_limits<double>::infinity(); });
    EXPECT_THROW(lane.evaluate_keyframe_output(at(20)), std::invalid_argument);
    EXPECT_DOUBLE_EQ(-1.5, *copy.evaluate_keyframe_output(at(20)));
    lane.set_keyframe_evaluator([](Time) { return std::optional<double>{}; });
    EXPECT_FALSE(lane.evaluate_keyframe_output(at(20)));
    EXPECT_EQ(2, lane.item_count());
    Document document(FrameGrid(Timebase(60), 6, 6, 1), 1, 2);
    document.add_lane(copy);
    const FrameInspection inspection = *inspect_frame(document, 1);
    EXPECT_DOUBLE_EQ(-2.25, *inspection.lanes[0].value);
    EXPECT_DOUBLE_EQ(-2.5, *inspection.lanes[0].output_value);
    const RangeInspection range = inspect_range(document, at(0), at(40));
    EXPECT_FALSE(range.lanes[0].output_value);
    EXPECT_DOUBLE_EQ(-3, *range.lanes[0].items[0].value);
}

TEST(Layout, samplesOwnedKeyframeRecipesWithoutBridgingGaps)
{
    Lane lane("recipe", "Recipe", "keyframes", at(0), at(60));
    lane.add(Keyframe("key-10", at(10), 2));
    lane.add(Keyframe("key-40", at(40), 4));
    lane.set_keyframe_evaluator(
        [](Time time) -> std::optional<double>
        {
            if (at(20) <= time && time < at(40))
            {
                return std::nullopt;
            }
            return time < at(20) ? 20 : 4;
        });
    for (bool framed : {false, true})
    {
        Document document = framed ? Document(FrameGrid(Timebase(60), 6, 6, 1), 1, 2, Metadata("Recipe", ""))
                                   : Document(Timebase(60), Metadata("Recipe", ""));
        document.add_lane(lane);
        const Layout layout(document, Viewport(600, 100, at(0), at(60)), LayoutMetrics(100, 20, 40, 4));
        int segments = 0;
        for (const Primitive &primitive : layout.display_list().primitives())
        {
            if (std::holds_alternative<Polyline>(primitive))
            {
                const Polyline &line = std::get<Polyline>(primitive);
                ++segments;
                EXPECT_TRUE(line.points.back().x < 300 || line.points.front().x >= 433);
                for (const Point &point : line.points)
                {
                    EXPECT_GE(point.y, 24);
                    EXPECT_LT(point.y, 56);
                }
                const std::optional<HitResult> hit = layout.hit_test(line.points.back(), 0);
                ASSERT_TRUE(hit);
                EXPECT_EQ("recipe", hit->id.lane_id);
                EXPECT_EQ("key-10", hit->id.item_id);
            }
        }
        EXPECT_EQ(2, segments);
        if (framed)
        {
            EXPECT_FALSE(inspect_frame(document, 2)->lanes[0].value);
            EXPECT_DOUBLE_EQ(20, *inspect_frame(document, 0)->lanes[0].value);
        }
    }
}

TEST(Keyframe, rejectsInvalidIdentityAndValue)
{
    EXPECT_THROW(Keyframe("", at(0), 1.0), std::invalid_argument);
    EXPECT_THROW(Keyframe("bad", at(0), std::numeric_limits<double>::infinity()), std::invalid_argument);
}

TEST(Lane, findsNeighboringKeyframes)
{
    Lane lane("zoom", "camera.zoom", "keyframes", at(0), at(40));
    lane.add(Keyframe("zoom-20", at(20), 2.0));
    lane.add(Keyframe("zoom-0", at(0), 0.0));
    lane.add(Keyframe("zoom-30", at(30), 3.0));

    const KeyframeNeighbors neighbors = lane.neighboring_keyframes(at(10));

    ASSERT_TRUE(neighbors.before().has_value());
    ASSERT_TRUE(neighbors.after().has_value());
    EXPECT_EQ("zoom-0", neighbors.before()->get().id());
    EXPECT_EQ("zoom-20", neighbors.after()->get().id());
}

TEST(Lane, evaluatesHoldAndLinearKeyframes)
{
    Lane hold("hold", "Hold", "keyframes", at(0), at(30));
    hold.add(Keyframe("hold-0", at(0), 2.0, KeyframeInterpolation::HOLD, {}));
    hold.add(Keyframe("hold-20", at(20), 6.0));

    ASSERT_TRUE(hold.evaluate_keyframes(at(10)).has_value());
    EXPECT_DOUBLE_EQ(2.0, *hold.evaluate_keyframes(at(10)));
    EXPECT_DOUBLE_EQ(6.0, *hold.evaluate_keyframes(at(25)));

    Lane linear("linear", "Linear", "keyframes", at(0), at(30));
    linear.add(Keyframe("linear-0", at(0), 2.0, KeyframeInterpolation::LINEAR, {}));
    linear.add(Keyframe("linear-20", at(20), 6.0));

    ASSERT_TRUE(linear.evaluate_keyframes(at(10)).has_value());
    EXPECT_DOUBLE_EQ(4.0, *linear.evaluate_keyframes(at(10)));
    EXPECT_DOUBLE_EQ(2.0, *linear.evaluate_keyframes(at(-10)));
}

TEST(Lane, rejectsDuplicateKeyframeTimes)
{
    Lane lane("zoom", "camera.zoom", "keyframes", at(0), at(30));
    lane.add(Keyframe("zoom-0", at(0), 1.0));

    EXPECT_THROW(lane.add(Keyframe("zoom-again", at(0), 2.0)), std::invalid_argument);
}

TEST(Lane, evaluatesGeometricSegmentsAndRejectsNonpositiveEndpoints)
{
    Lane lane("zoom", "Zoom", "keyframes", at(0), at(30));
    lane.add(Keyframe("zoom-0", at(0), 1.0, KeyframeInterpolation::GEOMETRIC, {}));
    lane.add(Keyframe("zoom-20", at(20), 9.0));
    EXPECT_NEAR(3.0, *lane.evaluate_keyframes(at(10)), 1e-12);
    EXPECT_DOUBLE_EQ(1.0, *lane.evaluate_keyframes(at(-1)));
    EXPECT_DOUBLE_EQ(9.0, *lane.evaluate_keyframes(at(25)));
    EXPECT_THROW(Keyframe("bad", at(0), 0.0, KeyframeInterpolation::GEOMETRIC, {}), std::invalid_argument);
    Lane invalid("bad", "Bad", "keyframes", at(0), at(30));
    invalid.add(Keyframe("positive", at(0), 1.0, KeyframeInterpolation::GEOMETRIC, {}));
    EXPECT_THROW(invalid.add(Keyframe("zero", at(20), 0.0)), std::invalid_argument);
}
