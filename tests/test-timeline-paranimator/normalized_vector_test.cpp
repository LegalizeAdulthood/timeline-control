// Copyright (c) 2026 Richard Thomson

#include <timelineParAnimator/TimelineJson.h>

#include <timeline/Layout.h>
#include <timeline/Query.h>
#include <timeline/size_cast.h>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <fstream>
#include <iterator>
#include <stdexcept>

using namespace timeline_par_animator;

namespace
{

std::string vector_golden_entry(const std::string &golden, int frame)
{
    const std::string name = "frame-000" + std::to_string(frame + 1) + " {";
    const std::size_t start = golden.find(name);
    return start == std::string::npos ? std::string{} : golden.substr(start, golden.find('}', start) - start);
}

double vector_golden_component(const std::string &entry, const std::string &parameter, int component)
{
    const std::string name = "\n    " + parameter + "=";
    std::size_t start = entry.find(name);
    if (start == std::string::npos)
    {
        throw std::runtime_error("missing source golden parameter");
    }
    start += name.size();
    for (int index = 0; index < component; ++index)
    {
        start = entry.find('/', start) + 1;
    }
    return std::stod(entry.substr(start));
}

} // namespace

TEST(NormalizedVector, matchesExtremeSourceOutputAtFramesAndBetweenFrames)
{
    const JsonImportResult imported = import_timeline_json("fixtures/extreme-normalized-vectors.json");
    ASSERT_TRUE(imported.succeeded());
    ASSERT_TRUE(imported.diagnostics.empty());
    ASSERT_EQ(25, imported.document->lane_count());
    std::ifstream input("fixtures/gold-extreme-normalized-vectors.par");
    ASSERT_TRUE(input);
    const std::string golden{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    const timeline::FrameGrid &grid = *imported.document->frame_grid();
    for (int sample = 0; sample < 9; ++sample)
    {
        SCOPED_TRACE(sample);
        const timeline::Time time =
            grid.offset() + timeline::Duration::from_ticks(sample * grid.frame_duration().ticks() / 2);
        const std::string entry = vector_golden_entry(golden, sample);
        ASSERT_FALSE(entry.empty());
        for (int index = 0; index < imported.document->lane_count(); ++index)
        {
            SCOPED_TRACE(index);
            const timeline::Lane &lane = imported.document->lanes()[index];
            const bool keyed = std::holds_alternative<timeline::Keyframe>(lane.items()[0]);
            const timeline::Attributes &attributes = keyed ? std::get<timeline::Keyframe>(lane.items()[0]).attributes()
                                                           : std::get<timeline::Curve>(lane.items()[0]).attributes();
            const double expected =
                vector_golden_component(entry, attributes.at("parameter"), std::stoi(attributes.at("component")));
            const double actual =
                keyed ? *lane.evaluate_keyframe_output(time) : std::get<timeline::Curve>(lane.items()[0]).sample(time);
            EXPECT_NEAR(expected, actual, std::abs(expected) * 1e-11);
            if (expected == 0)
            {
                EXPECT_EQ(std::signbit(expected), std::signbit(actual));
            }
            if (sample % 2 == 0)
            {
                const timeline::FrameInspection inspection = *timeline::inspect_frame(*imported.document, sample / 2);
                EXPECT_DOUBLE_EQ(
                    actual, keyed ? *inspection.lanes[index].output_value : *inspection.lanes[index].items[0].value);
            }
        }
    }
}

TEST(NormalizedVector, retainsExtremeAuthoredValuesRecipesBoundsAndHitIdentities)
{
    JsonImportResult imported = import_timeline_json("fixtures/extreme-normalized-vectors.json");
    ASSERT_TRUE(imported.succeeded());
    ASSERT_TRUE(imported.diagnostics.empty());
    ASSERT_EQ(25, imported.document->lane_count());
    const timeline::Document document = *imported.document;
    imported.document.reset();
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Time middle = grid.frame_start(2);
    const timeline::Keyframe &key = std::get<timeline::Keyframe>(document.lanes()[0].items()[0]);
    EXPECT_DOUBLE_EQ(1e200, key.value());
    EXPECT_DOUBLE_EQ(1.5e200, *document.lanes()[0].evaluate_keyframes(middle));
    EXPECT_DOUBLE_EQ(0, *document.lanes()[0].evaluate_keyframe_output(middle));
    EXPECT_DOUBLE_EQ(5e-13, *document.lanes()[6].evaluate_keyframes(middle));
    EXPECT_GT(*document.lanes()[6].evaluate_keyframe_output(middle), 0);
    const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
    ASSERT_TRUE(music.succeeded());
    const timeline::Document combined = timeline::combine_documents(document, *music.document);
    ASSERT_EQ(29, combined.lane_count());
    for (int index = 0; index < document.lane_count(); ++index)
    {
        const timeline::Lane &lane = combined.lanes()[index];
        EXPECT_EQ(document.lanes()[index].id(), lane.id());
        if (index < 11)
        {
            const timeline::Keyframe &copy = std::get<timeline::Keyframe>(lane.items()[0]);
            EXPECT_EQ(std::get<timeline::Keyframe>(document.lanes()[index].items()[0]).attributes(), copy.attributes());
            EXPECT_DOUBLE_EQ(
                *document.lanes()[index].evaluate_keyframe_output(middle), *lane.evaluate_keyframe_output(middle));
            EXPECT_NE(std::string::npos, copy.attributes().at("track-definition").find("value"));
        }
        else
        {
            const timeline::Curve &curve = std::get<timeline::Curve>(lane.items()[0]);
            EXPECT_EQ(std::get<timeline::Curve>(document.lanes()[index].items()[0]).attributes(), curve.attributes());
            EXPECT_DOUBLE_EQ(-1, *curve.minimum());
            EXPECT_DOUBLE_EQ(1, *curve.maximum());
            EXPECT_NE(std::string::npos, curve.attributes().at("track-definition").find("control-points"));
            EXPECT_DOUBLE_EQ(
                std::get<timeline::Curve>(document.lanes()[index].items()[0]).sample(middle), curve.sample(middle));
        }
    }
    const timeline::Curve &bezier = std::get<timeline::Curve>(combined.lanes()[11].items()[0]);
    EXPECT_NE(std::string::npos, bezier.attributes().at("path").find("1e200/1"));
    EXPECT_NE(std::string::npos, bezier.attributes().at("catalog-definition").find("vector2"));
    const timeline::Layout layout(combined, timeline::Viewport(600, 1300, grid.offset(), grid.end_time()),
        timeline::LayoutMetrics(100, 20, 40, 4));
    int found = 0;
    for (const timeline::Primitive &primitive : layout.display_list().primitives())
    {
        if (!std::holds_alternative<timeline::Polyline>(primitive))
        {
            continue;
        }
        const timeline::Polyline &line = std::get<timeline::Polyline>(primitive);
        if (layout.display_list().strings().lookup(line.id.lane_id).substr(0, 10) != "animation-")
        {
            continue;
        }
        for (const timeline::Point &point : line.points)
        {
            EXPECT_TRUE(std::isfinite(point.x));
            EXPECT_TRUE(std::isfinite(point.y));
        }
        const std::optional<timeline::HitResult> hit = layout.hit_test(line.points.front(), 0);
        ASSERT_TRUE(hit);
        EXPECT_EQ(line.id.lane_id, hit->id.lane_id);
        EXPECT_FALSE(hit->id.item_id.empty());
        ++found;
    }
    EXPECT_EQ(25, found);
}

TEST(NormalizedVector, rejectsExtremeSingularAndUnresolvedIntervalsNotOverflowAlone)
{
    const JsonImportResult imported = import_timeline_json("fixtures/extreme-normalized-invalid.json");
    EXPECT_FALSE(imported.succeeded());
    ASSERT_EQ(8, timeline::size_cast(imported.diagnostics));
    for (int index = 0; index < 7; ++index)
    {
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find("animation-" + std::to_string(index) + ":"));
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find("normalization"));
    }
    EXPECT_NE(std::string::npos, imported.diagnostics[0].find("nonzero"));
    EXPECT_NE(std::string::npos, imported.diagnostics[1].find("nonzero"));
    EXPECT_NE(std::string::npos, imported.diagnostics[2].find("singular"));
    EXPECT_NE(std::string::npos, imported.diagnostics[3].find("singular"));
    EXPECT_NE(std::string::npos, imported.diagnostics[4].find("singular"));
    EXPECT_NE(std::string::npos, imported.diagnostics[5].find("unresolved"));
    EXPECT_NE(std::string::npos, imported.diagnostics[6].find("nonzero"));
}

