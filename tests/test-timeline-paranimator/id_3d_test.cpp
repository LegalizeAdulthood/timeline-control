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

std::vector<std::map<std::string, std::string>> source_frames(const std::string &fixture)
{
    std::ifstream input("fixtures/gold-id-3d-view-" + fixture + ".par");
    if (!input)
    {
        throw std::runtime_error("Missing Id 3D golden");
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

TEST(Id3DView, matches_source_golden_and_preserves_owned_recipes)
{
    const std::array<std::string, 4> fixtures{"camera", "keyed", "hold", "override"};
    for (const std::string &fixture : fixtures)
    {
        SCOPED_TRACE(fixture);
        const timeline::Document document = [&fixture]
        {
            JsonImportOptions options;
            options.frames_per_second_numerator = 30000;
            options.frames_per_second_denominator = 1001;
            const JsonImportResult imported = import_timeline_json("fixtures/id-3d-view-" + fixture + ".json", options);
            if (!imported.succeeded() || !imported.diagnostics.empty())
            {
                throw std::runtime_error("Id 3D import failed");
            }
            return *imported.document;
        }();
        const int outputs = fixture == "keyed" ? 19 : 6;
        EXPECT_EQ(fixture == "keyed" ? 25 : fixture == "override" ? 16 : 15, document.lane_count());
        const timeline::FrameGrid &grid = *document.frame_grid();
        EXPECT_EQ(4004, grid.frame_duration().ticks());
        const std::vector<std::map<std::string, std::string>> frames = source_frames(fixture);
        ASSERT_EQ(grid.frame_count(), frames.size());
        for (int frame = 0; frame < timeline::size_cast(frames); ++frame)
        {
            const timeline::FrameInspection inspected = *timeline::inspect_frame(document, frame);
            for (int component = 0; component < outputs; ++component)
            {
                const timeline::Lane &lane = document.lanes()[component];
                const timeline::Attributes attributes =
                    std::visit([](const auto &item) { return item.attributes(); }, lane.items().front());
                EXPECT_EQ("view", attributes.at("view-name"));
                EXPECT_NE(std::string::npos, attributes.at("id-3d-view").find("outputs"));
                std::string value = frames[frame].at(attributes.at("parameter"));
                if (attributes.at("parameter") == "sphere")
                {
                    EXPECT_EQ(value, inspected.lanes[component].items.front().attributes.at("value"));
                    continue;
                }
                std::replace(value.begin(), value.end(), '/', ' ');
                std::istringstream values(value);
                double source = 0;
                const int index = std::stoi(attributes.at("component"));
                for (int scalar = 0; scalar <= index; ++scalar)
                {
                    ASSERT_TRUE(values >> source);
                }
                const timeline::LaneInspection &sample = inspected.lanes[component];
                EXPECT_NEAR(source, sample.value ? *sample.value : *sample.items.front().value, 1e-9);
                if (std::holds_alternative<timeline::Curve>(lane.items().front()))
                {
                    const timeline::Curve &curve = std::get<timeline::Curve>(lane.items().front());
                    EXPECT_TRUE(curve.samples().empty());
                    EXPECT_EQ(lane.id() + "-view", curve.id());
                }
            }
        }
        EXPECT_EQ("animation-0-rotation[0]", document.lanes()[0].id());
        EXPECT_EQ("animation-0-perspective", document.lanes()[3].id());
        const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
        ASSERT_TRUE(music.succeeded());
        const JsonImportResult comparison = import_timeline_json("fixtures/id-3d-view-" + fixture + ".json");
        ASSERT_TRUE(comparison.succeeded());
        const timeline::Document combined = timeline::combine_documents(*comparison.document, *music.document);
        EXPECT_EQ(document.lane_count() + 4, combined.lane_count());
        const timeline::Layout layout(combined, timeline::Viewport(550, 850, grid.offset(), grid.end_time()),
            timeline::LayoutMetrics(110, 20, 30, 4));
        EXPECT_NE(std::string::npos, timeline::render_snapshot(layout.display_list()).find("perspective"));
    }
}

TEST(Id3DView, samples_continuously_with_integer_rounding_and_endpoint_holds)
{
    const JsonImportResult camera = import_timeline_json("fixtures/id-3d-view-camera.json");
    ASSERT_TRUE(camera.succeeded());
    const timeline::FrameGrid &grid = *camera.document->frame_grid();
    const timeline::Time half_frame = grid.offset() + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2);
    const timeline::Curve &rotation = std::get<timeline::Curve>(camera.document->lanes()[1].items().front());
    constexpr double PI = 3.14159265358979323846;
    EXPECT_NEAR(-std::atan2(2.5, 7.5) * 180 / PI, rotation.sample(half_frame), 1e-12);
    const timeline::Curve &distance = std::get<timeline::Curve>(camera.document->lanes()[3].items().front());
    EXPECT_DOUBLE_EQ(8, distance.sample(half_frame));
    const timeline::Keyframe &eye = std::get<timeline::Keyframe>(camera.document->lanes()[6].items().front());
    EXPECT_EQ("view.eye", eye.attributes().at("parameter"));
    EXPECT_NE(std::string::npos, eye.attributes().at("signal").find("keys"));
    EXPECT_EQ("0", eye.attributes().at("component"));

    const JsonImportResult keyed = import_timeline_json("fixtures/id-3d-view-keyed.json");
    ASSERT_TRUE(keyed.succeeded());
    const timeline::Curve &integer = std::get<timeline::Curve>(keyed.document->lanes()[3].items().front());
    EXPECT_DOUBLE_EQ(12, integer.sample(grid.frame_start(1)));
    EXPECT_DOUBLE_EQ(11, integer.sample(half_frame));
    const timeline::Curve &negative = std::get<timeline::Curve>(keyed.document->lanes()[9].items().front());
    EXPECT_DOUBLE_EQ(-3, negative.sample(grid.frame_start(1)));
    const timeline::Keyframe &authored = std::get<timeline::Keyframe>(keyed.document->lanes()[19].items().back());
    EXPECT_DOUBLE_EQ(13, authored.value());

    for (const std::string &fixture : {"hold", "override"})
    {
        const JsonImportResult held = import_timeline_json("fixtures/id-3d-view-" + fixture + ".json");
        ASSERT_TRUE(held.succeeded());
        const timeline::Curve &curve = std::get<timeline::Curve>(held.document->lanes()[3].items().front());
        EXPECT_DOUBLE_EQ(fixture == "hold" ? 10 : 20, curve.sample(half_frame));
        EXPECT_DOUBLE_EQ(fixture == "hold" ? 10 : 23, curve.sample(grid.frame_start(2)));
    }
}

TEST(Id3DView, rejects_invalid_tracks_transactionally_with_indexed_diagnostics)
{
    const JsonImportResult imported = import_timeline_json("fixtures/partial-id-3d-view.json");
    ASSERT_TRUE(imported.succeeded());
    const std::array<std::string, 19> diagnostics{"paths are not allowed", "centered", "world-up", "vertical",
        "vertical", "normalize", "full frame range", "interpolation", "JSON integer", "component count", "JSON boolean",
        "bounds", "output", "catalog", "field", "field", "integer range", "integer range", "interpolation"};
    ASSERT_EQ(diagnostics.size(), imported.diagnostics.size());
    ASSERT_EQ(15, imported.document->lane_count());
    EXPECT_EQ("animation-19-rotation[0]", imported.document->lanes()[0].id());
    for (int index = 0; index < timeline::size_cast(diagnostics); ++index)
    {
        SCOPED_TRACE(index);
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find("animation-" + std::to_string(index) + ":"));
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find(diagnostics[index]));
    }
    const JsonImportResult failed = import_timeline_json("fixtures/invalid-id-3d-view.json");
    EXPECT_FALSE(failed.succeeded());
    EXPECT_FALSE(failed.diagnostics.empty());
}

