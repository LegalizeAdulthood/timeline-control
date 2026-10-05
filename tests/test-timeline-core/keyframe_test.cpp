// Copyright (c) 2026 Richard Thomson

#include <timeline/Keyframe.h>
#include <timeline/Lane.h>
#include <timeline/Layout.h>
#include <timeline/Query.h>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
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

Lane keyframe_lane()
{
    Lane lane(StringId{1}, LANE_LABEL, LANE_KIND, at(0), at(60));
    lane.add(Keyframe(StringId{2}, at(10), 2));
    lane.add(Keyframe(StringId{3}, at(40), 4));
    return lane;
}

Lane gapped_lane()
{
    Lane lane = keyframe_lane();
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
    return lane;
}

Lane linear_lane()
{
    Lane lane(StringId{1}, LANE_LABEL, LANE_KIND, at(0), at(60));
    lane.add(Keyframe(StringId{2}, at(0), -3, KeyframeInterpolation::LINEAR, {}));
    lane.add(Keyframe(StringId{3}, at(40), 0));
    return lane;
}

Lane output_lane()
{
    Lane lane = linear_lane();
    const double scale = 2;
    lane.set_keyframe_output_evaluator([scale](Time, double value) { return std::round(value * scale) / scale; });
    return lane;
}

Document output_document()
{
    StringTableBuilder strings;
    const StringId output_id = strings.intern("output");
    const StringId output_label = strings.intern("Output");
    const StringId keyframe_kind = strings.intern("keyframes");
    Lane lane(output_id, output_label, keyframe_kind, at(0), at(60));
    lane.add(Keyframe(strings.intern("first"), at(0), -3, KeyframeInterpolation::LINEAR, {}));
    lane.add(Keyframe(strings.intern("last"), at(40), 0));
    const double scale = 2;
    lane.set_keyframe_output_evaluator([scale](Time, double value) { return std::round(value * scale) / scale; });
    DocumentBuilder builder(Document(FrameGrid(Timebase(60), 6, 6, 1), 1, 2), std::move(strings).build());
    builder.add_lane(std::move(lane));
    return std::move(builder).build();
}

Document recipe_document(bool framed)
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
    Document document = framed ? Document(FrameGrid(Timebase(60), 6, 6, 1), 1, 2, Metadata(recipe_label, {}))
                               : Document(Timebase(60), Metadata(recipe_label, {}));
    DocumentBuilder builder(std::move(document), strings);
    builder.add_lane(std::move(lane));
    return std::move(builder).build();
}

std::vector<Polyline> keyframe_lines(const Layout &layout)
{
    std::vector<Polyline> result;
    for (const Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<Polyline>(primitive))
        {
            result.push_back(std::get<Polyline>(primitive));
        }
    }
    return result;
}

const Polyline &first_keyframe_line(const Layout &layout)
{
    for (const Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<Polyline>(primitive))
        {
            return std::get<Polyline>(primitive);
        }
    }
    throw std::logic_error("layout has no keyframe line");
}

Lane interpolation_lane(KeyframeInterpolation interpolation)
{
    Lane lane(StringId{1}, LANE_LABEL, LANE_KIND, at(0), at(30));
    lane.add(Keyframe(StringId{2}, at(0), 2.0, interpolation, {}));
    lane.add(Keyframe(StringId{3}, at(20), 6.0));
    return lane;
}

using KeyframeInterpolationName = std::pair<KeyframeInterpolation, std::string_view>;

class KeyframeInterpolationNameTest : public testing::TestWithParam<KeyframeInterpolationName>
{
};

std::string keyframe_interpolation_parameter_name(const testing::TestParamInfo<KeyframeInterpolationName> &info)
{
    switch (info.param.first)
    {
    case KeyframeInterpolation::HOLD:
        return "Hold";
    case KeyframeInterpolation::LINEAR:
        return "Linear";
    case KeyframeInterpolation::GEOMETRIC:
        return "Geometric";
    }
    throw std::logic_error("unknown keyframe interpolation policy");
}

} // namespace

TEST_P(KeyframeInterpolationNameTest, convertsToStableName)
{
    const auto &[value, expected] = GetParam();

    EXPECT_EQ(expected, to_string(value));
}

