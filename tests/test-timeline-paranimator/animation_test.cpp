// Copyright (c) 2026 Richard Thomson

#include <timelineParAnimator/TimelineJson.h>

#include <timeline/Layout.h>
#include <timeline/Query.h>
#include <timeline/Snapshot.h>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <variant>

using namespace timeline_par_animator;

TEST(AnimationImport, preservesSourceFunctionsInPwmOutputLikeParanimator)
{
    const JsonImportResult result = import_timeline_json("fixtures/function-slot-pwm.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.diagnostics.empty());
    ASSERT_EQ(2, result.document->lane_count());
    const std::array<std::string, 4> values{"sin/tan", "sin/tan", "sin/log", "sin/log"};
    std::ifstream golden_file("fixtures/gold-function-slot-pwm.par");
    ASSERT_TRUE(golden_file);
    const std::string golden{std::istreambuf_iterator<char>(golden_file), std::istreambuf_iterator<char>()};
    for (int frame = 0; frame < 4; ++frame)
    {
        const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, frame);
        ASSERT_TRUE(inspection);
        ASSERT_EQ(1, timeline::size_cast(inspection->lanes[0].items));
        const timeline::Attributes &attributes = inspection->lanes[0].items.front().attributes;
        EXPECT_EQ(values[frame], attributes.at("value"));
        EXPECT_EQ("1", attributes.at("slot"));
        EXPECT_EQ("sin/cos", attributes.at("source-value"));
        EXPECT_EQ("Function_Demo", attributes.at("source-entry"));
        EXPECT_EQ("function[1]", attributes.at("parameter"));
        ASSERT_TRUE(inspection->lanes[1].value);
        EXPECT_DOUBLE_EQ(frame / 3.0, *inspection->lanes[1].value);
        const std::size_t start = golden.find("frame-000" + std::to_string(frame + 1) + " {");
        ASSERT_NE(std::string::npos, start);
        EXPECT_NE(
            std::string::npos, golden.substr(start, golden.find('}', start) - start).find("function=" + values[frame]));
    }
    const timeline::FrameGrid &grid = *result.document->frame_grid();
    const timeline::Layout layout(*result.document, timeline::Viewport(500, 140, grid.offset(), grid.end_time()),
        timeline::LayoutMetrics(100, 20, 30, 4));
    const std::optional<timeline::HitResult> hit = layout.hit_test({350, 35}, 2);
    ASSERT_TRUE(hit);
    EXPECT_EQ("animation-0-pwm-2", hit->id.item_id);
    EXPECT_NE(std::string::npos, timeline::render_snapshot(layout.display_list()).find("function[1] / mix"));
}

TEST(AnimationImport, retainsOtherPwmSlotsAndFillsMissingSlotsWithIdent)
{
    const timeline::Document document = []
    {
        const JsonImportResult imported = import_timeline_json("fixtures/function-slot-variants.json");
        if (!imported.succeeded() || !imported.diagnostics.empty())
        {
            throw std::runtime_error("Function-slot PWM import failed");
        }
        return *imported.document;
    }();
    ASSERT_EQ(4, document.lane_count());
    const std::array<std::string, 4> values{"log/cos", "tan/cos", "log/cos", "tan/cos"};
    for (int frame = 0; frame < 4; ++frame)
    {
        const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(document, frame);
        ASSERT_TRUE(inspection);
        EXPECT_EQ(values[frame], inspection->lanes[0].items.front().attributes.at("value"));
        EXPECT_EQ(frame < 2 ? "sin/cos/ident/tan" : "sin/cos/ident/log",
            inspection->lanes[2].items.front().attributes.at("value"));
    }
    const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
    ASSERT_TRUE(music.succeeded());
    const timeline::Document combined = timeline::combine_documents(*music.document, document);
    ASSERT_EQ(8, combined.lane_count());
    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(combined, 2);
    ASSERT_TRUE(inspection);
    EXPECT_EQ("log/cos", inspection->lanes[4].items.front().attributes.at("value"));
}

TEST(AnimationImport, diagnosesInvalidPwmFunctionSlotsAndEndpoints)
{
    const JsonImportResult result = import_timeline_json("fixtures/partial-function-slot-pwm.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(6, timeline::size_cast(result.diagnostics));
    ASSERT_EQ(2, result.document->lane_count());
    for (int index = 0; index < 6; ++index)
    {
        EXPECT_NE(std::string::npos, result.diagnostics[index].find("animation-" + std::to_string(index)));
    }
    EXPECT_EQ("animation-6", result.document->strings().lookup(result.document->lanes().front().id()));
    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, 2);
    ASSERT_TRUE(inspection);
    EXPECT_EQ("sin/log", inspection->lanes.front().items.front().attributes.at("value"));
}

TEST(AnimationImport, resolvesLayerSourcesAndCatalogFunctionSlotsWithIndexedErrors)
{
    const JsonImportResult result = import_timeline_json("fixtures/function-slot-sources.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(6, result.document->lane_count());
    ASSERT_EQ(4, timeline::size_cast(result.diagnostics));
    EXPECT_NE(std::string::npos, result.diagnostics[0].find("animation-layer-0-1"));
    EXPECT_NE(std::string::npos, result.diagnostics[0].find("not declared"));
    EXPECT_NE(std::string::npos, result.diagnostics[1].find("unable to open PAR source"));
    EXPECT_NE(std::string::npos, result.diagnostics[2].find("entry not found"));
    EXPECT_NE(std::string::npos, result.diagnostics[3].find("unterminated PAR source entry"));
    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, 2);
    ASSERT_TRUE(inspection);
    EXPECT_EQ("log/exp", inspection->lanes[0].items.front().attributes.at("value"));
    EXPECT_EQ("fractal", inspection->lanes[0].items.front().attributes.at("layer"));
    EXPECT_EQ("ident/ident/log", inspection->lanes[2].items.front().attributes.at("value"));
    EXPECT_EQ("sin/log/exp", inspection->lanes[4].items.front().attributes.at("value"));
    EXPECT_EQ("sin/cos/exp", inspection->lanes[4].items.front().attributes.at("source-value"));
}

TEST(AnimationImport, displaysPwmMixAndFrameAlignedOutputLikeParanimator)
{
    const JsonImportResult result = import_timeline_json("fixtures/yes-no-pwm.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.diagnostics.empty());
    ASSERT_EQ(2, result.document->lane_count());
    EXPECT_EQ(1, result.document->track_count());
    EXPECT_EQ(2, result.document->keyframe_count());
    const timeline::FrameGrid &grid = *result.document->frame_grid();
    const std::array<std::string, 4> values{"no", "no", "yes", "yes"};
    std::ifstream golden_file("fixtures/gold-yes-no-pwm.par");
    ASSERT_TRUE(golden_file);
    const std::string golden{std::istreambuf_iterator<char>(golden_file), std::istreambuf_iterator<char>()};
    for (int frame = 0; frame < 4; ++frame)
    {
        const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, frame);
        ASSERT_TRUE(inspection);
        ASSERT_EQ(1, timeline::size_cast(inspection->lanes[0].items));
        const timeline::InspectionItem &item = inspection->lanes[0].items.front();
        EXPECT_EQ(timeline::InspectionItemType::INTERVAL, item.type);
        EXPECT_FALSE(item.value);
        EXPECT_EQ(values[frame], item.attributes.at("value"));
        EXPECT_EQ("pwm", item.attributes.at("mode"));
        EXPECT_NE(std::string::npos, item.attributes.at("pwm").find("duty"));
        ASSERT_TRUE(inspection->lanes[1].value);
        EXPECT_DOUBLE_EQ(frame / 3.0, *inspection->lanes[1].value);
        const std::size_t start = golden.find("frame-000" + std::to_string(frame + 1) + " {");
        ASSERT_NE(std::string::npos, start);
        EXPECT_NE(std::string::npos,
            golden.substr(start, golden.find('}', start) - start).find("showorbit=" + values[frame]));
    }
    const timeline::Lane &output = result.document->lanes()[0];
    ASSERT_EQ(2, output.item_count());
    const timeline::Interval &first = std::get<timeline::Interval>(output.items().front());
    EXPECT_EQ(grid.offset(), first.start());
    EXPECT_EQ(grid.frame_start(2), first.end());
    const timeline::Interval &last = std::get<timeline::Interval>(output.items().back());
    EXPECT_EQ(grid.end_time(), last.end());
    const timeline::Layout layout(*result.document, timeline::Viewport(500, 140, grid.offset(), grid.end_time()),
        timeline::LayoutMetrics(100, 20, 30, 4));
    const std::optional<timeline::HitResult> hit = layout.hit_test({150, 35}, 2);
    ASSERT_TRUE(hit);
    EXPECT_EQ(first.id(), hit->id.item_id);
    EXPECT_NE(std::string::npos, timeline::render_snapshot(layout.display_list()).find("showorbit / mix"));
}

