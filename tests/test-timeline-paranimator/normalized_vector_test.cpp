// Copyright (c) 2026 Richard Thomson

#include <timeline/Layout.h>
#include <timeline/Query.h>
#include <timelineParAnimator/TimelineJson.h>

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

TEST(NormalizedVector, matches_source_control_point_output_at_every_frame)
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

TEST(NormalizedVector, owns_recipes_bounds_and_hit_identity_across_copying_and_comparison)
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
            if (line.id.lane_id == "animation-0[0]")
            {
                ASSERT_EQ(5, timeline::size_cast(line.points));
                const std::optional<timeline::HitResult> hit = layout.hit_test(line.points[2], 0);
                ASSERT_TRUE(hit);
                EXPECT_EQ("animation-0[0]", hit->id.lane_id);
                EXPECT_EQ("animation-0[0]-path", hit->id.item_id);
                found = true;
            }
        }
    }
    EXPECT_TRUE(found);
}

TEST(NormalizedVector, normalizes_after_interpolation_and_source_cleanup_at_fractional_times)
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

TEST(NormalizedVector, rejects_singular_intervals_and_malformed_targets_without_partial_lanes)
{
    const JsonImportResult imported = import_timeline_json("fixtures/normalized-vector-partial.json");
    ASSERT_TRUE(imported.succeeded());
    ASSERT_EQ(10, timeline::size_cast(imported.diagnostics));
    ASSERT_EQ(2, imported.document->lane_count());
    EXPECT_EQ("animation-10[0]", imported.document->lanes()[0].id());
    for (int index = 0; index < 10; ++index)
    {
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find("animation-" + std::to_string(index) + ":"));
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find(index == 5 ? "arity" : "normalization"));
    }
    EXPECT_FALSE(import_timeline_json("fixtures/normalized-vector-invalid.json").succeeded());
}