INSTANTIATE_TEST_SUITE_P(KeyframeInterpolationNames, KeyframeInterpolationNameTest,
    testing::Values(KeyframeInterpolationName{KeyframeInterpolation::HOLD, "hold"},
        KeyframeInterpolationName{KeyframeInterpolation::LINEAR, "linear"},
        KeyframeInterpolationName{KeyframeInterpolation::GEOMETRIC, "geometric"}),
    keyframe_interpolation_parameter_name);

TEST(Keyframe, storesIdentity)
{
    const Keyframe keyframe(StringId{1}, at(10), 1.5);

    EXPECT_EQ(StringId{1}, keyframe.id());
}

TEST(Keyframe, storesTime)
{
    const Keyframe keyframe(StringId{1}, at(10), 1.5);

    EXPECT_EQ(at(10), keyframe.time());
}

TEST(Keyframe, storesNumericValue)
{
    const Keyframe keyframe(StringId{1}, at(10), 1.5);

    EXPECT_DOUBLE_EQ(1.5, keyframe.value());
}

TEST(Keyframe, storesInterpolation)
{
    const Keyframe keyframe(StringId{1}, at(10), 1.5, KeyframeInterpolation::LINEAR, {});

    EXPECT_EQ(KeyframeInterpolation::LINEAR, keyframe.interpolation());
}

TEST(Keyframe, storesAttributes)
{
    const Keyframe keyframe(StringId{1}, at(10), 1.5, KeyframeInterpolation::LINEAR,
        Attributes(std::vector<Attribute>{{StringId{2}, StringId{3}}, {StringId{4}, StringId{5}}}));

    EXPECT_EQ(StringId{3}, keyframe.attributes().find(StringId{2}));
}

TEST(Lane, startsWithoutKeyframeEvaluator)
{
    const Lane lane = keyframe_lane();

    EXPECT_FALSE(lane.has_keyframe_evaluator());
}

TEST(Lane, rejectsKeyframeEvaluatorWithoutAuthoredKeys)
{
    Lane lane(StringId{1}, LANE_LABEL, LANE_KIND, at(0), at(60));

    EXPECT_THROW(lane.set_keyframe_evaluator([](Time) { return std::optional<double>(1); }), std::invalid_argument);
}

TEST(Lane, ownsKeyframeEvaluator)
{
    Lane lane = keyframe_lane();
    const double scale = 3;

    lane.set_keyframe_evaluator([scale](Time) { return std::optional<double>(scale); });
    const std::optional<double> value = lane.evaluate_keyframes(at(20));

    ASSERT_TRUE(value);
    EXPECT_TRUE(lane.has_keyframe_evaluator());
    EXPECT_DOUBLE_EQ(3, *value);
}

TEST(Lane, preservesAuthoredKeysWhenSettingEvaluator)
{
    Lane lane = keyframe_lane();

    lane.set_keyframe_evaluator([](Time) { return std::optional<double>(1); });

    EXPECT_EQ(2, lane.item_count());
}

TEST(Lane, representsKeyframeRecipeGaps)
{
    const Lane lane = gapped_lane();

    const std::optional<double> value = lane.evaluate_keyframes(at(30));

    EXPECT_FALSE(value);
}

TEST(Lane, copiesOwnedKeyframeEvaluator)
{
    const Lane lane = gapped_lane();

    const Lane copy = lane;
    const std::optional<double> value = copy.evaluate_keyframes(at(0));

    ASSERT_TRUE(value);
    EXPECT_DOUBLE_EQ(2, *value);
}

TEST(Lane, rejectsEmptyKeyframeEvaluatorWithoutReplacingRecipe)
{
    Lane lane = gapped_lane();

    EXPECT_THROW(lane.set_keyframe_evaluator(KeyframeEvaluator{}), std::invalid_argument);

    EXPECT_FALSE(lane.evaluate_keyframes(at(30)));
}

TEST(Lane, rejectsNonfiniteKeyframeEvaluatorResult)
{
    Lane lane = keyframe_lane();
    lane.set_keyframe_evaluator([](Time) { return std::optional<double>(std::numeric_limits<double>::infinity()); });

    EXPECT_THROW(lane.evaluate_keyframes(at(0)), std::invalid_argument);
}