TEST(AnimationImport, preservesPwmRoundingAliasesAndCategoricalWindowBoundaries)
{
    const timeline::Document document = []
    {
        const JsonImportResult imported = import_timeline_json("fixtures/pwm-variants.json");
        if (!imported.succeeded() || !imported.diagnostics.empty())
        {
            throw std::runtime_error("PWM variant import failed");
        }
        return *imported.document;
    }();
    ASSERT_EQ(8, document.lane_count());
    EXPECT_EQ(4, document.track_count());
    EXPECT_EQ(8, document.keyframe_count());
    const std::array<std::array<std::string, 8>, 4> values{{
        {"bof60", "bof60", "1", "1", "bof60", "bof60", "1", "1"},
        {"no", "yes", "no", "yes", "no", "yes", "no", "yes"},
        {"no", "no", "no", "no", "show", "show", "show", "show"},
        {"yes", "yes", "no", "yes", "no", "no", "no", "no"},
    }};
    for (int frame = 0; frame < 8; ++frame)
    {
        const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(document, frame);
        ASSERT_TRUE(inspection);
        for (int track = 0; track < 4; ++track)
        {
            ASSERT_EQ(1, timeline::size_cast(inspection->lanes[2 * track].items));
            EXPECT_EQ(values[track][frame], inspection->lanes[2 * track].items.front().attributes.at("value"));
        }
    }
    const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
    ASSERT_TRUE(music.succeeded());
    const timeline::Document combined = timeline::combine_documents(*music.document, document);
    ASSERT_EQ(12, combined.lane_count());
    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(combined, 2);
    ASSERT_TRUE(inspection);
    EXPECT_EQ("1", inspection->lanes[4].items.front().attributes.at("value"));
}

TEST(AnimationImport, diagnosesInvalidPwmRecipesAndRetainsValidOutput)
{
    const JsonImportResult result = import_timeline_json("fixtures/partial-pwm.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(14, timeline::size_cast(result.diagnostics));
    ASSERT_EQ(2, result.document->lane_count());
    for (int index = 0; index < 14; ++index)
    {
        EXPECT_NE(std::string::npos, result.diagnostics[index].find("animation-" + std::to_string(index)));
    }
    EXPECT_NE(std::string::npos, result.diagnostics[13].find("slot"));
    EXPECT_EQ("animation-14", result.document->strings().lookup(result.document->lanes().front().id()));
    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, 2);
    ASSERT_TRUE(inspection);
    EXPECT_EQ("yes", inspection->lanes.front().items.front().attributes.at("value"));
}

TEST(AnimationImport, samplesAnalyticCatmullRomLikeParanimatorAcrossSegments)
{
    const JsonImportResult result = import_timeline_json("fixtures/catmull-rom-path.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.diagnostics.empty());
    ASSERT_EQ(2, result.document->lane_count());
    EXPECT_EQ(1, result.document->track_count());
    EXPECT_EQ(0, result.document->keyframe_count());
    const timeline::FrameGrid &grid = *result.document->frame_grid();
    const std::array<double, 7> x{0.0, 0.4375, 1.0, 2.0, 3.0, 3.5625, 4.0};
    const std::array<double, 7> y{0.0, 1.125, 2.0, 2.25, 2.0, 1.125, 0.0};
    const std::array<std::string, 7> parameters{"0/0", "0.4375/1.125", "1/2", "2/2.25", "3/2", "3.5625/1.125", "4/0"};
    std::ifstream golden_file("fixtures/gold-catmull-rom-path.par");
    ASSERT_TRUE(golden_file);
    const std::string golden{std::istreambuf_iterator<char>(golden_file), std::istreambuf_iterator<char>()};
    for (int frame = 0; frame < 7; ++frame)
    {
        const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, frame);
        ASSERT_TRUE(inspection);
        ASSERT_EQ(2, timeline::size_cast(inspection->lanes));
        for (int component = 0; component < 2; ++component)
        {
            const timeline::InspectionItem &item = inspection->lanes[component].items.front();
            ASSERT_TRUE(item.value);
            EXPECT_DOUBLE_EQ(component == 0 ? x[frame] : y[frame], *item.value);
            EXPECT_EQ(timeline::InspectionItemRole::SAMPLED, item.role);
            EXPECT_EQ("params.c", item.attributes.at("parameter"));
            EXPECT_EQ("animation-0", item.attributes.at("track"));
            EXPECT_EQ(std::to_string(component), item.attributes.at("component"));
            EXPECT_EQ("{\"control-points\":[\"0/0\",\"1/2\",\"3/2\",\"4/0\"],\"kind\":\"catmull-rom\"}",
                item.attributes.at("path"));
        }
        const std::size_t start = golden.find("frame-000" + std::to_string(frame + 1) + " {");
        ASSERT_NE(std::string::npos, start);
        const std::string entry = golden.substr(start, golden.find('}', start) - start);
        EXPECT_NE(std::string::npos, entry.find("params=" + parameters[frame]));
    }
    const timeline::Curve &curve = std::get<timeline::Curve>(result.document->lanes()[1].items().front());
    EXPECT_EQ(timeline::CurveInterpolation::ANALYTIC, curve.interpolation());
    EXPECT_EQ(0, curve.sample_count());
    ASSERT_TRUE(curve.minimum());
    ASSERT_TRUE(curve.maximum());
    EXPECT_LE(*curve.minimum(), 0.0);
    EXPECT_GE(*curve.maximum(), 2.25);
    EXPECT_DOUBLE_EQ(
        0.546875, curve.sample(grid.offset() + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2)));
    const timeline::Layout layout(*result.document, timeline::Viewport(500, 140, grid.offset(), grid.end_time()),
        timeline::LayoutMetrics(100, 20, 30, 4));
    int curve_count = 0;
    for (const timeline::Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<timeline::Polyline>(primitive))
        {
            const timeline::Polyline &line = std::get<timeline::Polyline>(primitive);
            if (line.style == timeline::StyleRole::CURVE)
            {
                ++curve_count;
                EXPECT_EQ(7, timeline::size_cast(line.points));
                const std::optional<timeline::HitResult> hit = layout.hit_test(line.points[3], 2);
                ASSERT_TRUE(hit);
                EXPECT_EQ(line.id.item_id, hit->id.item_id);
            }
        }
    }
    EXPECT_EQ(2, curve_count);
}

