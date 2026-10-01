// Copyright (c) 2026 Richard Thomson

#include <timeline/Layout.h>
#include <timeline/Query.h>
#include <timeline/Snapshot.h>
#include <timelineParAnimator/TimelineJson.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>

using namespace timeline_par_animator;

namespace
{

std::vector<std::map<std::string, std::string>> julibrot_source_frames(const std::string &fixture)
{
    std::ifstream input("fixtures/gold-julibrot-view-" + fixture + ".par");
    if (!input)
    {
        throw std::runtime_error("Missing Julibrot golden");
    }
    std::vector<std::map<std::string, std::string>> frames;
    for (std::string line; std::getline(input, line);)
    {
        if (line.substr(0, 6) == "frame-")
        {
            frames.emplace_back();
        }
        const std::size_t equal = line.find('=');
        if (equal != std::string::npos)
        {
            const std::size_t start = line.find_first_not_of(" \t");
            std::string value = line.substr(equal + 1);
            if (!value.empty() && value.back() == '\r')
            {
                value.pop_back();
            }
            frames.back()[line.substr(start, equal - start)] = value;
        }
    }
    return frames;
}

} // namespace

TEST(JulibrotView, matches_source_golden_and_preserves_owned_recipes)
{
    for (const std::string &fixture : {"camera", "keyed", "hold", "normalized", "override", "camera-hold"})
    {
        SCOPED_TRACE(fixture);
        const timeline::Document document = [&fixture]
        {
            JsonImportOptions options;
            options.frames_per_second_numerator = 30000;
            options.frames_per_second_denominator = 1001;
            const JsonImportResult imported =
                import_timeline_json("fixtures/julibrot-view-" + fixture + ".json", options);
            if (!imported.succeeded() || !imported.diagnostics.empty())
            {
                throw std::runtime_error("Julibrot import failed: " + imported.diagnostics.front());
            }
            return *imported.document;
        }();
        const int outputs = fixture == "keyed" || fixture == "hold" ? 12 : 6;
        ASSERT_EQ(outputs == 12 ? 12 : 15, document.lane_count());
        const timeline::FrameGrid &grid = *document.frame_grid();
        EXPECT_EQ(4004, grid.frame_duration().ticks());
        const std::vector<std::map<std::string, std::string>> frames = julibrot_source_frames(fixture);
        ASSERT_EQ(grid.frame_count(), timeline::size_cast(frames));
        for (int frame = 0; frame < timeline::size_cast(frames); ++frame)
        {
            const timeline::FrameInspection inspected = *timeline::inspect_frame(document, frame);
            for (int component = 0; component < outputs; ++component)
            {
                const timeline::Lane &lane = document.lanes()[component];
                const timeline::Attributes attributes =
                    std::visit([](const auto &item) { return item.attributes(); }, lane.items().front());
                EXPECT_EQ("view", attributes.at("view-name"));
                EXPECT_NE(std::string::npos, attributes.at("julibrot-view").find("outputs"));
                std::string value = frames[frame].at(attributes.at("parameter"));
                const timeline::LaneInspection &sample = inspected.lanes[component];
                if (attributes.at("member") == "mode")
                {
                    EXPECT_EQ(value, sample.items.front().attributes.at("value"));
                    continue;
                }
                std::replace(value.begin(), value.end(), '/', ' ');
                std::istringstream values(value);
                double expected = 0;
                for (int scalar = 0; scalar <= std::stoi(attributes.at("component")); ++scalar)
                {
                    ASSERT_TRUE(values >> expected);
                }
                EXPECT_NEAR(expected, sample.value ? *sample.value : *sample.items.front().value, 1e-9);
                if (std::holds_alternative<timeline::Curve>(lane.items().front()))
                {
                    const timeline::Curve &curve = std::get<timeline::Curve>(lane.items().front());
                    EXPECT_TRUE(curve.samples().empty());
                    EXPECT_EQ(lane.id() + "-view", curve.id());
                }
            }
        }
        EXPECT_EQ("animation-0-geometry[0]", document.lanes()[outputs == 12 ? 1 : 0].id());
        const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
        const JsonImportResult animation = import_timeline_json("fixtures/julibrot-view-" + fixture + ".json");
        ASSERT_TRUE(music.succeeded());
        ASSERT_TRUE(animation.succeeded());
        const timeline::Document combined = timeline::combine_documents(*animation.document, *music.document);
        EXPECT_EQ(document.lane_count() + 4, combined.lane_count());
        const timeline::Layout layout(combined, timeline::Viewport(550, 850, grid.offset(), grid.end_time()),
            timeline::LayoutMetrics(110, 20, 30, 4));
        EXPECT_NE(std::string::npos, timeline::render_snapshot(layout.display_list()).find("geometry"));
    }
}

