// Copyright (c) 2026 Richard Thomson

#include <timeline/Keyframe.h>
#include <timeline/Lane.h>
#include <timeline/Layout.h>
#include <timeline/Query.h>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace timeline;

namespace
{

constexpr StringId LANE_LABEL{10};
constexpr StringId LANE_KIND{11};

Time at(Ticks ticks)
{
    return Time::from_ticks(ticks);
}

} // namespace

TEST(KeyframeInterpolation, convertsEveryValueToString)
{
    EXPECT_EQ("hold", to_string(KeyframeInterpolation::HOLD));
    EXPECT_EQ("linear", to_string(KeyframeInterpolation::LINEAR));
    EXPECT_EQ("geometric", to_string(KeyframeInterpolation::GEOMETRIC));
}

TEST(Keyframe, storesNumericValueAndInterpolation)
{
    const Keyframe keyframe(StringId{1}, at(10), 1.5, KeyframeInterpolation::LINEAR,
        Attributes(std::vector<Attribute>{{StringId{2}, StringId{3}}, {StringId{4}, StringId{5}}}));

    EXPECT_EQ(StringId{1}, keyframe.id());
    EXPECT_EQ(10, keyframe.time().ticks());
    EXPECT_DOUBLE_EQ(1.5, keyframe.value());
    EXPECT_EQ(KeyframeInterpolation::LINEAR, keyframe.interpolation());
    EXPECT_EQ(StringId{3}, keyframe.attributes().find(StringId{2}));
}

TEST(Lane, ownsOptionalKeyframeEvaluationAndRejectsInvalidRecipes)
{
    Lane empty(StringId{1}, LANE_LABEL, LANE_KIND, at(0), at(60));
    EXPECT_THROW(empty.set_keyframe_evaluator([](Time) { return std::optional<double>(1); }), std::invalid_argument);
    Lane lane(StringId{1}, LANE_LABEL, LANE_KIND, at(0), at(60));
    lane.add(Keyframe(StringId{2}, at(10), 2));
    lane.add(Keyframe(StringId{3}, at(40), 4));
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
    Lane empty(StringId{1}, LANE_LABEL, LANE_KIND, at(0), at(60));
    EXPECT_FALSE(empty.evaluate_keyframe_output(at(0)));
    EXPECT_THROW(empty.set_keyframe_output_evaluator([](Time, double value) { return value; }), std::invalid_argument);
    StringTableBuilder strings;
    const StringId output_id = strings.intern("output");
    const StringId output_label = strings.intern("Output");
    const StringId keyframe_kind = strings.intern("keyframes");
    Lane lane(output_id, output_label, keyframe_kind, at(0), at(60));
    lane.add(Keyframe(strings.intern("first"), at(0), -3, KeyframeInterpolation::LINEAR, {}));
    lane.add(Keyframe(strings.intern("last"), at(40), 0));
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
    DocumentBuilder builder(Document(FrameGrid(Timebase(60), 6, 6, 1), 1, 2), std::move(strings).build());
    builder.add_lane(copy);
    const Document document = std::move(builder).build();
    const FrameInspection inspection = *inspect_frame(document, 1);
    EXPECT_DOUBLE_EQ(-2.25, *inspection.lanes[0].value);
    EXPECT_DOUBLE_EQ(-2.5, *inspection.lanes[0].output_value);
    const RangeInspection range = inspect_range(document, at(0), at(40));
    EXPECT_FALSE(range.lanes[0].output_value);
    EXPECT_DOUBLE_EQ(-3, *range.lanes[0].items[0].value);
}

TEST(Layout, samplesOwnedKeyframeRecipesWithoutBridgingGaps)
{
    StringTableBuilder string_builder;
    const StringId recipe_id = string_builder.intern("recipe");
    const StringId key_10_id = string_builder.intern("key-10");
    const StringId key_40_id = string_builder.intern("key-40");
    const StringId recipe_label = string_builder.intern("Recipe");
    const StringId keyframe_kind = string_builder.intern("keyframes");
    const StringTable strings = std::move(string_builder).build();
    Lane lane(recipe_id, recipe_label, keyframe_kind, at(0), at(60));
    lane.add(Keyframe(key_10_id, at(10), 2));
    lane.add(Keyframe(key_40_id, at(40), 4));
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
        DocumentBuilder builder(std::move(document), strings);
        builder.add_lane(lane);
        document = std::move(builder).build();
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
                EXPECT_EQ(recipe_id, hit->id.lane_id);
                EXPECT_EQ(key_10_id, hit->id.item_id);
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
    EXPECT_THROW(Keyframe(StringId{}, at(0), 1.0), std::invalid_argument);
    EXPECT_THROW(Keyframe(StringId{1}, at(0), std::numeric_limits<double>::infinity()), std::invalid_argument);
}