TEST(AnimationImport, ownsCatmullRomTuplesWithExtraPointsAndEndpointTangents)
{
    const timeline::Document document = []
    {
        const JsonImportResult imported = import_timeline_json("fixtures/catmull-rom-tuples.json");
        if (!imported.succeeded() || !imported.diagnostics.empty())
        {
            throw std::runtime_error("Catmull-Rom tuple import failed");
        }
        return *imported.document;
    }();
    ASSERT_EQ(6, document.lane_count());
    EXPECT_EQ(3, document.track_count());
    EXPECT_EQ(0, document.keyframe_count());
    const timeline::FrameGrid &grid = *document.frame_grid();
    const std::array<double, 9> x{0.0, 0.4375, 1.0, 2.0, 3.0, 3.5, 4.0, 4.9375, 6.0};
    const std::array<double, 9> y{0.0, 1.125, 2.0, 2.25, 2.0, 1.125, 0.0, -1.0, -2.0};
    for (int frame = 0; frame < 9; ++frame)
    {
        for (int component = 0; component < 3; ++component)
        {
            const timeline::Curve &curve = std::get<timeline::Curve>(document.lanes()[component].items().front());
            EXPECT_DOUBLE_EQ(component == 0 ? x[frame]
                    : component == 1        ? y[frame]
                                            : 3.0 * x[frame],
                curve.sample(grid.frame_start(frame)));
        }
        const timeline::Curve &constant = std::get<timeline::Curve>(document.lanes()[3].items().front());
        EXPECT_DOUBLE_EQ(3.0, constant.sample(grid.frame_start(frame)));
        for (int component = 0; component < 2; ++component)
        {
            const timeline::Curve &line = std::get<timeline::Curve>(document.lanes()[4 + component].items().front());
            EXPECT_DOUBLE_EQ(3.0 * (component + 1) * frame / 8.0, line.sample(grid.frame_start(frame)));
        }
    }
    EXPECT_EQ("animation-1", document.strings().lookup(document.lanes()[3].id()));
    const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
    ASSERT_TRUE(music.succeeded());
    const timeline::Document combined = timeline::combine_documents(*music.document, document);
    ASSERT_EQ(10, combined.lane_count());
    const timeline::Curve &copy = std::get<timeline::Curve>(combined.lanes()[5].items().front());
    EXPECT_DOUBLE_EQ(2.25, copy.sample(grid.frame_start(3)));
    EXPECT_EQ(0, copy.sample_count());
}

TEST(AnimationImport, diagnosesMalformedCatmullRomRecipesWithoutLosingValidTracks)
{
    const JsonImportResult result = import_timeline_json("fixtures/partial-catmull-rom-paths.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(10, timeline::size_cast(result.diagnostics));
    ASSERT_EQ(4, result.document->lane_count());
    for (int index = 0; index < 11; ++index)
    {
        if (index != 7)
        {
            const int diagnostic = index < 7 ? index : index - 1;
            EXPECT_NE(std::string::npos, result.diagnostics[diagnostic].find("animation-" + std::to_string(index)));
        }
    }
    EXPECT_NE(std::string::npos, result.diagnostics[0].find("at least four control points"));
    EXPECT_EQ("animation-7[0]", result.document->strings().lookup(result.document->lanes()[0].id()));
    EXPECT_EQ("true", std::get<timeline::Curve>(result.document->lanes()[0].items()[0]).attributes().at("normalize"));
    EXPECT_EQ("animation-11[0]", result.document->strings().lookup(result.document->lanes()[2].id()));
    const timeline::Curve &curve = std::get<timeline::Curve>(result.document->lanes()[3].items().front());
    EXPECT_DOUBLE_EQ(2.25, curve.sample(result.document->frame_grid()->frame_start(3)));
    const JsonImportResult single = import_timeline_json("fixtures/single-frame-catmull-rom.json");
    EXPECT_FALSE(single.succeeded());
    ASSERT_FALSE(single.diagnostics.empty());
    EXPECT_NE(std::string::npos, single.diagnostics.front().find("at least two frames"));
}

TEST(AnimationImport, samplesAnalyticBezierLikeParanimatorAndPreservesControlPoints)
{
    const JsonImportResult result = import_timeline_json("fixtures/bezier-path.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.diagnostics.empty());
    ASSERT_EQ(2, result.document->lane_count());
    EXPECT_EQ(1, result.document->track_count());
    EXPECT_EQ(0, result.document->keyframe_count());
    const timeline::FrameGrid &grid = *result.document->frame_grid();
    const std::array<double, 5> x{0.0, 1.0, 2.0, 3.0, 4.0};
    const std::array<double, 5> y{0.0, 1.5, 2.0, 1.5, 0.0};
    const std::array<std::string, 5> parameters{"0/0", "1/1.5", "2/2", "3/1.5", "4/0"};
    std::ifstream golden_file("fixtures/gold-bezier-path.par");
    ASSERT_TRUE(golden_file);
    const std::string golden{std::istreambuf_iterator<char>(golden_file), std::istreambuf_iterator<char>()};
    for (int frame = 0; frame < 5; ++frame)
    {
        const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, frame);
        ASSERT_TRUE(inspection);
        ASSERT_EQ(2, timeline::size_cast(inspection->lanes));
        for (int component = 0; component < 2; ++component)
        {
            const timeline::LaneInspection &lane = inspection->lanes[component];
            ASSERT_EQ(1, timeline::size_cast(lane.items));
            ASSERT_TRUE(lane.items.front().value);
            EXPECT_DOUBLE_EQ(component == 0 ? x[frame] : y[frame], *lane.items.front().value);
            EXPECT_EQ(timeline::InspectionItemRole::SAMPLED, lane.items.front().role);
            EXPECT_EQ("params.c", lane.items.front().attributes.at("parameter"));
            EXPECT_EQ("animation-0", lane.items.front().attributes.at("track"));
            EXPECT_EQ(std::to_string(component), lane.items.front().attributes.at("component"));
            EXPECT_EQ("{\"control-points\":[\"0/0\",\"2/4\",\"4/0\"],\"kind\":\"bezier\"}",
                lane.items.front().attributes.at("path"));
        }
        const std::size_t start = golden.find("frame-000" + std::to_string(frame + 1) + " {");
        ASSERT_NE(std::string::npos, start);
        const std::string entry = golden.substr(start, golden.find('}', start) - start);
        EXPECT_NE(std::string::npos, entry.find("params=" + parameters[frame]));
    }
    const timeline::Curve &curve = std::get<timeline::Curve>(result.document->lanes()[1].items().front());
    EXPECT_EQ(timeline::CurveInterpolation::ANALYTIC, curve.interpolation());
    EXPECT_EQ(0, curve.sample_count());
    EXPECT_DOUBLE_EQ(0.0, *curve.minimum());
    EXPECT_DOUBLE_EQ(4.0, *curve.maximum());
    EXPECT_DOUBLE_EQ(
        0.875, curve.sample(grid.offset() + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2)));
    const timeline::Layout layout(*result.document, timeline::Viewport(500, 140, grid.offset(), grid.end_time()),
        timeline::LayoutMetrics(100, 20, 30, 4));
    int curve_count = 0;
    for (const timeline::Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<timeline::Polyline>(primitive))
        {
            const timeline::Polyline &line = std::get<timeline::Polyline>(primitive);
            if (line.style == timeline::StyleRole::CURVE)
            {
                ++curve_count;
                EXPECT_EQ(5, timeline::size_cast(line.points));
                const std::optional<timeline::HitResult> hit = layout.hit_test(line.points[2], 2);
                ASSERT_TRUE(hit);
                EXPECT_EQ(line.id.item_id, hit->id.item_id);
            }
        }
    }
    EXPECT_EQ(2, curve_count);
}