TEST(Lane, hasNoKeyframeOutputWithoutRule)
{
    const Lane lane = linear_lane();

    EXPECT_FALSE(lane.evaluate_keyframe_output(at(10)));
}

TEST(Lane, emptyLaneHasNoKeyframeOutput)
{
    const Lane lane(StringId{1}, LANE_LABEL, LANE_KIND, at(0), at(60));

    EXPECT_FALSE(lane.evaluate_keyframe_output(at(0)));
}

TEST(Lane, rejectsKeyframeOutputRuleWithoutAuthoredKeys)
{
    Lane lane(StringId{1}, LANE_LABEL, LANE_KIND, at(0), at(60));

    EXPECT_THROW(lane.set_keyframe_output_evaluator([](Time, double value) { return value; }), std::invalid_argument);
}

TEST(Lane, appliesKeyframeOutputRule)
{
    const Lane lane = output_lane();

    const std::optional<double> value = lane.evaluate_keyframe_output(at(10));

    ASSERT_TRUE(value);
    EXPECT_DOUBLE_EQ(-2.5, *value);
}

TEST(Lane, outputRuleDoesNotChangeAuthoredEvaluation)
{
    const Lane lane = output_lane();

    const std::optional<double> value = lane.evaluate_keyframes(at(10));

    ASSERT_TRUE(value);
    EXPECT_DOUBLE_EQ(-2.25, *value);
}

TEST(Lane, outputRuleDoesNotChangeAuthoredItems)
{
    const Lane lane = output_lane();

    EXPECT_EQ(2, lane.item_count());
}

TEST(Lane, rejectsEmptyOutputRuleWithoutReplacingRule)
{
    Lane lane = output_lane();

    EXPECT_THROW(lane.set_keyframe_output_evaluator(KeyframeOutputEvaluator{}), std::invalid_argument);
    const std::optional<double> value = lane.evaluate_keyframe_output(at(10));

    ASSERT_TRUE(value);
    EXPECT_DOUBLE_EQ(-2.5, *value);
}

TEST(Lane, rejectsNonfiniteKeyframeOutput)
{
    Lane lane = output_lane();
    lane.set_keyframe_output_evaluator([](Time, double) { return std::numeric_limits<double>::infinity(); });

    EXPECT_THROW(lane.evaluate_keyframe_output(at(10)), std::invalid_argument);
}

TEST(Lane, copiesOwnedKeyframeOutputRule)
{
    Lane lane = output_lane();
    const Lane copy = lane;
    lane.set_keyframe_output_evaluator([](Time, double) { return 1; });

    const std::optional<double> value = copy.evaluate_keyframe_output(at(10));

    ASSERT_TRUE(value);
    EXPECT_DOUBLE_EQ(-2.5, *value);
}

TEST(Lane, omitsOutputForKeyframeRecipeGap)
{
    Lane lane = output_lane();
    lane.set_keyframe_evaluator([](Time) { return std::optional<double>{}; });

    const std::optional<double> value = lane.evaluate_keyframe_output(at(10));

    EXPECT_FALSE(value);
}

TEST(FrameInspection, reportsAuthoredKeyframeValue)
{
    const Document document = output_document();

    const FrameInspection inspection = *inspect_frame(document, 1);

    ASSERT_EQ(1, size_cast(inspection.lanes));
    ASSERT_TRUE(inspection.lanes[0].value);
    EXPECT_DOUBLE_EQ(-2.25, *inspection.lanes[0].value);
}

TEST(FrameInspection, reportsKeyframeOutputValue)
{
    const Document document = output_document();

    const FrameInspection inspection = *inspect_frame(document, 1);

    ASSERT_EQ(1, size_cast(inspection.lanes));
    ASSERT_TRUE(inspection.lanes[0].output_value);
    EXPECT_DOUBLE_EQ(-2.5, *inspection.lanes[0].output_value);
}