TEST(NormalizedVector, matchesSourceKeyedOutputWithoutCleaningAuthoredComponents)
{
    const JsonImportResult imported = import_timeline_json("fixtures/normalized-keyed-vectors.json");
    ASSERT_TRUE(imported.succeeded());
    ASSERT_TRUE(imported.diagnostics.empty());
    ASSERT_EQ(17, imported.document->lane_count());
    std::ifstream input("fixtures/gold-normalized-keyed-vectors.par");
    ASSERT_TRUE(input);
    const std::string golden{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    const std::array<std::string, 17> parameters{"direction2", "direction2", "direction3", "direction3", "direction3",
        "hull2", "hull2", "line3", "line3", "line3", "tiny2", "tiny2", "raw3", "raw3", "raw3", "raw2", "raw2"};
    const std::array<int, 17> components{0, 1, 0, 1, 2, 0, 1, 0, 1, 2, 0, 1, 0, 1, 2, 0, 1};
    for (int frame = 0; frame < 5; ++frame)
    {
        SCOPED_TRACE(frame);
        const std::string entry = vector_golden_entry(golden, frame);
        ASSERT_FALSE(entry.empty());
        const timeline::FrameInspection inspection = *timeline::inspect_frame(*imported.document, frame);
        for (int lane = 0; lane < 17; ++lane)
        {
            SCOPED_TRACE(lane);
            const std::optional<double> value =
                lane < 12 ? inspection.lanes[lane].output_value : inspection.lanes[lane].value;
            ASSERT_TRUE(value);
            EXPECT_NEAR(vector_golden_component(entry, parameters[lane], components[lane]), *value, 1e-11);
            EXPECT_EQ(lane < 12, inspection.lanes[lane].output_value.has_value());
        }
    }
    const timeline::FrameInspection middle = *timeline::inspect_frame(*imported.document, 2);
    EXPECT_DOUBLE_EQ(5, *middle.lanes[0].value);
    EXPECT_DOUBLE_EQ(10, *middle.lanes[1].value);
    EXPECT_NEAR(1 / std::sqrt(17.0), *middle.lanes[10].output_value, 1e-14);
    EXPECT_DOUBLE_EQ(5e-13, *middle.lanes[10].value);
    EXPECT_EQ("true", middle.lanes[0].items[0].attributes.at("normalize"));
    EXPECT_EQ("0", middle.lanes[0].items[0].attributes.at("component"));
}

TEST(NormalizedVector, retainsKeyedOutputAfterCopyingComparisonAndLayout)
{
    JsonImportResult imported = import_timeline_json("fixtures/normalized-keyed-vectors.json");
    ASSERT_TRUE(imported.succeeded());
    const timeline::Document document = *imported.document;
    imported.document.reset();
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Time half = grid.offset() + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2);
    const timeline::Lane &lane = document.lanes()[0];
    EXPECT_DOUBLE_EQ(8.75, *lane.evaluate_keyframes(half));
    EXPECT_NEAR(8.75 / std::sqrt(8.75 * 8.75 + 2.5 * 2.5), *lane.evaluate_keyframe_output(half), 1e-14);
    const timeline::Keyframe &key = std::get<timeline::Keyframe>(lane.items()[0]);
    EXPECT_DOUBLE_EQ(10, key.value());
    EXPECT_NE(std::string::npos, key.attributes().at("track-definition").find("direction2"));
    EXPECT_NE(std::string::npos, key.attributes().at("catalog-definition").find("vector2"));
    const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
    ASSERT_TRUE(music.succeeded());
    const timeline::Document combined = timeline::combine_documents(document, *music.document);
    ASSERT_EQ(21, combined.lane_count());
    EXPECT_DOUBLE_EQ(*lane.evaluate_keyframe_output(half), *combined.lanes()[0].evaluate_keyframe_output(half));
    const timeline::Layout layout(combined, timeline::Viewport(600, 900, grid.offset(), grid.end_time()),
        timeline::LayoutMetrics(100, 20, 40, 4));
    bool found = false;
    for (const timeline::Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<timeline::Polyline>(primitive))
        {
            const timeline::Polyline &line = std::get<timeline::Polyline>(primitive);
            if (layout.display_list().strings().lookup(line.id.lane_id) == "animation-0[0]")
            {
                ASSERT_EQ(2, timeline::size_cast(line.points));
                const std::optional<timeline::HitResult> hit = layout.hit_test(line.points.front(), 0);
                ASSERT_TRUE(hit);
                EXPECT_EQ("animation-0[0]", layout.display_list().strings().lookup(hit->id.lane_id));
                EXPECT_EQ("animation-0-key-0", layout.display_list().strings().lookup(hit->id.item_id));
                found = true;
            }
        }
    }
    EXPECT_TRUE(found);
}

