// Copyright (c) 2026 Richard Thomson

#include <timelineParAnimator/TimelineJson.h>

#include <timeline/Layout.h>
#include <timeline/Query.h>

#include <gtest/gtest.h>

#include <array>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>

using namespace timeline_par_animator;

namespace
{

std::string golden_entry(const std::string &golden, int frame)
{
    std::ostringstream name_builder;
    name_builder << "frame-" << std::setfill('0') << std::setw(4) << frame + 1 << " {";
    const std::string name = name_builder.str();
    const std::size_t start = golden.find(name);
    if (start == std::string::npos)
    {
        return {};
    }
    return golden.substr(start, golden.find('}', start) - start);
}

std::optional<double> golden_value(const std::string &entry, const std::string &parameter, int component)
{
    const std::string name = "\n    " + parameter + "=";
    std::size_t start = entry.find(name);
    if (start == std::string::npos)
    {
        return std::nullopt;
    }
    start += name.size();
    for (int index = 0; index < component; ++index)
    {
        start = entry.find('/', start) + 1;
    }
    return std::stod(entry.substr(start));
}

} // namespace

TEST(IntegerOutput, retainsFractionalCurveAndDeclaresOutputRounding)
{
    const JsonImportResult imported = import_timeline_json("fixtures/integer-output.json");
    ASSERT_TRUE(imported.succeeded());
    ASSERT_TRUE(imported.diagnostics.empty());
    ASSERT_EQ(7, imported.document->lane_count());
    const timeline::FrameInspection inspection = *timeline::inspect_frame(*imported.document, 1);
    EXPECT_DOUBLE_EQ(100.5, *inspection.lanes[0].value);
    EXPECT_DOUBLE_EQ(-2.5, *inspection.lanes[1].value);
    EXPECT_DOUBLE_EQ(-2.5, *inspection.lanes[2].value);
    EXPECT_DOUBLE_EQ(100.5, *inspection.lanes[3].value);
    EXPECT_DOUBLE_EQ(-2.5, *inspection.lanes[4].value);
    EXPECT_DOUBLE_EQ(1.5, *inspection.lanes[5].value);
    for (int lane = 0; lane < 5; ++lane)
    {
        EXPECT_EQ(1, inspection.lanes[lane].items[0].attributes.count("output-rounding"));
        EXPECT_EQ("nearest-half-away-from-zero", inspection.lanes[lane].items[0].attributes.at("output-rounding"));
    }
    EXPECT_DOUBLE_EQ(101, *inspection.lanes[0].output_value);
    EXPECT_DOUBLE_EQ(-3, *inspection.lanes[1].output_value);
    EXPECT_DOUBLE_EQ(-3, *inspection.lanes[2].output_value);
    EXPECT_DOUBLE_EQ(101, *inspection.lanes[3].output_value);
    EXPECT_DOUBLE_EQ(-3, *inspection.lanes[4].output_value);
    EXPECT_FALSE(inspection.lanes[5].output_value);
    EXPECT_FALSE(inspection.lanes[6].output_value);
    const timeline::Time half = imported.document->frame_grid()->frame_start(1);
    const timeline::Lane &positive = imported.document->lanes()[0];
    const timeline::Lane &negative = imported.document->lanes()[1];
    EXPECT_DOUBLE_EQ(100, *positive.evaluate_keyframe_output(timeline::Time::from_ticks(half.ticks() - 1)));
    EXPECT_DOUBLE_EQ(101, *positive.evaluate_keyframe_output(timeline::Time::from_ticks(half.ticks() + 1)));
    EXPECT_DOUBLE_EQ(-3, *negative.evaluate_keyframe_output(timeline::Time::from_ticks(half.ticks() - 1)));
    EXPECT_DOUBLE_EQ(-2, *negative.evaluate_keyframe_output(timeline::Time::from_ticks(half.ticks() + 1)));
    EXPECT_FALSE(import_timeline_json("fixtures/integer-output-invalid.json").succeeded());
}

TEST(IntegerOutput, matchesSourceGoldenAndSurvivesCopyingComparisonAndLayout)
{
    JsonImportResult imported = import_timeline_json("fixtures/integer-output.json");
    ASSERT_TRUE(imported.succeeded());
    const timeline::Document document = *imported.document;
    imported.document.reset();
    std::ifstream input("fixtures/gold-integer-output.par");
    ASSERT_TRUE(input);
    const std::string golden{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    const std::array<std::string, 5> parameters{"maxiter", "negative", "choice", "tuple", "tuple"};
    for (int frame = 0; frame < document.frame_grid()->frame_count(); ++frame)
    {
        const std::string entry = golden_entry(golden, frame);
        ASSERT_FALSE(entry.empty());
        const timeline::FrameInspection inspection = *timeline::inspect_frame(document, frame);
        for (int lane = 0; lane < 5; ++lane)
        {
            SCOPED_TRACE(parameters[lane] + " frame " + std::to_string(frame));
            EXPECT_EQ(golden_value(entry, parameters[lane], lane == 4 ? 1 : 0), inspection.lanes[lane].output_value);
        }
        EXPECT_EQ(golden_value(entry, "bailout", 0), inspection.lanes[5].value);
        EXPECT_FALSE(inspection.lanes[5].output_value);
        EXPECT_FALSE(inspection.lanes[6].value);
        EXPECT_EQ(frame == 6 ? "b" : "a", inspection.lanes[6].items[0].attributes.at("value"));
    }
    const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
    ASSERT_TRUE(music.succeeded());
    const timeline::Document combined = timeline::combine_documents(document, *music.document);
    ASSERT_EQ(11, combined.lane_count());
    EXPECT_DOUBLE_EQ(101, *timeline::inspect_frame(combined, 1)->lanes[0].output_value);
    const timeline::Layout layout(combined,
        timeline::Viewport(600, 400, document.frame_grid()->offset(), document.frame_grid()->end_time()),
        timeline::LayoutMetrics(100, 20, 40, 4));
    bool hit = false;
    for (const timeline::Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<timeline::Polyline>(primitive))
        {
            const timeline::Polyline &line = std::get<timeline::Polyline>(primitive);
            if (layout.display_list().strings().lookup(line.id.lane_id) == "animation-0")
            {
                const std::optional<timeline::HitResult> result = layout.hit_test(line.points.front(), 0);
                ASSERT_TRUE(result);
                EXPECT_EQ("animation-0", layout.display_list().strings().lookup(result->id.lane_id));
                EXPECT_EQ("animation-0-key-0", result->id.item_id);
                hit = true;
            }
        }
    }
    EXPECT_TRUE(hit);
}