TEST(Lane, findsNeighboringKeyframes)
{
    Lane lane(StringId{1}, LANE_LABEL, LANE_KIND, at(0), at(40));
    lane.add(Keyframe(StringId{2}, at(20), 2.0));
    lane.add(Keyframe(StringId{3}, at(0), 0.0));
    lane.add(Keyframe(StringId{4}, at(30), 3.0));

    const KeyframeNeighbors neighbors = lane.neighboring_keyframes(at(10));

    ASSERT_TRUE(neighbors.before().has_value());
    ASSERT_TRUE(neighbors.after().has_value());
    EXPECT_EQ(StringId{3}, neighbors.before()->get().id());
    EXPECT_EQ(StringId{2}, neighbors.after()->get().id());
}

TEST(Lane, evaluatesHoldAndLinearKeyframes)
{
    Lane hold(StringId{1}, LANE_LABEL, LANE_KIND, at(0), at(30));
    hold.add(Keyframe(StringId{2}, at(0), 2.0, KeyframeInterpolation::HOLD, {}));
    hold.add(Keyframe(StringId{3}, at(20), 6.0));

    ASSERT_TRUE(hold.evaluate_keyframes(at(10)).has_value());
    EXPECT_DOUBLE_EQ(2.0, *hold.evaluate_keyframes(at(10)));
    EXPECT_DOUBLE_EQ(6.0, *hold.evaluate_keyframes(at(25)));

    Lane linear(StringId{1}, LANE_LABEL, LANE_KIND, at(0), at(30));
    linear.add(Keyframe(StringId{2}, at(0), 2.0, KeyframeInterpolation::LINEAR, {}));
    linear.add(Keyframe(StringId{3}, at(20), 6.0));

    ASSERT_TRUE(linear.evaluate_keyframes(at(10)).has_value());
    EXPECT_DOUBLE_EQ(4.0, *linear.evaluate_keyframes(at(10)));
    EXPECT_DOUBLE_EQ(2.0, *linear.evaluate_keyframes(at(-10)));
}

TEST(Lane, rejectsDuplicateKeyframeTimes)
{
    Lane lane(StringId{1}, LANE_LABEL, LANE_KIND, at(0), at(30));
    lane.add(Keyframe(StringId{2}, at(0), 1.0));

    EXPECT_THROW(lane.add(Keyframe(StringId{3}, at(0), 2.0)), std::invalid_argument);
}

TEST(Lane, evaluatesGeometricSegmentsAndRejectsNonpositiveEndpoints)
{
    Lane lane(StringId{1}, LANE_LABEL, LANE_KIND, at(0), at(30));
    lane.add(Keyframe(StringId{2}, at(0), 1.0, KeyframeInterpolation::GEOMETRIC, {}));
    lane.add(Keyframe(StringId{3}, at(20), 9.0));
    EXPECT_NEAR(3.0, *lane.evaluate_keyframes(at(10)), 1e-12);
    EXPECT_DOUBLE_EQ(1.0, *lane.evaluate_keyframes(at(-1)));
    EXPECT_DOUBLE_EQ(9.0, *lane.evaluate_keyframes(at(25)));
    EXPECT_THROW(Keyframe(StringId{2}, at(0), 0.0, KeyframeInterpolation::GEOMETRIC, {}), std::invalid_argument);
    Lane invalid(StringId{1}, LANE_LABEL, LANE_KIND, at(0), at(30));
    invalid.add(Keyframe(StringId{2}, at(0), 1.0, KeyframeInterpolation::GEOMETRIC, {}));
    EXPECT_THROW(invalid.add(Keyframe(StringId{3}, at(20), 0.0)), std::invalid_argument);
}