TEST(NormalizedVector, diagnosesInvalidKeyedInputsWithoutPartialComponents)
{
    const JsonImportResult imported = import_timeline_json("fixtures/normalized-keyed-partial.json");
    ASSERT_TRUE(imported.succeeded());
    ASSERT_EQ(13, timeline::size_cast(imported.diagnostics));
    ASSERT_EQ(4, imported.document->lane_count());
    EXPECT_EQ("animation-3[0]", imported.document->strings().lookup(imported.document->lanes()[0].id()));
    EXPECT_EQ("animation-14[0]", imported.document->strings().lookup(imported.document->lanes()[2].id()));
    const std::array<int, 13> rejected{0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
    for (int index = 0; index < 13; ++index)
    {
        EXPECT_NE(
            std::string::npos, imported.diagnostics[index].find("animation-" + std::to_string(rejected[index]) + ":"));
    }
    EXPECT_NE(std::string::npos, imported.diagnostics[0].find("normalization"));
    EXPECT_NE(std::string::npos, imported.diagnostics[1].find("numeric array"));
    EXPECT_NE(std::string::npos, imported.diagnostics[2].find("numeric array"));
    EXPECT_NE(std::string::npos, imported.diagnostics[3].find("normalization"));
    EXPECT_NE(std::string::npos, imported.diagnostics[4].find("arity"));
    EXPECT_NE(std::string::npos, imported.diagnostics[5].find("curves"));
    EXPECT_NE(std::string::npos, imported.diagnostics[6].find("full frame range"));
    EXPECT_NE(std::string::npos, imported.diagnostics[7].find("numeric array"));
    EXPECT_NE(std::string::npos, imported.diagnostics[8].find("bounds"));
    for (int index = 9; index < 13; ++index)
    {
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find("extrapolation"));
    }
    EXPECT_DOUBLE_EQ(0, *timeline::inspect_frame(*imported.document, 3)->lanes[0].output_value);
    EXPECT_DOUBLE_EQ(1, *timeline::inspect_frame(*imported.document, 3)->lanes[2].output_value);
    EXPECT_DOUBLE_EQ(-1, *timeline::inspect_frame(*imported.document, 4)->lanes[2].output_value);
    EXPECT_FALSE(import_timeline_json("fixtures/normalized-keyed-invalid.json").succeeded());
}