TEST(IntegerOutput, roundsAfterExtrapolationAndRetainsGapsAndHeldValues)
{
    const JsonImportResult imported = import_timeline_json("fixtures/integer-output-policies.json");
    ASSERT_TRUE(imported.succeeded());
    ASSERT_TRUE(imported.diagnostics.empty());
    ASSERT_EQ(6, imported.document->lane_count());
    std::ifstream input("fixtures/gold-integer-output-policies.par");
    ASSERT_TRUE(input);
    const std::string golden{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    const std::array<std::string, 6> parameters{"base", "omit", "cycle", "ping", "hold", "step"};
    for (int frame = 0; frame < imported.document->frame_grid()->frame_count(); ++frame)
    {
        const std::string entry = golden_entry(golden, frame);
        ASSERT_FALSE(entry.empty());
        const timeline::FrameInspection inspection = *timeline::inspect_frame(*imported.document, frame);
        for (int lane = 0; lane < 6; ++lane)
        {
            SCOPED_TRACE(parameters[lane] + " frame " + std::to_string(frame));
            EXPECT_EQ(golden_value(entry, parameters[lane], 0), inspection.lanes[lane].output_value);
        }
    }
    const timeline::FrameInspection middle = *timeline::inspect_frame(*imported.document, 3);
    EXPECT_DOUBLE_EQ(101.5, *middle.lanes[2].value);
    EXPECT_DOUBLE_EQ(102, *middle.lanes[2].output_value);
    EXPECT_DOUBLE_EQ(-1.5, *middle.lanes[3].value);
    EXPECT_DOUBLE_EQ(-2, *middle.lanes[3].output_value);
    EXPECT_FALSE(timeline::inspect_frame(*imported.document, 0)->lanes[1].output_value);
    EXPECT_DOUBLE_EQ(678, *timeline::inspect_frame(*imported.document, 0)->lanes[0].output_value);
}

TEST(IntegerOutput, diagnosesInvalidKeysTransactionally)
{
    const JsonImportResult imported = import_timeline_json("fixtures/integer-output-partial.json");
    ASSERT_TRUE(imported.succeeded());
    ASSERT_EQ(6, imported.diagnostics.size());
    ASSERT_EQ(1, imported.document->lane_count());
    EXPECT_EQ("animation-6", imported.document->strings().lookup(imported.document->lanes()[0].id()));
    for (int index = 0; index < 6; ++index)
    {
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find("animation-" + std::to_string(index) + ":"));
    }
    EXPECT_NE(std::string::npos, imported.diagnostics[0].find("integral"));
    EXPECT_NE(std::string::npos, imported.diagnostics[1].find("int range"));
    EXPECT_NE(std::string::npos, imported.diagnostics[2].find("curves"));
    EXPECT_NE(std::string::npos, imported.diagnostics[3].find("integral"));
    EXPECT_NE(std::string::npos, imported.diagnostics[4].find("arity"));
    EXPECT_NE(std::string::npos, imported.diagnostics[5].find("bounds"));
    EXPECT_FALSE(timeline::inspect_frame(*imported.document, 1)->lanes[0].output_value);
}

TEST(IntegerOutput, preservesSourceArithmeticOrderNearHalfwayBoundaries)
{
    const JsonImportResult imported = import_timeline_json("fixtures/integer-output-boundary.json");
    ASSERT_TRUE(imported.succeeded());
    std::ifstream input("fixtures/gold-integer-output-boundary.par");
    ASSERT_TRUE(input);
    const std::string golden{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    for (int frame = 0; frame < 23; ++frame)
    {
        SCOPED_TRACE(frame);
        const std::string entry = golden_entry(golden, frame);
        ASSERT_FALSE(entry.empty());
        const timeline::FrameInspection inspection = *timeline::inspect_frame(*imported.document, frame);
        EXPECT_EQ(golden_value(entry, "choice", 0), inspection.lanes[0].output_value);
        EXPECT_EQ(golden_value(entry, "negative", 0), inspection.lanes[1].output_value);
    }
    const timeline::FrameInspection boundary = *timeline::inspect_frame(*imported.document, 15);
    EXPECT_LT(*boundary.lanes[0].value, 7.5);
    EXPECT_GT(*boundary.lanes[1].value, -7.5);
    EXPECT_DOUBLE_EQ(7, *boundary.lanes[0].output_value);
    EXPECT_DOUBLE_EQ(-7, *boundary.lanes[1].output_value);
}
