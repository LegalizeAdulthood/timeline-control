// Copyright (c) 2026 Richard Thomson

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

TEST(CameraSkew, matchesSourceGoldenAndPreservesOwnedDefinitions)
{
    const std::array<std::string, 9> fixtures{
        "center-mag", "corners", "geometric", "negative", "hold", "step", "reverse", "eye", "eye-corners"};
    for (const std::string &fixture : fixtures)
    {
        SCOPED_TRACE(fixture);
        const std::string output = fixture == "center-mag" || fixture == "eye" ? "center-mag" : "corners";
        const bool eye = fixture == "eye" || fixture == "eye-corners";
        const timeline::Document document = [&fixture]
        {
            JsonImportOptions options;
            options.frames_per_second_numerator = 30000;
            options.frames_per_second_denominator = 1001;
            const JsonImportResult imported =
                import_timeline_json("fixtures/camera2d-skew-" + fixture + ".json", options);
            if (!imported.succeeded() || !imported.diagnostics.empty())
            {
                throw std::runtime_error("Camera2D skew import failed");
            }
            return *imported.document;
        }();
        ASSERT_EQ(eye ? 16 : 12, document.lane_count());
        const timeline::FrameGrid &grid = *document.frame_grid();
        EXPECT_EQ(4004, grid.frame_duration().ticks());
        std::ifstream golden("fixtures/gold-camera2d-skew-" + fixture + ".par");
        ASSERT_TRUE(golden);
        int frame = 0;
        for (std::string line; std::getline(golden, line);)
        {
            const std::size_t start = line.find(output + "=");
            if (start == std::string::npos)
            {
                continue;
            }
            std::string value = line.substr(start + output.size() + 1);
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
            if (output == "corners" && source.size() == 4)
            {
                source.push_back(source[0]);
                source.push_back(source[2]);
            }
            else if (output == "center-mag")
            {
                if (source.size() == 3)
                {
                    source.push_back(1);
                }
                source.resize(6, 0);
            }
            ASSERT_EQ(6, source.size());
            const std::optional<timeline::FrameInspection> inspected = timeline::inspect_frame(document, frame++);
            ASSERT_TRUE(inspected);
            for (int component = 0; component < 6; ++component)
            {
                EXPECT_NEAR(source[component], *inspected->lanes[component].items.front().value, 1e-9);
                const timeline::Curve &curve = std::get<timeline::Curve>(document.lanes()[component].items().front());
                EXPECT_TRUE(curve.samples().empty());
                EXPECT_EQ("animation-0-" + output + "[" + std::to_string(component) + "]-camera", curve.id());
                EXPECT_NE(std::string::npos, curve.attributes().at("camera2d").find("skew"));
            }
        }
        EXPECT_EQ(grid.frame_count(), frame);
        if (fixture == "center-mag" || fixture == "corners")
        {
            const timeline::Keyframe &skew = std::get<timeline::Keyframe>(document.lanes()[11].items().front());
            EXPECT_EQ("camera.skew", skew.attributes().at("parameter"));
            EXPECT_EQ(timeline::KeyframeInterpolation::LINEAR, skew.interpolation());
            EXPECT_NE(std::string::npos, skew.attributes().at("signal").find("keys"));
            const timeline::Time half_frame =
                grid.offset() + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2);
            const timeline::Curve &curve = std::get<timeline::Curve>(document.lanes()[0].items().front());
            constexpr double PI = 3.14159265358979323846;
            EXPECT_NEAR(output == "corners" ? -1 + 2 * std::tan(2.5 * PI / 180) : 0, curve.sample(half_frame), 1e-12);
        }
        const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
        ASSERT_TRUE(music.succeeded());
        const JsonImportResult animation = import_timeline_json("fixtures/camera2d-skew-" + fixture + ".json");
        ASSERT_TRUE(animation.succeeded());
        const timeline::Document combined = timeline::combine_documents(*animation.document, *music.document);
        EXPECT_EQ(document.lane_count() + 4, combined.lane_count());
        const timeline::FrameGrid &combined_grid = *combined.frame_grid();
        const timeline::Layout layout(combined,
            timeline::Viewport(500, 850, combined_grid.offset(), combined_grid.end_time()),
            timeline::LayoutMetrics(100, 20, 30, 4));
        EXPECT_NE(std::string::npos, timeline::render_snapshot(layout.display_list()).find("skew"));
    }
}