TEST(NormalizedVector, matchesSourceControlPointOutputAtEveryFrame)
{
    const std::array<std::string, 2> fixtures{"normalized-bezier-vectors", "normalized-catmull-rom-vectors"};
    const std::array<std::string, 12> parameters{"direction2", "direction2", "direction3", "direction3", "direction3",
        "raw3", "raw3", "raw3", "tiny2", "tiny2", "hull2", "hull2"};
    const std::array<int, 12> components{0, 1, 0, 1, 2, 0, 1, 2, 0, 1, 0, 1};
    for (const std::string &fixture : fixtures)
    {
        SCOPED_TRACE(fixture);
        const JsonImportResult imported = import_timeline_json("fixtures/" + fixture + ".json");
        ASSERT_TRUE(imported.succeeded());
        ASSERT_TRUE(imported.diagnostics.empty());
        ASSERT_EQ(fixture == fixtures[0] ? 12 : 8, imported.document->lane_count());
        std::ifstream input("fixtures/gold-" + fixture + ".par");
        ASSERT_TRUE(input);
        const std::string golden{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        for (int frame = 0; frame < imported.document->frame_grid()->frame_count(); ++frame)
        {
            SCOPED_TRACE(frame);
            const std::string entry = vector_golden_entry(golden, frame);
            ASSERT_FALSE(entry.empty());
            const timeline::FrameInspection inspection = *timeline::inspect_frame(*imported.document, frame);
            for (int lane = 0; lane < imported.document->lane_count(); ++lane)
            {
                SCOPED_TRACE(lane);
                ASSERT_EQ(1, timeline::size_cast(inspection.lanes[lane].items));
                ASSERT_TRUE(inspection.lanes[lane].items[0].value);
                EXPECT_NEAR(vector_golden_component(entry, parameters[lane], components[lane]),
                    *inspection.lanes[lane].items[0].value, 1e-11);
            }
        }
    }
}

TEST(NormalizedVector, ownsRecipesBoundsAndHitIdentityAcrossCopyingAndComparison)
{
    JsonImportResult imported = import_timeline_json("fixtures/normalized-bezier-vectors.json");
    ASSERT_TRUE(imported.succeeded());
    const timeline::Document document = *imported.document;
    imported.document.reset();
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Curve &curve = std::get<timeline::Curve>(document.lanes()[0].items()[0]);
    EXPECT_EQ(timeline::CurveInterpolation::ANALYTIC, curve.interpolation());
    EXPECT_EQ(0, curve.sample_count());
    EXPECT_DOUBLE_EQ(-1, *curve.minimum());
    EXPECT_DOUBLE_EQ(1, *curve.maximum());
    EXPECT_EQ("true", curve.attributes().at("normalize"));
    EXPECT_NE(std::string::npos, curve.attributes().at("path").find("10/0"));
    EXPECT_NE(std::string::npos, curve.attributes().at("catalog-definition").find("vector2"));
    EXPECT_NE(std::string::npos, curve.attributes().at("track-definition").find("direction2"));
    EXPECT_EQ("0", curve.attributes().at("component"));
    EXPECT_EQ("animation-0", curve.attributes().at("track"));
    EXPECT_NEAR(1 / std::sqrt(5.0), curve.sample(grid.frame_start(2)), 1e-14);
    const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
    ASSERT_TRUE(music.succeeded());
    const timeline::Document combined = timeline::combine_documents(document, *music.document);
    ASSERT_EQ(16, combined.lane_count());
    const timeline::Curve &copy = std::get<timeline::Curve>(combined.lanes()[0].items()[0]);
    EXPECT_EQ(curve.attributes(), copy.attributes());
    EXPECT_DOUBLE_EQ(curve.sample(grid.frame_start(2)), copy.sample(grid.frame_start(2)));
    const timeline::Layout layout(combined, timeline::Viewport(600, 700, grid.offset(), grid.end_time()),
        timeline::LayoutMetrics(100, 20, 40, 4));
    bool found = false;
    for (const timeline::Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<timeline::Polyline>(primitive))
        {
            const timeline::Polyline &line = std::get<timeline::Polyline>(primitive);
            if (layout.display_list().strings().lookup(line.id.lane_id) == "animation-0[0]")
            {
                ASSERT_EQ(5, timeline::size_cast(line.points));
                const std::optional<timeline::HitResult> hit = layout.hit_test(line.points[2], 0);
                ASSERT_TRUE(hit);
                EXPECT_EQ("animation-0[0]", layout.display_list().strings().lookup(hit->id.lane_id));
                EXPECT_EQ("animation-0[0]-path", layout.display_list().strings().lookup(hit->id.item_id));
                found = true;
            }
        }
    }
    EXPECT_TRUE(found);
}