TEST(AnimationImport, ownsBezierTupleDefinitionsAcrossDegreesAndComparison)
{
    const timeline::Document document = []
    {
        const JsonImportResult imported = import_timeline_json("fixtures/bezier-tuples.json");
        if (!imported.succeeded() || !imported.diagnostics.empty())
        {
            throw std::runtime_error("Bezier tuple import failed");
        }
        return *imported.document;
    }();
    const std::array<double, 15> midpoint{2.0, 4.0, 0.5, 0.5, 2.0, 2.0, 5.0, 2.0, 3.0, 1.0, 1.0, 2.0, 3.0, 4.0, 2.0};
    ASSERT_EQ(15, document.lane_count());
    EXPECT_EQ(6, document.track_count());
    EXPECT_EQ(0, document.keyframe_count());
    const timeline::FrameGrid &grid = *document.frame_grid();
    for (int index = 0; index < document.lane_count(); ++index)
    {
        const timeline::Curve &curve = std::get<timeline::Curve>(document.lanes()[index].items().front());
        EXPECT_DOUBLE_EQ(midpoint[index], curve.sample(grid.frame_start(2)));
        EXPECT_EQ(0, curve.sample_count());
        EXPECT_NE(std::string::npos, curve.attributes().at("path").find("control-points"));
    }
    EXPECT_EQ("animation-5", document.strings().lookup(document.lanes()[14].id()));
    const timeline::Curve &quartic = std::get<timeline::Curve>(document.lanes()[10].items().front());
    EXPECT_DOUBLE_EQ(0.0, quartic.sample(grid.frame_start(0)));
    EXPECT_DOUBLE_EQ(16.0, quartic.sample(grid.frame_start(4)));
    EXPECT_DOUBLE_EQ(0.0625, quartic.sample(grid.frame_start(1)));
    const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
    ASSERT_TRUE(music.succeeded());
    const timeline::Document combined = timeline::combine_documents(*music.document, document);
    ASSERT_EQ(19, combined.lane_count());
    const timeline::Curve &copy = std::get<timeline::Curve>(combined.lanes()[14].items().front());
    EXPECT_DOUBLE_EQ(1.0, copy.sample(grid.frame_start(2)));
    EXPECT_EQ(quartic.attributes(), copy.attributes());
}

TEST(AnimationImport, diagnosesMalformedBezierPointsAndUnsupportedTargets)
{
    const JsonImportResult result = import_timeline_json("fixtures/partial-bezier-paths.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(10, timeline::size_cast(result.diagnostics));
    ASSERT_EQ(6, result.document->lane_count());
    for (int index = 0; index < 11; ++index)
    {
        if (index != 7)
        {
            const int diagnostic = index < 7 ? index : index - 1;
            EXPECT_NE(std::string::npos, result.diagnostics[diagnostic].find("animation-" + std::to_string(index)));
        }
    }
    EXPECT_EQ("animation-7[0]", result.document->strings().lookup(result.document->lanes()[0].id()));
    EXPECT_EQ("true", std::get<timeline::Curve>(result.document->lanes()[0].items()[0]).attributes().at("normalize"));
    EXPECT_EQ("animation-11[0]", result.document->strings().lookup(result.document->lanes()[2].id()));
    const timeline::FrameGrid &grid = *result.document->frame_grid();
    const timeline::Curve &constant = std::get<timeline::Curve>(result.document->lanes()[2].items().front());
    EXPECT_DOUBLE_EQ(1.0, constant.sample(grid.frame_start(2)));
    const timeline::Curve &line = std::get<timeline::Curve>(result.document->lanes()[5].items().front());
    EXPECT_DOUBLE_EQ(4.0, line.sample(grid.frame_start(2)));
    const JsonImportResult single = import_timeline_json("fixtures/single-frame-bezier.json");
    EXPECT_FALSE(single.succeeded());
    ASSERT_FALSE(single.diagnostics.empty());
    EXPECT_NE(std::string::npos, single.diagnostics.front().find("at least two frames"));
}

TEST(AnimationImport, samplesAnalyticSpiralsLikeParanimatorAndPreservesTheRecipe)
{
    const JsonImportResult result = import_timeline_json("fixtures/spiral-path.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.diagnostics.empty());
    ASSERT_EQ(2, result.document->lane_count());
    EXPECT_EQ(1, result.document->track_count());
    EXPECT_EQ(0, result.document->keyframe_count());
    const timeline::FrameGrid &grid = *result.document->frame_grid();
    const std::array<double, 5> x{1.0, 0.0, -2.0, 0.0, 3.0};
    const std::array<double, 5> y{0.0, 1.5, 0.0, -2.5, 0.0};
    const std::array<std::string, 5> parameters{"1/0", "0/1.5", "-2/0", "0/-2.5", "3/0"};
    std::ifstream golden_file("fixtures/gold-spiral-path.par");
    ASSERT_TRUE(golden_file);
    const std::string golden{std::istreambuf_iterator<char>(golden_file), std::istreambuf_iterator<char>()};
    for (int frame = 0; frame < 5; ++frame)
    {
        const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, frame);
        ASSERT_TRUE(inspection);
        ASSERT_EQ(2, timeline::size_cast(inspection->lanes));
        for (int component = 0; component < 2; ++component)
        {
            const timeline::LaneInspection &lane = inspection->lanes[component];
            ASSERT_EQ(1, timeline::size_cast(lane.items));
            ASSERT_TRUE(lane.items.front().value);
            EXPECT_DOUBLE_EQ(component == 0 ? x[frame] : y[frame], *lane.items.front().value);
            EXPECT_EQ(timeline::InspectionItemRole::SAMPLED, lane.items.front().role);
            EXPECT_EQ("params.c", lane.items.front().attributes.at("parameter"));
            EXPECT_EQ("animation-0", lane.items.front().attributes.at("track"));
            EXPECT_EQ(std::to_string(component), lane.items.front().attributes.at("component"));
            EXPECT_EQ("{\"center\":\"0/0\",\"from-radius\":1,\"kind\":\"spiral\",\"to-radius\":3,\"turns\":1}",
                lane.items.front().attributes.at("path"));
        }
        const std::size_t start = golden.find("frame-000" + std::to_string(frame + 1) + " {");
        ASSERT_NE(std::string::npos, start);
        const std::string entry = golden.substr(start, golden.find('}', start) - start);
        EXPECT_NE(std::string::npos, entry.find("params=" + parameters[frame]));
    }
    const timeline::Curve &curve = std::get<timeline::Curve>(result.document->lanes()[0].items().front());
    EXPECT_EQ(timeline::CurveInterpolation::ANALYTIC, curve.interpolation());
    EXPECT_EQ(0, curve.sample_count());
    EXPECT_DOUBLE_EQ(-3.0, *curve.minimum());
    EXPECT_DOUBLE_EQ(3.0, *curve.maximum());
    EXPECT_NEAR(1.25 / std::sqrt(2.0),
        curve.sample(grid.offset() + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2)), 1e-12);
    const timeline::Layout layout(*result.document, timeline::Viewport(500, 140, grid.offset(), grid.end_time()),
        timeline::LayoutMetrics(100, 20, 30, 4));
    int curve_count = 0;
    for (const timeline::Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<timeline::Polyline>(primitive))
        {
            const timeline::Polyline &line = std::get<timeline::Polyline>(primitive);
            if (line.style == timeline::StyleRole::CURVE)
            {
                ++curve_count;
                EXPECT_EQ(5, timeline::size_cast(line.points));
                const std::optional<timeline::HitResult> hit = layout.hit_test(line.points[2], 2);
                ASSERT_TRUE(hit);
                EXPECT_EQ(line.id.item_id, hit->id.item_id);
            }
        }
    }
    EXPECT_EQ(2, curve_count);
}