TEST(RangeInspection, doesNotAggregateKeyframeOutput)
{
    const Document document = output_document();

    const RangeInspection inspection = inspect_range(document, at(0), at(40));

    ASSERT_EQ(1, size_cast(inspection.lanes));
    EXPECT_FALSE(inspection.lanes[0].output_value);
}

TEST(RangeInspection, preservesAuthoredKeyframeValue)
{
    const Document document = output_document();

    const RangeInspection inspection = inspect_range(document, at(0), at(40));

    ASSERT_EQ(1, size_cast(inspection.lanes));
    ASSERT_FALSE(inspection.lanes[0].items.empty());
    ASSERT_TRUE(inspection.lanes[0].items[0].value);
    EXPECT_DOUBLE_EQ(-3, *inspection.lanes[0].items[0].value);
}

TEST(Layout, framelessKeyframeRecipeDoesNotBridgeGaps)
{
    const Document document = recipe_document(false);

    const Layout layout(document, Viewport(600, 100, at(0), at(60)), LayoutMetrics(100, 20, 40, 4));
    const std::vector<Polyline> lines = keyframe_lines(layout);

    ASSERT_EQ(2, size_cast(lines));
    EXPECT_LT(lines[0].points.back().x, 300);
    EXPECT_GE(lines[1].points.front().x, 433);
}

TEST(Layout, framedKeyframeRecipeDoesNotBridgeGaps)
{
    const Document document = recipe_document(true);

    const Layout layout(document, Viewport(600, 100, at(0), at(60)), LayoutMetrics(100, 20, 40, 4));
    const std::vector<Polyline> lines = keyframe_lines(layout);

    ASSERT_EQ(2, size_cast(lines));
    EXPECT_LT(lines[0].points.back().x, 300);
    EXPECT_GE(lines[1].points.front().x, 433);
}

TEST(Layout, keepsKeyframeRecipeSamplesWithinLane)
{
    const Document document = recipe_document(false);
    const Layout layout(document, Viewport(600, 100, at(0), at(60)), LayoutMetrics(100, 20, 40, 4));

    const std::vector<Polyline> lines = keyframe_lines(layout);

    ASSERT_FALSE(lines.empty());
    for (const Polyline &line : lines)
    {
        for (const Point &point : line.points)
        {
            EXPECT_GE(point.y, 24);
            EXPECT_LT(point.y, 56);
        }
    }
}

TEST(Layout, preservesKeyframeRecipeHitIdentity)
{
    const Document document = recipe_document(false);
    const Layout layout(document, Viewport(600, 100, at(0), at(60)), LayoutMetrics(100, 20, 40, 4));
    const Polyline &line = first_keyframe_line(layout);

    const std::optional<HitResult> hit = layout.hit_test(line.points.back(), 0);

    ASSERT_TRUE(hit);
    const Lane &lane = document.lanes().front();
    EXPECT_EQ(lane.id(), hit->id.lane_id);
    EXPECT_EQ(std::get<Keyframe>(lane.items().front()).id(), hit->id.item_id);
}

TEST(FrameInspection, reportsKeyframeRecipeGap)
{
    const Document document = recipe_document(true);

    const std::optional<FrameInspection> inspection = inspect_frame(document, 2);

    ASSERT_TRUE(inspection);
    ASSERT_EQ(1, size_cast(inspection->lanes));
    EXPECT_FALSE(inspection->lanes[0].value);
}

TEST(FrameInspection, reportsKeyframeRecipeValue)
{
    const Document document = recipe_document(true);

    const std::optional<FrameInspection> inspection = inspect_frame(document, 0);

    ASSERT_TRUE(inspection);
    ASSERT_EQ(1, size_cast(inspection->lanes));
    ASSERT_TRUE(inspection->lanes[0].value);
    EXPECT_DOUBLE_EQ(20, *inspection->lanes[0].value);
}

TEST(Keyframe, rejectsEmptyIdentity)
{
    EXPECT_THROW(Keyframe(StringId{}, at(0), 1.0), std::invalid_argument);
}

TEST(Keyframe, rejectsNonfiniteValue)
{
    EXPECT_THROW(Keyframe(StringId{1}, at(0), std::numeric_limits<double>::infinity()), std::invalid_argument);
}