TEST(NormalizedVector, normalizesAfterInterpolationAndSourceCleanupAtFractionalTimes)
{
    const JsonImportResult imported =
        import_timeline_json("fixtures/normalized-bezier-vectors.json", JsonImportOptions{48000, 24000, 1001, {}});
    ASSERT_TRUE(imported.succeeded());
    const timeline::FrameGrid &grid = *imported.document->frame_grid();
    const timeline::Time half = grid.offset() + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2);
    const timeline::Curve &x = std::get<timeline::Curve>(imported.document->lanes()[0].items()[0]);
    const timeline::Curve &y = std::get<timeline::Curve>(imported.document->lanes()[1].items()[0]);
    const double length = std::sqrt(8.75 * 8.75 + 2.5 * 2.5);
    EXPECT_NEAR(8.75 / length, x.sample(half), 1e-14);
    EXPECT_NEAR(2.5 / length, y.sample(half), 1e-14);
    for (int frame = 0; frame < grid.frame_count(); ++frame)
    {
        EXPECT_DOUBLE_EQ(
            0, std::get<timeline::Curve>(imported.document->lanes()[8].items()[0]).sample(grid.frame_start(frame)));
        EXPECT_DOUBLE_EQ(
            1, std::get<timeline::Curve>(imported.document->lanes()[9].items()[0]).sample(grid.frame_start(frame)));
    }
    EXPECT_DOUBLE_EQ(
        1.5, std::get<timeline::Curve>(imported.document->lanes()[5].items()[0]).sample(grid.frame_start(2)));
    EXPECT_FALSE(std::get<timeline::Curve>(imported.document->lanes()[5].items()[0]).attributes().count("normalize"));
}

TEST(NormalizedVector, rejectsSingularIntervalsAndMalformedTargetsWithoutPartialLanes)
{
    const JsonImportResult imported = import_timeline_json("fixtures/normalized-vector-partial.json");
    ASSERT_TRUE(imported.succeeded());
    ASSERT_EQ(9, timeline::size_cast(imported.diagnostics));
    ASSERT_EQ(4, imported.document->lane_count());
    EXPECT_EQ("animation-4[0]", imported.document->strings().lookup(imported.document->lanes()[0].id()));
    EXPECT_EQ("animation-10[0]", imported.document->strings().lookup(imported.document->lanes()[2].id()));
    const std::array<int, 9> rejected{0, 1, 2, 3, 5, 6, 7, 8, 9};
    for (int index = 0; index < 9; ++index)
    {
        EXPECT_NE(
            std::string::npos, imported.diagnostics[index].find("animation-" + std::to_string(rejected[index]) + ":"));
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find(index == 4 ? "arity" : "normalization"));
    }
    EXPECT_DOUBLE_EQ(0, *timeline::inspect_frame(*imported.document, 2)->lanes[0].items[0].value);
    EXPECT_FALSE(import_timeline_json("fixtures/normalized-vector-invalid.json").succeeded());
}