TEST(AnimationImport, keepsReverseShrinkingSpiralsAndConstantRadiusDefinitionsOwned)
{
    const timeline::Document document = []
    {
        const JsonImportResult imported = import_timeline_json("fixtures/spiral-variants.json");
        if (!imported.succeeded() || !imported.diagnostics.empty())
        {
            throw std::runtime_error("spiral import failed");
        }
        return *imported.document;
    }();
    ASSERT_EQ(4, document.lane_count());
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Curve &x = std::get<timeline::Curve>(document.lanes()[0].items().front());
    const timeline::Curve &y = std::get<timeline::Curve>(document.lanes()[1].items().front());
    EXPECT_NEAR(1.0, x.sample(grid.frame_start(0)), 1e-12);
    EXPECT_DOUBLE_EQ(1.0, y.sample(grid.frame_start(0)));
    EXPECT_NEAR(1.0 + 2.5 / std::sqrt(2.0), x.sample(grid.frame_start(1)), 1e-12);
    EXPECT_NEAR(-2.0 + 2.5 / std::sqrt(2.0), y.sample(grid.frame_start(1)), 1e-12);
    EXPECT_DOUBLE_EQ(3.0, x.sample(grid.frame_start(2)));
    EXPECT_DOUBLE_EQ(-2.0, y.sample(grid.frame_start(2)));
    EXPECT_NEAR(1.0, x.sample(grid.frame_start(4)), 1e-12);
    EXPECT_DOUBLE_EQ(-3.0, y.sample(grid.frame_start(4)));
    EXPECT_DOUBLE_EQ(-2.0, *x.minimum());
    EXPECT_DOUBLE_EQ(4.0, *x.maximum());
    const JsonImportResult circle = import_timeline_json("fixtures/circle-path.json");
    ASSERT_TRUE(circle.succeeded());
    for (int component = 0; component < 2; ++component)
    {
        const timeline::Curve &constant = std::get<timeline::Curve>(document.lanes()[component + 2].items().front());
        const timeline::Curve &reference =
            std::get<timeline::Curve>(circle.document->lanes()[component].items().front());
        for (int frame = 0; frame < 5; ++frame)
        {
            EXPECT_DOUBLE_EQ(reference.sample(grid.frame_start(frame)), constant.sample(grid.frame_start(frame)));
        }
    }
    const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
    ASSERT_TRUE(music.succeeded());
    const timeline::Document combined = timeline::combine_documents(*music.document, document);
    ASSERT_EQ(8, combined.lane_count());
    const timeline::Curve &copy = std::get<timeline::Curve>(combined.lanes()[4].items().front());
    EXPECT_DOUBLE_EQ(3.0, copy.sample(grid.frame_start(2)));
    EXPECT_EQ(x.attributes(), copy.attributes());
}

TEST(AnimationImport, diagnosesInvalidSpiralRecipesAndKeepsDefaultsAndZeroRadii)
{
    const JsonImportResult result = import_timeline_json("fixtures/partial-spiral-paths.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(8, timeline::size_cast(result.diagnostics));
    ASSERT_EQ(4, result.document->lane_count());
    for (int index = 0; index < 8; ++index)
    {
        EXPECT_NE(std::string::npos, result.diagnostics[index].find("animation-" + std::to_string(index)));
    }
    const timeline::FrameGrid &grid = *result.document->frame_grid();
    const timeline::Curve &x = std::get<timeline::Curve>(result.document->lanes()[0].items().front());
    const timeline::Curve &y = std::get<timeline::Curve>(result.document->lanes()[1].items().front());
    EXPECT_EQ("animation-8[0]", result.document->strings().lookup(result.document->lanes()[0].id()));
    EXPECT_DOUBLE_EQ(1.0, x.sample(grid.frame_start(0)));
    EXPECT_DOUBLE_EQ(-2.0, y.sample(grid.frame_start(0)));
    EXPECT_DOUBLE_EQ(0.0, x.sample(grid.frame_start(2)));
    EXPECT_DOUBLE_EQ(-2.0, y.sample(grid.frame_start(2)));
    EXPECT_DOUBLE_EQ(3.0, x.sample(grid.frame_start(4)));
    for (int component = 0; component < 2; ++component)
    {
        const timeline::Curve &zero =
            std::get<timeline::Curve>(result.document->lanes()[component + 2].items().front());
        EXPECT_DOUBLE_EQ(component == 0 ? 1.0 : -2.0, zero.sample(grid.frame_start(3)));
    }
}

TEST(AnimationImport, samplesLissajousLikeParanimatorAndPreservesTheRecipe)
{
    const JsonImportResult result = import_timeline_json("fixtures/lissajous-path.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.diagnostics.empty());
    ASSERT_EQ(2, result.document->lane_count());
    EXPECT_EQ(1, result.document->track_count());
    EXPECT_EQ(0, result.document->keyframe_count());
    const timeline::FrameGrid &grid = *result.document->frame_grid();
    const std::array<double, 5> x{2.0, 0.0, -2.0, 0.0, 2.0};
    const std::array<double, 5> y{0.0, 1.0, 0.0, -1.0, 0.0};
    std::ifstream golden_file("fixtures/gold-lissajous-path.par");
    ASSERT_TRUE(golden_file);
    const std::string golden{std::istreambuf_iterator<char>(golden_file), std::istreambuf_iterator<char>()};
    for (int frame = 0; frame < 5; ++frame)
    {
        const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, frame);
        ASSERT_TRUE(inspection);
        ASSERT_EQ(2, timeline::size_cast(inspection->lanes));
        for (int component = 0; component < 2; ++component)
        {
            const timeline::LaneInspection &lane = inspection->lanes[component];
            ASSERT_EQ(1, timeline::size_cast(lane.items));
            ASSERT_TRUE(lane.items.front().value);
            EXPECT_DOUBLE_EQ(component == 0 ? x[frame] : y[frame], *lane.items.front().value);
            EXPECT_EQ(timeline::InspectionItemRole::SAMPLED, lane.items.front().role);
            EXPECT_EQ("params.c", lane.items.front().attributes.at("parameter"));
            EXPECT_EQ("animation-0", lane.items.front().attributes.at("track"));
            EXPECT_EQ(std::to_string(component), lane.items.front().attributes.at("component"));
            EXPECT_EQ("{\"center\":\"0/0\",\"kind\":\"lissajous\",\"x-frequency\":1,\"x-radius\":2,"
                      "\"y-frequency\":1,\"y-radius\":1}",
                lane.items.front().attributes.at("path"));
        }
        const std::size_t start = golden.find("frame-000" + std::to_string(frame + 1) + " {");
        ASSERT_NE(std::string::npos, start);
        const std::string entry = golden.substr(start, golden.find('}', start) - start);
        EXPECT_NE(std::string::npos,
            entry.find("params=" + std::to_string(static_cast<int>(x[frame])) + "/" +
                std::to_string(static_cast<int>(y[frame]))));
    }
    const timeline::Curve &curve = std::get<timeline::Curve>(result.document->lanes()[0].items().front());
    EXPECT_EQ(timeline::CurveInterpolation::ANALYTIC, curve.interpolation());
    EXPECT_EQ(0, curve.sample_count());
    EXPECT_NEAR(std::sqrt(2.0),
        curve.sample(grid.offset() + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2)), 1e-12);
    const timeline::Layout layout(*result.document, timeline::Viewport(500, 140, grid.offset(), grid.end_time()),
        timeline::LayoutMetrics(100, 20, 30, 4));
    int curve_count = 0;
    for (const timeline::Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<timeline::Polyline>(primitive))
        {
            const timeline::Polyline &line = std::get<timeline::Polyline>(primitive);
            if (line.style == timeline::StyleRole::CURVE)
            {
                ++curve_count;
                EXPECT_EQ(5, timeline::size_cast(line.points));
                const std::optional<timeline::HitResult> hit = layout.hit_test(line.points[2], 2);
                ASSERT_TRUE(hit);
                EXPECT_EQ(line.id.item_id, hit->id.item_id);
            }
        }
    }
    EXPECT_EQ(2, curve_count);
}