TEST(Id3DView, preserves_layer_sources_aliases_and_unused_camera_inputs)
{
    const JsonImportResult imported = import_timeline_json("fixtures/id-3d-view-layers.json");
    ASSERT_TRUE(imported.succeeded());
    ASSERT_TRUE(imported.diagnostics.empty());
    ASSERT_EQ(31, imported.document->lane_count());
    const timeline::Document document = *imported.document;
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Curve &pitch = std::get<timeline::Curve>(document.lanes()[0].items().front());
    EXPECT_EQ("animation-layer-0-0-rotation[0]", document.lanes()[0].id());
    EXPECT_EQ("view-rotation", pitch.attributes().at("parameter"));
    EXPECT_EQ("pitched", pitch.attributes().at("layer"));
    EXPECT_EQ("Mandel_Demo", pitch.attributes().at("source-entry"));
    EXPECT_NEAR(35.264389682754654, pitch.sample(grid.frame_start(1)), 1e-12);
    const timeline::Curve &up = std::get<timeline::Curve>(document.lanes()[13].items().front());
    EXPECT_DOUBLE_EQ(1, up.sample(grid.frame_start(1)));
    EXPECT_EQ("true", up.attributes().at("normalize"));
    const timeline::Keyframe &override = std::get<timeline::Keyframe>(document.lanes()[15].items().front());
    EXPECT_EQ("Julia_Demo", override.attributes().at("source-entry"));
    const timeline::Keyframe &unused = std::get<timeline::Keyframe>(document.lanes()[28].items().front());
    EXPECT_EQ("false", unused.attributes().at("used-by-camera"));
    EXPECT_DOUBLE_EQ(0, unused.value());
    for (int step = 0; step <= 100; ++step)
    {
        const timeline::Time time =
            grid.offset() + timeline::Duration::from_ticks((grid.frame_start(2) - grid.offset()).ticks() * step / 100);
        for (int component = 0; component < 6; ++component)
        {
            const timeline::Curve &curve = std::get<timeline::Curve>(document.lanes()[component].items().front());
            EXPECT_LE(*curve.minimum(), curve.sample(time));
            EXPECT_GE(*curve.maximum(), curve.sample(time));
        }
    }
}