TEST(CameraSkew, evaluatesInterpolationAndComposesWithAnalyticEye)
{
    const std::array<std::string, 6> variants{"geometric", "negative", "hold", "step", "reverse", "eye"};
    for (const std::string &variant : variants)
    {
        SCOPED_TRACE(variant);
        const JsonImportResult imported = import_timeline_json("fixtures/camera2d-skew-" + variant + ".json");
        ASSERT_TRUE(imported.succeeded());
        ASSERT_TRUE(imported.diagnostics.empty());
        const timeline::Document copy = *imported.document;
        EXPECT_EQ(variant == "eye" ? 16 : 12, copy.lane_count());
        const timeline::FrameGrid &grid = *copy.frame_grid();
        const timeline::Time start = grid.offset();
        const timeline::Time end = grid.frame_start(grid.frame_count() - 1);
        const timeline::Time middle = start + timeline::Duration::from_ticks((end - start).ticks() / 2);
        constexpr double PI = 3.14159265358979323846;
        const double angle = variant == "geometric"     ? 20
            : variant == "negative" || variant == "eye" ? -20
            : variant == "hold"                         ? 80
            : variant == "step"                         ? 100
                                                        : 110;
        const int component = variant == "eye" ? 5 : 0;
        const timeline::Curve &curve = std::get<timeline::Curve>(copy.lanes()[component].items().front());
        EXPECT_NEAR(variant == "eye" ? angle : -1 + 2 * std::tan(angle * PI / 180), curve.sample(middle), 1e-10);
        for (int fraction = 0; fraction <= 100; ++fraction)
        {
            const timeline::Time time = start + timeline::Duration::from_ticks((end - start).ticks() * fraction / 100);
            for (int output = 0; output < 6; ++output)
            {
                const timeline::Curve &sample = std::get<timeline::Curve>(copy.lanes()[output].items().front());
                EXPECT_TRUE(std::isfinite(sample.sample(time)));
                EXPECT_LE(*sample.minimum(), sample.sample(time));
                EXPECT_GE(*sample.maximum(), sample.sample(time));
            }
        }
        const timeline::Lane &input = copy.lanes()[11];
        EXPECT_EQ("animation-0-skew", input.id());
        std::visit(
            [](const auto &item) { EXPECT_FALSE(item.attributes().at("signal").empty()); }, input.items().front());
        if (variant == "hold" || variant == "step")
        {
            const timeline::Keyframe &key = std::get<timeline::Keyframe>(input.items().front());
            EXPECT_EQ(timeline::KeyframeInterpolation::HOLD, key.interpolation());
            EXPECT_NE(curve.sample(end), curve.sample(timeline::Time::from_ticks(end.ticks() - 1)));
        }
    }
}

TEST(CameraSkew, diagnosesMalformedKeysAndUnsampledCornersPoles)
{
    const JsonImportResult center = import_timeline_json("fixtures/camera2d-skew-center-pole.json");
    ASSERT_TRUE(center.succeeded());
    EXPECT_TRUE(center.diagnostics.empty());
    const timeline::Curve &skew = std::get<timeline::Curve>(center.document->lanes()[5].items().front());
    EXPECT_DOUBLE_EQ(200, skew.sample(skew.end()));
    EXPECT_DOUBLE_EQ(200, *skew.maximum());
    const std::array<std::pair<std::string, std::string>, 11> cases{
        std::pair<std::string, std::string>{"near-pole", "singular"},
        std::pair<std::string, std::string>{"overflow", "finite"},
        std::pair<std::string, std::string>{"pole", "singular"},
        std::pair<std::string, std::string>{"endpoint", "singular"},
        std::pair<std::string, std::string>{"path", "paths are not allowed"},
        std::pair<std::string, std::string>{"number", "JSON number"},
        std::pair<std::string, std::string>{"type", "double"},
        std::pair<std::string, std::string>{"range", "full frame range"},
        std::pair<std::string, std::string>{"curve", "interpolation"},
        std::pair<std::string, std::string>{"zero", "geometric"},
        std::pair<std::string, std::string>{"sign", "geometric"}};
    for (const auto &[name, diagnostic] : cases)
    {
        SCOPED_TRACE(name);
        const JsonImportResult rejected = import_timeline_json("fixtures/invalid-camera2d-skew-" + name + ".json");
        EXPECT_FALSE(rejected.succeeded());
        ASSERT_FALSE(rejected.diagnostics.empty());
        EXPECT_NE(std::string::npos, rejected.diagnostics.front().find("animation-0:"));
        EXPECT_NE(std::string::npos, rejected.diagnostics.front().find(diagnostic));
    }
}