TEST(AnimationImport, keepsLissajousFrequenciesIndependentAndPhaseOnXOnly)
{
    const timeline::Document document = []
    {
        const JsonImportResult imported = import_timeline_json("fixtures/lissajous-phase.json");
        if (!imported.succeeded() || !imported.diagnostics.empty())
        {
            throw std::runtime_error("lissajous import failed");
        }
        return *imported.document;
    }();
    ASSERT_EQ(2, document.lane_count());
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Curve &x = std::get<timeline::Curve>(document.lanes()[0].items().front());
    const timeline::Curve &y = std::get<timeline::Curve>(document.lanes()[1].items().front());
    EXPECT_NEAR(2.0, x.sample(grid.frame_start(0)), 1e-12);
    EXPECT_DOUBLE_EQ(-2.0, y.sample(grid.frame_start(0)));
    EXPECT_NEAR(1.0 + (std::sqrt(2.0) - std::sqrt(6.0)) / 2.0, x.sample(grid.frame_start(1)), 1e-12);
    EXPECT_NEAR(-2.0 + 3.0 * std::sqrt(2.0) / 2.0, y.sample(grid.frame_start(1)), 1e-12);
    EXPECT_NEAR(1.0 - std::sqrt(3.0), x.sample(grid.frame_start(2)), 1e-12);
    EXPECT_DOUBLE_EQ(-5.0, y.sample(grid.frame_start(2)));
    EXPECT_DOUBLE_EQ(0.0, x.sample(grid.frame_start(4)));
    EXPECT_NEAR(-2.0, y.sample(grid.frame_start(4)), 1e-12);
    const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
    ASSERT_TRUE(music.succeeded());
    const timeline::Document combined = timeline::combine_documents(*music.document, document);
    ASSERT_EQ(6, combined.lane_count());
    const timeline::Curve &copy = std::get<timeline::Curve>(combined.lanes()[4].items().front());
    EXPECT_NEAR(1.0 - std::sqrt(3.0), copy.sample(grid.frame_start(2)), 1e-12);
    EXPECT_EQ(x.attributes(), copy.attributes());
}

TEST(AnimationImport, diagnosesInvalidLissajousRecipesAndAcceptsZeroRadii)
{
    const JsonImportResult result = import_timeline_json("fixtures/partial-lissajous-paths.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(8, timeline::size_cast(result.diagnostics));
    ASSERT_EQ(2, result.document->lane_count());
    for (int index = 0; index < 8; ++index)
    {
        EXPECT_NE(std::string::npos, result.diagnostics[index].find("animation-" + std::to_string(index)));
    }
    const timeline::Curve &x = std::get<timeline::Curve>(result.document->lanes()[0].items().front());
    const timeline::Curve &y = std::get<timeline::Curve>(result.document->lanes()[1].items().front());
    EXPECT_EQ("animation-8[0]", result.document->strings().lookup(result.document->lanes()[0].id()));
    EXPECT_DOUBLE_EQ(1.0, x.sample(result.document->frame_grid()->frame_start(3)));
    EXPECT_DOUBLE_EQ(-2.0, y.sample(result.document->frame_grid()->frame_start(3)));
}

TEST(AnimationImport, preservesAndSamplesAnalyticEllipseRecipes)
{
    const JsonImportResult result = import_timeline_json("fixtures/ellipse-path.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.diagnostics.empty());
    ASSERT_EQ(2, result.document->lane_count());
    EXPECT_EQ(1, result.document->track_count());
    EXPECT_EQ(0, result.document->keyframe_count());
    const timeline::FrameGrid &grid = *result.document->frame_grid();
    const std::array<double, 5> x{2.0, 0.0, -2.0, 0.0, 2.0};
    const std::array<double, 5> y{0.0, 1.0, 0.0, -1.0, 0.0};
    std::ifstream golden_file("fixtures/gold-ellipse-path.par");
    ASSERT_TRUE(golden_file);
    const std::string golden{std::istreambuf_iterator<char>(golden_file), std::istreambuf_iterator<char>()};
    for (int frame = 0; frame < 5; ++frame)
    {
        const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, frame);
        ASSERT_TRUE(inspection);
        ASSERT_EQ(2, timeline::size_cast(inspection->lanes));
        for (int component = 0; component < 2; ++component)
        {
            const timeline::LaneInspection &lane = inspection->lanes[component];
            ASSERT_EQ(1, timeline::size_cast(lane.items));
            ASSERT_TRUE(lane.items.front().value);
            EXPECT_DOUBLE_EQ(component == 0 ? x[frame] : y[frame], *lane.items.front().value);
            EXPECT_EQ(timeline::InspectionItemRole::SAMPLED, lane.items.front().role);
            EXPECT_NE(std::string::npos, lane.items.front().attributes.at("path").find("ellipse"));
        }
        const std::size_t start = golden.find("frame-000" + std::to_string(frame + 1) + " {");
        ASSERT_NE(std::string::npos, start);
        const std::string entry = golden.substr(start, golden.find('}', start) - start);
        EXPECT_NE(std::string::npos,
            entry.find("params=" + std::to_string(static_cast<int>(x[frame])) + "/" +
                std::to_string(static_cast<int>(y[frame]))));
    }
    const timeline::Curve &curve = std::get<timeline::Curve>(result.document->lanes()[0].items().front());
    EXPECT_EQ(0, curve.sample_count());
    EXPECT_NEAR(std::sqrt(2.0),
        curve.sample(grid.offset() + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2)), 1e-12);
    const timeline::Layout layout(*result.document, timeline::Viewport(500, 140, grid.offset(), grid.end_time()),
        timeline::LayoutMetrics(100, 20, 30, 4));
    int curve_count = 0;
    for (const timeline::Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<timeline::Polyline>(primitive))
        {
            const timeline::Polyline &line = std::get<timeline::Polyline>(primitive);
            if (line.style == timeline::StyleRole::CURVE)
            {
                ++curve_count;
                EXPECT_EQ(5, timeline::size_cast(line.points));
                const std::optional<timeline::HitResult> hit = layout.hit_test(line.points[1], 2);
                ASSERT_TRUE(hit);
                EXPECT_EQ(line.id.item_id, hit->id.item_id);
            }
        }
    }
    EXPECT_EQ(2, curve_count);
}

