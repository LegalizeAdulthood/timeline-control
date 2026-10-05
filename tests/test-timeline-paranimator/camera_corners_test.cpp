// Copyright (c) 2026 Richard Thomson

#include <ResolvedAttributes.h>

#include <timelineParAnimator/TimelineJson.h>

#include <timeline/Layout.h>
#include <timeline/Query.h>
#include <timeline/Snapshot.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <sstream>
#include <variant>

using namespace timeline_par_animator;

TEST(CameraCorners, samplesOwnedAffineComponentsLikeParanimator)
{
    const std::array<std::string, 4> fixtures{"camera2d-corners", "camera2d-eye-corners",
        "camera2d-corners-rotated-source", "camera2d-corners-reversed-source"};
    const std::array<std::string, 6> components{
        "top-left-x", "bottom-right-x", "bottom-right-y", "top-left-y", "bottom-left-x", "bottom-left-y"};
    for (const std::string &fixture : fixtures)
    {
        SCOPED_TRACE(fixture);
        const timeline::Document document = [&fixture]
        {
            JsonImportOptions options;
            options.frames_per_second_numerator = 30000;
            options.frames_per_second_denominator = 1001;
            const JsonImportResult imported = import_timeline_json("fixtures/" + fixture + ".json", options);
            if (!imported.succeeded() || !imported.diagnostics.empty())
            {
                throw std::runtime_error("Camera2D corners import failed");
            }
            return *imported.document;
        }();
        const bool eye = fixture == "camera2d-eye-corners";
        ASSERT_EQ(eye ? 13 : 11, document.lane_count());
        const timeline::FrameGrid &grid = *document.frame_grid();
        EXPECT_EQ(4004, grid.frame_duration().ticks());
        std::ifstream golden("fixtures/gold-" + fixture + ".par");
        ASSERT_TRUE(golden);
        int frame = 0;
        for (std::string line; std::getline(golden, line);)
        {
            const std::size_t start = line.find("corners=");
            if (start == std::string::npos)
            {
                continue;
            }
            std::string value = line.substr(start + 8);
            while (!value.empty() && value.back() == '\\')
            {
                value.pop_back();
                ASSERT_TRUE(std::getline(golden, line));
                value += line.substr(line.find_first_not_of(" \t"));
            }
            std::replace(value.begin(), value.end(), '/', ' ');
            std::istringstream values(value);
            std::vector<double> source;
            for (double component; values >> component;)
            {
                source.push_back(component);
            }
            ASSERT_TRUE(source.size() == 4 || source.size() == 6);
            const std::array<double, 6> expected = source.size() == 4
                ? std::array<double, 6>{source[0], source[1], source[2], source[3], source[0], source[2]}
                : std::array<double, 6>{source[0], source[1], source[2], source[3], source[4], source[5]};
            const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(document, frame);
            ASSERT_TRUE(inspection);
            EXPECT_EQ(grid.frame_start(frame), inspection->time);
            for (int component = 0; component < 6; ++component)
            {
                ASSERT_TRUE(inspection->lanes[component].items.front().value);
                EXPECT_NEAR(expected[component], *inspection->lanes[component].items.front().value, 1e-9);
                const timeline::Lane &lane = document.lanes()[component];
                const std::string lane_name = "animation-0-corners[" + std::to_string(component) + "]";
                EXPECT_EQ(lane_name, document.strings().lookup(lane.id()));
                const timeline::Curve &curve = std::get<timeline::Curve>(lane.items().front());
                EXPECT_EQ(lane_name + "-camera", document.strings().lookup(curve.id()));
                EXPECT_EQ("corners", resolved_attributes(document, curve.attributes()).at("parameter"));
                EXPECT_EQ(components[component], resolved_attributes(document, curve.attributes()).at("component"));
                EXPECT_FALSE(resolved_attributes(document, curve.attributes()).at("camera2d").empty());
                EXPECT_TRUE(curve.samples().empty());
                EXPECT_EQ(grid.offset(), curve.start());
                EXPECT_EQ(grid.frame_start(grid.frame_count() - 1), curve.end());
            }
            ++frame;
        }
        EXPECT_EQ(grid.frame_count(), frame);
        for (int tenth_frame = 0; tenth_frame <= 10 * (frame - 1); ++tenth_frame)
        {
            const timeline::Time time =
                grid.offset() + timeline::Duration::from_ticks(tenth_frame * grid.frame_duration().ticks() / 10);
            for (int component = 0; component < 6; ++component)
            {
                const timeline::Curve &curve = std::get<timeline::Curve>(document.lanes()[component].items().front());
                const double value = curve.sample(time);
                EXPECT_TRUE(std::isfinite(value));
                EXPECT_LE(*curve.minimum(), value);
                EXPECT_GE(*curve.maximum(), value);
            }
        }
        const timeline::Keyframe &height = std::get<timeline::Keyframe>(document.lanes()[10].items().front());
        EXPECT_FALSE(resolved_attributes(document, height.attributes()).at("signal").empty());
        const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
        ASSERT_TRUE(music.succeeded());
        const JsonImportResult comparison = import_timeline_json("fixtures/" + fixture + ".json");
        ASSERT_TRUE(comparison.succeeded());
        const timeline::Document combined = timeline::combine_documents(*comparison.document, *music.document);
        EXPECT_EQ(document.lane_count() + 4, combined.lane_count());
        const timeline::FrameGrid &combined_grid = *combined.frame_grid();
        const timeline::Layout layout(combined,
            timeline::Viewport(500, 600, combined_grid.offset(), combined_grid.end_time()),
            timeline::LayoutMetrics(100, 20, 30, 4));
        const std::string snapshot = timeline::render_snapshot(layout.display_list());
        EXPECT_NE(std::string::npos, snapshot.find("top-left-x"));
        EXPECT_NE(std::string::npos, snapshot.find("bottom-left-y"));
        EXPECT_NE(std::string::npos, snapshot.find("height"));
        const timeline::Curve &first = std::get<timeline::Curve>(document.lanes()[0].items().front());
        const double expected_aspect = fixture == "camera2d-corners-rotated-source" ? 2
            : fixture == "camera2d-corners-reversed-source"                         ? 1.5
                                                                                    : 0.5;
        EXPECT_DOUBLE_EQ(
            expected_aspect, std::stod(std::string(resolved_attributes(document, first.attributes()).at("aspect"))));
    }
}

TEST(CameraCorners, diagnosesUnusableSourceViewsAndOutputTypes)
{
    const std::array<std::pair<std::string, std::string>, 6> cases{
        std::pair<std::string, std::string>{"BadArity", "four or six"},
        std::pair<std::string, std::string>{"ZeroWidth", "width or height"},
        std::pair<std::string, std::string>{"ZeroHeight", "width or height"},
        std::pair<std::string, std::string>{"ZeroRotatedWidth", "width or height"},
        std::pair<std::string, std::string>{"Missing", "missing from the source"},
        std::pair<std::string, std::string>{"catalog", "catalog-declared"}};
    for (const auto &[name, diagnostic] : cases)
    {
        SCOPED_TRACE(name);
        const JsonImportResult rejected = import_timeline_json("fixtures/invalid-camera2d-corners-" + name + ".json");
        EXPECT_FALSE(rejected.succeeded());
        ASSERT_FALSE(rejected.diagnostics.empty());
        EXPECT_NE(std::string::npos, rejected.diagnostics.front().find("animation-0:"));
        EXPECT_NE(std::string::npos, rejected.diagnostics.front().find(diagnostic));
    }
}