TEST(Lane, findsKeyframeBeforeTime)
{
    Lane lane(StringId{1}, LANE_LABEL, LANE_KIND, at(0), at(40));
    lane.add(Keyframe(StringId{2}, at(20), 2.0));
    lane.add(Keyframe(StringId{3}, at(0), 0.0));
    lane.add(Keyframe(StringId{4}, at(30), 3.0));

    const KeyframeNeighbors neighbors = lane.neighboring_keyframes(at(10));

    ASSERT_TRUE(neighbors.before());
    EXPECT_EQ(StringId{3}, neighbors.before()->get().id());
}

TEST(Lane, findsKeyframeAfterTime)
{
    Lane lane(StringId{1}, LANE_LABEL, LANE_KIND, at(0), at(40));
    lane.add(Keyframe(StringId{2}, at(20), 2.0));
    lane.add(Keyframe(StringId{3}, at(0), 0.0));
    lane.add(Keyframe(StringId{4}, at(30), 3.0));

    const KeyframeNeighbors neighbors = lane.neighboring_keyframes(at(10));

    ASSERT_TRUE(neighbors.after());
    EXPECT_EQ(StringId{2}, neighbors.after()->get().id());
}

TEST(Lane, holdsValueBetweenKeyframes)
{
    const Lane lane = interpolation_lane(KeyframeInterpolation::HOLD);

    const std::optional<double> value = lane.evaluate_keyframes(at(10));

    ASSERT_TRUE(value);
    EXPECT_DOUBLE_EQ(2.0, *value);
}

TEST(Lane, linearlyInterpolatesBetweenKeyframes)
{
    const Lane lane = interpolation_lane(KeyframeInterpolation::LINEAR);

    const std::optional<double> value = lane.evaluate_keyframes(at(10));

    ASSERT_TRUE(value);
    EXPECT_DOUBLE_EQ(4.0, *value);
}

TEST(Lane, usesFirstValueBeforeKeyframes)
{
    const Lane lane = interpolation_lane(KeyframeInterpolation::LINEAR);

    const std::optional<double> value = lane.evaluate_keyframes(at(-10));

    ASSERT_TRUE(value);
    EXPECT_DOUBLE_EQ(2.0, *value);
}

TEST(Lane, usesLastValueAfterKeyframes)
{
    const Lane lane = interpolation_lane(KeyframeInterpolation::HOLD);

    const std::optional<double> value = lane.evaluate_keyframes(at(25));

    ASSERT_TRUE(value);
    EXPECT_DOUBLE_EQ(6.0, *value);
}

TEST(Lane, rejectsDuplicateKeyframeTimes)
{
    Lane lane(StringId{1}, LANE_LABEL, LANE_KIND, at(0), at(30));
    lane.add(Keyframe(StringId{2}, at(0), 1.0));

    EXPECT_THROW(lane.add(Keyframe(StringId{3}, at(0), 2.0)), std::invalid_argument);
}

TEST(Lane, geometricallyInterpolatesBetweenKeyframes)
{
    Lane lane(StringId{1}, LANE_LABEL, LANE_KIND, at(0), at(30));
    lane.add(Keyframe(StringId{2}, at(0), 1.0, KeyframeInterpolation::GEOMETRIC, {}));
    lane.add(Keyframe(StringId{3}, at(20), 9.0));

    const std::optional<double> value = lane.evaluate_keyframes(at(10));

    ASSERT_TRUE(value);
    EXPECT_NEAR(3.0, *value, 1e-12);
}

TEST(Keyframe, rejectsNonpositiveGeometricValue)
{
    EXPECT_THROW(Keyframe(StringId{2}, at(0), 0.0, KeyframeInterpolation::GEOMETRIC, {}), std::invalid_argument);
}

TEST(Lane, rejectsNonpositiveGeometricEndpoint)
{
    Lane lane(StringId{1}, LANE_LABEL, LANE_KIND, at(0), at(30));
    lane.add(Keyframe(StringId{2}, at(0), 1.0, KeyframeInterpolation::GEOMETRIC, {}));

    EXPECT_THROW(lane.add(Keyframe(StringId{3}, at(20), 0.0)), std::invalid_argument);
}