TEST(JulibrotView, samples_fractional_distance_and_normalizes_after_interpolation)
{
    const JsonImportResult imported = import_timeline_json("fixtures/julibrot-view-normalized.json");
    ASSERT_TRUE(imported.succeeded());
    const timeline::Document document = *imported.document;
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Time half = grid.offset() + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2);
    const timeline::Curve &distance = std::get<timeline::Curve>(document.lanes()[5].items().front());
    EXPECT_DOUBLE_EQ(21.25, distance.sample(half));
    EXPECT_DOUBLE_EQ(18.5, distance.sample(grid.frame_start(1)));
    EXPECT_DOUBLE_EQ(13, distance.sample(grid.end_time()));
    for (int component = 0; component < 3; ++component)
    {
        const timeline::Curve &up = std::get<timeline::Curve>(document.lanes()[12 + component].items().front());
        const double expected = component == 0 ? 0 : component == 1 ? 1.75 : 0.25;
        EXPECT_NEAR(expected / std::sqrt(1.75 * 1.75 + 0.25 * 0.25), up.sample(half), 1e-12);
        EXPECT_EQ("true", up.attributes().at("normalize"));
        EXPECT_NE(std::string::npos, up.attributes().at("signal").find("0/2/1"));
        for (int index = 0; index <= 100; ++index)
        {
            const timeline::Time time = grid.offset() +
                timeline::Duration::from_ticks((grid.frame_start(2) - grid.offset()).ticks() * index / 100);
            EXPECT_GE(up.sample(time), up.minimum());
            EXPECT_LE(up.sample(time), up.maximum());
        }
    }
    const JsonImportResult keyed = import_timeline_json("fixtures/julibrot-view-keyed.json");
    ASSERT_TRUE(keyed.succeeded());
    EXPECT_DOUBLE_EQ(0.8125, *keyed.document->lanes()[7].evaluate_keyframes(half));
    EXPECT_EQ("monocular", timeline::inspect_frame(*keyed.document, 1)->lanes[0].items.front().attributes.at("value"));
    EXPECT_EQ("red-blue", timeline::inspect_frame(*keyed.document, 2)->lanes[0].items.front().attributes.at("value"));
}

TEST(JulibrotView, preserves_layer_base_geometry_aliases_and_unused_camera)
{
    const timeline::Document document = []
    {
        const JsonImportResult imported = import_timeline_json("fixtures/julibrot-view-layers.json");
        if (!imported.succeeded() || !imported.diagnostics.empty())
        {
            throw std::runtime_error("Julibrot layer import failed: " + imported.diagnostics.front());
        }
        return *imported.document;
    }();
    ASSERT_EQ(30, document.lane_count());
    const timeline::Curve &width = std::get<timeline::Curve>(document.lanes()[4].items().front());
    EXPECT_DOUBLE_EQ(6, width.sample(document.frame_grid()->frame_start(1)));
    EXPECT_EQ("geometry-alias", width.attributes().at("parameter"));
    EXPECT_EQ("Aliased", width.attributes().at("source-entry"));
    EXPECT_EQ("aliased", width.attributes().at("layer"));
    EXPECT_EQ("64/3/4/5/6/99", width.attributes().at("source-value"));
    EXPECT_EQ("animation-layer-0-0-geometry[4]", document.lanes()[4].id());
    const timeline::Lane &overridden = document.lanes()[20];
    EXPECT_DOUBLE_EQ(18.5, *overridden.evaluate_keyframes(document.frame_grid()->frame_start(1)));
    const timeline::Keyframe &up = std::get<timeline::Keyframe>(document.lanes()[28].items().front());
    EXPECT_DOUBLE_EQ(0, up.value());
    EXPECT_EQ("false", up.attributes().at("used-by-camera"));
    EXPECT_EQ("overridden", up.attributes().at("layer"));
}

TEST(JulibrotView, rejects_invalid_tracks_transactionally_with_indexed_diagnostics)
{
    const JsonImportResult imported = import_timeline_json("fixtures/partial-julibrot-view.json");
    ASSERT_TRUE(imported.succeeded());
    const std::array<std::string, 19> diagnostics{"paths", "centered", "straight-on", "straight-on", "view-up",
        "normalize", "full frame range", "interpolation", "enum", "JSON number", "component count", "arity", "output",
        "catalog", "field", "view-up", "finite difference", "interpolation", "straight-on"};
    ASSERT_EQ(diagnostics.size(), imported.diagnostics.size());
    ASSERT_EQ(15, imported.document->lane_count());
    EXPECT_EQ("animation-19-geometry[0]", imported.document->lanes()[0].id());
    for (int index = 0; index < timeline::size_cast(diagnostics); ++index)
    {
        SCOPED_TRACE(index);
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find("animation-" + std::to_string(index) + ":"));
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find(diagnostics[index]));
    }
    const JsonImportResult failed = import_timeline_json("fixtures/invalid-julibrot-view.json");
    EXPECT_FALSE(failed.succeeded());
    EXPECT_FALSE(failed.diagnostics.empty());
}

TEST(JulibrotView, diagnoses_source_geometry_catalog_policies_and_off_grid_normalization)
{
    const JsonImportResult imported = import_timeline_json("fixtures/partial-julibrot-sources.json");
    ASSERT_TRUE(imported.succeeded());
    const std::array<std::string, 5> diagnostics{"source geometry", "six values", "numeric", "view-up", "bounds"};
    ASSERT_EQ(diagnostics.size(), imported.diagnostics.size());
    ASSERT_EQ(15, imported.document->lane_count());
    EXPECT_EQ("animation-layer-5-0-geometry[0]", imported.document->lanes()[0].id());
    for (int index = 0; index < timeline::size_cast(diagnostics); ++index)
    {
        SCOPED_TRACE(index);
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find("animation-layer-" + std::to_string(index)));
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find(diagnostics[index]));
    }
}