TEST(AnimationImport, honorsCirclePhaseReverseTurnsAndDocumentOwnership)
{
    const timeline::Document document = []
    {
        const JsonImportResult imported = import_timeline_json("fixtures/circle-path.json");
        if (!imported.succeeded())
        {
            throw std::runtime_error("circle import failed");
        }
        return *imported.document;
    }();
    ASSERT_EQ(2, document.lane_count());
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Curve &x = std::get<timeline::Curve>(document.lanes()[0].items().front());
    const timeline::Curve &y = std::get<timeline::Curve>(document.lanes()[1].items().front());
    EXPECT_NEAR(1.0 + std::sqrt(2.0), x.sample(grid.frame_start(1)), 1e-12);
    EXPECT_NEAR(-2.0 + std::sqrt(2.0), y.sample(grid.frame_start(1)), 1e-12);
    EXPECT_DOUBLE_EQ(3.0, x.sample(grid.frame_start(2)));
    EXPECT_DOUBLE_EQ(-2.0, y.sample(grid.frame_start(2)));
    EXPECT_NEAR(1.0, x.sample(grid.frame_start(4)), 1e-12);
    EXPECT_DOUBLE_EQ(-4.0, y.sample(grid.frame_start(4)));
    const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
    ASSERT_TRUE(music.succeeded());
    const timeline::Document combined = timeline::combine_documents(*music.document, document);
    ASSERT_EQ(6, combined.lane_count());
    const timeline::Curve &copy = std::get<timeline::Curve>(combined.lanes()[4].items().front());
    EXPECT_DOUBLE_EQ(3.0, copy.sample(grid.frame_start(2)));
    EXPECT_EQ(x.attributes(), copy.attributes());
}

TEST(AnimationImport, diagnosesInvalidPlanarPathsAndKeepsZeroRadiusDefaults)
{
    const JsonImportResult result = import_timeline_json("fixtures/partial-planar-paths.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(10, timeline::size_cast(result.diagnostics));
    ASSERT_EQ(2, result.document->lane_count());
    for (int index = 0; index < 10; ++index)
    {
        EXPECT_NE(std::string::npos, result.diagnostics[index].find("animation-" + std::to_string(index)));
    }
    const timeline::Curve &x = std::get<timeline::Curve>(result.document->lanes()[0].items().front());
    const timeline::Curve &y = std::get<timeline::Curve>(result.document->lanes()[1].items().front());
    EXPECT_EQ("animation-10[0]", result.document->strings().lookup(result.document->lanes()[0].id()));
    EXPECT_DOUBLE_EQ(1.0, x.sample(result.document->frame_grid()->frame_start(3)));
    EXPECT_DOUBLE_EQ(-2.0, y.sample(result.document->frame_grid()->frame_start(3)));
}

TEST(AnimationImport, samplesConstantAndLinePathsLikeParanimator)
{
    const JsonImportResult result = import_timeline_json("fixtures/path-generators.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.diagnostics.empty());
    ASSERT_EQ(3, result.document->lane_count());
    EXPECT_EQ(2, result.document->track_count());
    EXPECT_EQ(0, result.document->keyframe_count());
    const timeline::FrameGrid &grid = *result.document->frame_grid();
    std::ifstream golden_file("fixtures/gold-path-generators.par");
    ASSERT_TRUE(golden_file);
    const std::string golden{std::istreambuf_iterator<char>(golden_file), std::istreambuf_iterator<char>()};
    for (int frame = 0; frame < 3; ++frame)
    {
        const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, frame);
        ASSERT_TRUE(inspection);
        ASSERT_EQ(3, timeline::size_cast(inspection->lanes));
        ASSERT_TRUE(inspection->lanes[0].value);
        ASSERT_TRUE(inspection->lanes[1].value);
        ASSERT_TRUE(inspection->lanes[2].value);
        EXPECT_DOUBLE_EQ(321.0, *inspection->lanes[0].value);
        EXPECT_DOUBLE_EQ(static_cast<double>(frame), *inspection->lanes[1].value);
        EXPECT_DOUBLE_EQ(static_cast<double>(frame + 1), *inspection->lanes[2].value);
        const std::string entry = "frame-000" + std::to_string(frame + 1) + " {";
        const std::size_t start = golden.find(entry);
        ASSERT_NE(std::string::npos, start);
        const std::string reference = golden.substr(start, golden.find('}', start) - start);
        EXPECT_NE(std::string::npos, reference.find("maxiter=321"));
        EXPECT_NE(
            std::string::npos, reference.find("params=" + std::to_string(frame) + "/" + std::to_string(frame + 1)));
    }
    const timeline::Lane &line = result.document->lanes()[1];
    EXPECT_DOUBLE_EQ(0.5,
        *line.evaluate_keyframes(
            grid.frame_start(0) + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2)));
    const timeline::Keyframe &constant = std::get<timeline::Keyframe>(result.document->lanes()[0].items().front());
    EXPECT_EQ(timeline::KeyframeInterpolation::HOLD, constant.interpolation());
    EXPECT_EQ("{\"kind\":\"constant\",\"value\":\"321\"}", constant.attributes().at("path"));
    for (const timeline::Item &item : line.items())
    {
        const timeline::Keyframe &key = std::get<timeline::Keyframe>(item);
        EXPECT_EQ("{\"from\":\"0/1\",\"kind\":\"line\",\"to\":\"2/3\"}", key.attributes().at("path"));
        EXPECT_EQ("params.c", key.attributes().at("parameter"));
    }
    const timeline::Layout layout(*result.document, timeline::Viewport(400, 160, grid.offset(), grid.end_time()),
        timeline::LayoutMetrics(100, 20, 30, 4));
    const std::string snapshot = timeline::render_snapshot(layout.display_list());
    EXPECT_NE(std::string::npos, snapshot.find("params.c[0]"));
    EXPECT_NE(std::string::npos, snapshot.find("params.c[1]"));
    EXPECT_NE(std::string::npos, snapshot.find("KEYFRAME_MARKER"));
}

TEST(AnimationImport, diagnosesInvalidPathsWithoutDiscardingAValidConstant)
{
    const JsonImportResult result = import_timeline_json("fixtures/partial-paths.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(7, timeline::size_cast(result.diagnostics));
    ASSERT_EQ(1, result.document->lane_count());
    EXPECT_EQ("animation-7", result.document->strings().lookup(result.document->lanes().front().id()));
    for (int index = 0; index < 7; ++index)
    {
        EXPECT_NE(std::string::npos, result.diagnostics[index].find("animation-" + std::to_string(index)));
    }
    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, 1);
    ASSERT_TRUE(inspection);
    ASSERT_EQ(1, timeline::size_cast(inspection->lanes.front().items));
    const timeline::Attributes &attributes = inspection->lanes.front().items.front().attributes;
    EXPECT_EQ("bof60", attributes.at("value"));
    EXPECT_EQ("{\"kind\":\"constant\",\"value\":\"bof60\"}", attributes.at("path"));
}

TEST(AnimationImport, rejectsPathsWithFewerThanTwoFrames)
{
    const JsonImportResult result = import_timeline_json("fixtures/single-frame-path.json");
    EXPECT_FALSE(result.succeeded());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_NE(std::string::npos, result.diagnostics.front().find("at least two frames"));
}

TEST(AnimationImport, translatesDestinationCurveToOutgoingSegment)
{
    const JsonImportResult result = import_timeline_json("fixtures/maxiter-step.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.diagnostics.empty());
    ASSERT_EQ(1, result.document->lane_count());
    const timeline::Lane &lane = result.document->lanes().front();
    const timeline::FrameGrid &grid = *result.document->frame_grid();
    ASSERT_EQ(2, lane.item_count());
    const timeline::Keyframe &first = std::get<timeline::Keyframe>(lane.items().front());
    EXPECT_EQ(timeline::KeyframeInterpolation::HOLD, first.interpolation());
    EXPECT_EQ("step", first.attributes().at("outgoing-curve"));
    EXPECT_DOUBLE_EQ(100.0, *lane.evaluate_keyframes(grid.frame_start(2)));
    EXPECT_DOUBLE_EQ(200.0, *lane.evaluate_keyframes(grid.frame_start(3)));
}

TEST(AnimationImport, splitsCompoundParametersWithoutLosingAuthoredValues)
{
    const JsonImportResult result = import_timeline_json("fixtures/multi-track.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.diagnostics.empty());
    ASSERT_EQ(4, result.document->lane_count());
    EXPECT_EQ(2, result.document->track_count());
    EXPECT_EQ(4, result.document->keyframe_count());
    const timeline::FrameGrid &grid = *result.document->frame_grid();
    const timeline::Lane &magnification = result.document->lanes()[2];
    EXPECT_EQ("center-mag[2]", magnification.label());
    EXPECT_NEAR(std::sqrt(10.0), *magnification.evaluate_keyframes(grid.frame_start(1)), 1e-12);
    const timeline::Keyframe &first = std::get<timeline::Keyframe>(magnification.items().front());
    EXPECT_EQ("-0.5/0/1", first.attributes().at("value"));
    EXPECT_EQ("center-mag", first.attributes().at("parameter"));
    EXPECT_EQ("geometric", first.attributes().at("outgoing-curve"));
    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, 1);
    ASSERT_TRUE(inspection);
    ASSERT_TRUE(inspection->lanes[2].value);
    EXPECT_NEAR(std::sqrt(10.0), *inspection->lanes[2].value, 1e-12);
}

TEST(AnimationImport, retainsLayerIdentityAndDrivesDisplayAndInspection)
{
    const JsonImportResult result = import_timeline_json("fixtures/single-layer.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(1, result.document->lane_count());
    const timeline::Lane &lane = result.document->lanes().front();
    EXPECT_EQ("base / maxiter", lane.label());
    const timeline::Keyframe &first = std::get<timeline::Keyframe>(lane.items().front());
    EXPECT_EQ("base", first.attributes().at("layer"));
    const timeline::FrameGrid &grid = *result.document->frame_grid();
    const timeline::Layout layout(*result.document, timeline::Viewport(400, 120, grid.offset(), grid.end_time()),
        timeline::LayoutMetrics(100, 20, 30, 4));
    const std::string snapshot = timeline::render_snapshot(layout.display_list());
    EXPECT_NE(std::string::npos, snapshot.find("KEYFRAME_MARKER"));
    EXPECT_NE(std::string::npos, snapshot.find("base / maxiter"));
    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, 1);
    ASSERT_TRUE(inspection);
    ASSERT_EQ(1, timeline::size_cast(inspection->lanes));
    ASSERT_FALSE(inspection->lanes.front().items.empty());
    EXPECT_EQ("base", inspection->lanes.front().items.front().attributes.at("layer"));
    EXPECT_DOUBLE_EQ(150.0, *lane.evaluate_keyframes(grid.frame_start(1)));
}

TEST(AnimationImport, combinesMusicGeneratedOutputAndAnimationWithoutIdentityCollisions)
{
    const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
    ASSERT_TRUE(music.succeeded());
    JsonImportOptions options;
    options.frames_per_second_numerator = music.document->frame_grid()->frames_per_second_numerator();
    options.frames_per_second_denominator = music.document->frame_grid()->frames_per_second_denominator();
    const JsonImportResult animation = import_timeline_json("fixtures/maxiter.json", options);
    ASSERT_TRUE(animation.succeeded());
    const timeline::Document combined = timeline::combine_documents(*music.document, *animation.document);
    ASSERT_EQ(5, combined.lane_count());
    EXPECT_EQ(5, combined.frame_grid()->frame_count());
    EXPECT_EQ(11, combined.keyframe_count());
    EXPECT_EQ(music.document->lanes().front().id(), combined.lanes().front().id());
    const timeline::Lane &authored = combined.lanes().back();
    EXPECT_EQ("maxiter", authored.label());
    EXPECT_DOUBLE_EQ(150.0, *authored.evaluate_keyframes(combined.frame_grid()->frame_start(1)));
    const timeline::Document repeated = timeline::combine_documents(combined, *animation.document);
    ASSERT_EQ(6, repeated.lane_count());
    EXPECT_NE(repeated.lanes()[4].id(), repeated.lanes()[5].id());
    EXPECT_EQ(std::get<timeline::Keyframe>(animation.document->lanes().front().items().front()).time(),
        std::get<timeline::Keyframe>(authored.items().front()).time());
    const timeline::Layout layout(combined,
        timeline::Viewport(600, 240, combined.frame_grid()->offset(), combined.frame_grid()->end_time()),
        timeline::LayoutMetrics(120, 20, 30, 4));
    const std::string snapshot = timeline::render_snapshot(layout.display_list());
    EXPECT_NE(std::string::npos, snapshot.find("RMS"));
    EXPECT_NE(std::string::npos, snapshot.find("camera.zoom"));
    EXPECT_NE(std::string::npos, snapshot.find("maxiter"));
}

TEST(AnimationImport, rejectsIncompatibleComparisonTiming)
{
    const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
    JsonImportOptions options;
    options.frames_per_second_numerator = 10;
    const JsonImportResult animation = import_timeline_json("fixtures/maxiter.json", options);
    ASSERT_TRUE(music.succeeded());
    ASSERT_TRUE(animation.succeeded());
    EXPECT_THROW(timeline::combine_documents(*music.document, *animation.document), std::invalid_argument);
}

TEST(AnimationImport, preservesCategoricalValuesAsHeldSpansAndKeyInstants)
{
    const JsonImportResult result = import_timeline_json("fixtures/inside-hold.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.diagnostics.empty());
    ASSERT_EQ(1, result.document->lane_count());
    EXPECT_EQ(2, result.document->keyframe_count());
    const timeline::Lane &lane = result.document->lanes().front();
    EXPECT_EQ("keyframes", lane.kind());
    ASSERT_EQ(4, lane.item_count());
    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, 1);
    ASSERT_TRUE(inspection);
    ASSERT_EQ(1, timeline::size_cast(inspection->lanes.front().items));
    const timeline::InspectionItem &held = inspection->lanes.front().items.front();
    EXPECT_FALSE(held.value);
    EXPECT_EQ("bof60", held.attributes.at("value"));
    EXPECT_EQ("hold", held.attributes.at("outgoing-curve"));
    const std::optional<timeline::FrameInspection> final = timeline::inspect_frame(*result.document, 2);
    ASSERT_TRUE(final);
    for (const timeline::InspectionItem &item : final->lanes.front().items)
    {
        EXPECT_EQ("zmag", item.attributes.at("value"));
    }
}

TEST(AnimationImport, retainsValidTracksWithIndexedDiagnosticsForInvalidKeys)
{
    const JsonImportResult result = import_timeline_json("fixtures/partial-animation.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(1, result.document->lane_count());
    ASSERT_EQ(6, timeline::size_cast(result.diagnostics));
    for (int index = 0; index < 6; ++index)
    {
        EXPECT_NE(std::string::npos, result.diagnostics[index].find("animation-" + std::to_string(index)));
    }
    EXPECT_EQ("animation-6", result.document->strings().lookup(result.document->lanes().front().id()));
}

TEST(AnimationImport, rejectsUnusableAnimationAndMissingCatalogs)
{
    for (const char *name : {"invalid-animation.json", "missing-catalog.json"})
    {
        const JsonImportResult result = import_timeline_json(std::filesystem::path("fixtures") / name);
        EXPECT_FALSE(result.succeeded()) << name;
        EXPECT_FALSE(result.diagnostics.empty()) << name;
    }
}
