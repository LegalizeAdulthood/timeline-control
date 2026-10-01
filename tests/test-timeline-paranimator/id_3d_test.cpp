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
    const std::array<std::string, 17> fixtures{"camera", "keyed", "hold", "override", "tilted", "tilted-hold",
        "oblique", "oblique-hold", "plane-hold", "oblique-plane-step", "azimuth", "azimuth-reverse", "tolerance",
        "near-parallel", "orientation-minimum", "near-vertical", "hint-cleanup"};
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

TEST(Id3DView, normalizes_tilted_hints_after_interpolation_with_owned_recipes)
{
    for (const std::string &fixture : {"tilted", "tilted-hold"})
    {
        SCOPED_TRACE(fixture);
        const timeline::Document document = [&fixture]
        {
            const JsonImportResult imported = import_timeline_json("fixtures/id-3d-view-" + fixture + ".json");
            if (!imported.succeeded() || !imported.diagnostics.empty())
            {
                throw std::runtime_error("Tilted Id camera import failed");
            }
            return *imported.document;
        }();
        const timeline::FrameGrid &grid = *document.frame_grid();
        for (int step = 0; step <= 100; ++step)
        {
            const timeline::Time time = grid.offset() +
                timeline::Duration::from_ticks((grid.frame_start(2) - grid.offset()).ticks() * step / 100);
            const double fraction = fixture == "tilted" ? step / 100.0 : step == 100 ? 1 : 0;
            const double y = -1 + 3 * fraction;
            const double horizontal = (fixture == "tilted" ? 1 : -1) * (-3 + 4 * fraction);
            const double length = std::hypot(y, horizontal);
            for (int axis = 0; axis < 3; ++axis)
            {
                const timeline::Curve &up = std::get<timeline::Curve>(document.lanes()[12 + axis].items().front());
                const int horizontal_axis = fixture == "tilted" ? 2 : 0;
                const double expected = axis == 1 ? y / length : axis == horizontal_axis ? horizontal / length : 0;
                EXPECT_NEAR(expected, up.sample(time), 1e-12);
                EXPECT_LE(*up.minimum(), up.sample(time));
                EXPECT_GE(*up.maximum(), up.sample(time));
                EXPECT_TRUE(up.samples().empty());
                EXPECT_EQ("true", up.attributes().at("normalize"));
                EXPECT_NE(std::string::npos, up.attributes().at("signal").find("-1"));
            }
        }
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

TEST(Id3DView, samples_owned_oblique_hints_and_outputs_continuously)
{
    for (const std::string &fixture : {"oblique", "oblique-hold"})
    {
        SCOPED_TRACE(fixture);
        const timeline::Document document = [&fixture]
        {
            const JsonImportResult imported = import_timeline_json("fixtures/id-3d-view-" + fixture + ".json");
            if (!imported.succeeded() || !imported.diagnostics.empty())
            {
                throw std::runtime_error("Oblique Id camera import failed");
            }
            return *imported.document;
        }();
        const timeline::FrameGrid &grid = *document.frame_grid();
        constexpr double PI = 3.14159265358979323846;
        for (int step = 0; step <= 100; ++step)
        {
            const timeline::Time time = grid.offset() +
                timeline::Duration::from_ticks((grid.frame_start(2) - grid.offset()).ticks() * step / 100);
            const double fraction = fixture == "oblique" ? step / 100.0 : step == 100 ? 1 : 0;
            const double sign = fixture == "oblique" ? 1 : -1;
            const std::array<double, 3> up{sign * (-3 + 3.6 * fraction), -1 + 3 * fraction, -4 + 4.8 * fraction};
            const double length = std::sqrt(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]);
            for (int axis = 0; axis < 3; ++axis)
            {
                const timeline::Curve &curve = std::get<timeline::Curve>(document.lanes()[12 + axis].items().front());
                EXPECT_NEAR(up[axis] / length, curve.sample(time), 1e-12);
                EXPECT_LE(*curve.minimum(), curve.sample(time));
                EXPECT_GE(*curve.maximum(), curve.sample(time));
                EXPECT_TRUE(curve.samples().empty());
                EXPECT_EQ("true", curve.attributes().at("normalize"));
                EXPECT_NE(std::string::npos, curve.attributes().at("signal").find("keys"));
            }
            const double horizontal = 5 + 5 * fraction;
            const double y = 10 - 5 * fraction;
            EXPECT_NEAR(std::atan2(y, horizontal) * 180 / PI,
                std::get<timeline::Curve>(document.lanes()[0].items().front()).sample(time), 1e-12);
            EXPECT_NEAR(-sign * std::atan2(3.0, 4.0) * 180 / PI,
                std::get<timeline::Curve>(document.lanes()[1].items().front()).sample(time), 1e-12);
            EXPECT_DOUBLE_EQ(std::round(std::hypot(horizontal, y)),
                std::get<timeline::Curve>(document.lanes()[3].items().front()).sample(time));
        }
        for (int frame = 0; frame < grid.frame_count(); ++frame)
        {
            const timeline::FrameInspection inspection = *timeline::inspect_frame(document, frame);
            for (int axis = 0; axis < 3; ++axis)
            {
                const std::optional<double> value = inspection.lanes[12 + axis].items.front().value;
                ASSERT_TRUE(value.has_value());
                EXPECT_DOUBLE_EQ(std::get<timeline::Curve>(document.lanes()[12 + axis].items().front())
                                     .sample(grid.frame_start(frame)),
                    *value);
            }
        }
    }
}

TEST(Id3DView, holds_owned_camera_planes_until_the_exact_destination_key)
{
    for (const std::string &fixture : {"plane-hold", "oblique-plane-step"})
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
                throw std::runtime_error("Held Id camera plane import failed");
            }
            return *imported.document;
        }();
        const timeline::FrameGrid &grid = *document.frame_grid();
        const timeline::Time end = grid.frame_start(2);
        const timeline::Time left = end + timeline::Duration::from_ticks(-1);
        constexpr double PI = 3.14159265358979323846;
        const double initial_yaw = fixture == "plane-hold" ? 0 : -std::atan2(3.0, 4.0) * 180 / PI;
        const double final_yaw =
            fixture == "plane-hold" ? std::atan2(6.0, 8.0) * 180 / PI : std::atan2(8.0, -6.0) * 180 / PI;
        const std::array<double, 3> initial_up =
            fixture == "plane-hold" ? std::array<double, 3>{0, -1, -3} : std::array<double, 3>{-3, -1, -4};
        const std::array<double, 3> final_up =
            fixture == "plane-hold" ? std::array<double, 3>{-0.6, 2, 0.8} : std::array<double, 3>{-0.8, 2, -0.6};
        for (timeline::Time time : {grid.offset(), grid.frame_start(1), left, end})
        {
            const bool destination = time == end;
            const std::array<double, 3> &up = destination ? final_up : initial_up;
            const double length = std::sqrt(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]);
            for (int axis = 0; axis < 3; ++axis)
            {
                const timeline::Curve &curve = std::get<timeline::Curve>(document.lanes()[12 + axis].items().front());
                EXPECT_NEAR(up[axis] / length, curve.sample(time), 1e-12);
                EXPECT_LE(*curve.minimum(), curve.sample(time));
                EXPECT_GE(*curve.maximum(), curve.sample(time));
                EXPECT_TRUE(curve.samples().empty());
                EXPECT_EQ("true", curve.attributes().at("normalize"));
                EXPECT_NE(std::string::npos, curve.attributes().at("signal").find("curve"));
            }
            const timeline::Curve &pitch = std::get<timeline::Curve>(document.lanes()[0].items().front());
            const timeline::Curve &yaw = std::get<timeline::Curve>(document.lanes()[1].items().front());
            EXPECT_NEAR(
                std::atan2(destination ? 5.0 : 10.0, destination ? 10.0 : 5.0) * 180 / PI, pitch.sample(time), 1e-12);
            EXPECT_NEAR(destination ? final_yaw : initial_yaw, yaw.sample(time), 1e-12);
            EXPECT_LE(*yaw.minimum(), yaw.sample(time));
            EXPECT_GE(*yaw.maximum(), yaw.sample(time));
            EXPECT_DOUBLE_EQ(11, std::get<timeline::Curve>(document.lanes()[3].items().front()).sample(time));
        }
        const timeline::Keyframe &eye = std::get<timeline::Keyframe>(document.lanes()[6].items().front());
        EXPECT_EQ(timeline::KeyframeInterpolation::HOLD, eye.interpolation());
        const timeline::FrameInspection before = *timeline::inspect_frame(document, 1);
        const timeline::FrameInspection after = *timeline::inspect_frame(document, 2);
        ASSERT_TRUE(before.lanes[1].items.front().value.has_value());
        ASSERT_TRUE(after.lanes[1].items.front().value.has_value());
        EXPECT_NEAR(initial_yaw, *before.lanes[1].items.front().value, 1e-12);
        EXPECT_NEAR(final_yaw, *after.lanes[1].items.front().value, 1e-12);
    }
}

TEST(Id3DView, samples_owned_moving_azimuth_and_tilted_hints_between_frames)
{
    for (const std::string &fixture : {"azimuth", "azimuth-reverse"})
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
                throw std::runtime_error("Moving-azimuth Id camera import failed");
            }
            return *imported.document;
        }();
        const timeline::FrameGrid &grid = *document.frame_grid();
        const timeline::Ticks span = (grid.frame_start(2) - grid.offset()).ticks();
        constexpr double PI = 3.14159265358979323846;
        for (int step = 0; step <= 100; ++step)
        {
            const timeline::Ticks ticks = span * step / 100;
            const timeline::Time time = grid.offset() + timeline::Duration::from_ticks(ticks);
            const double fraction = static_cast<double>(ticks) / static_cast<double>(span);
            const double motion = fixture == "azimuth" ? fraction : 1 - fraction;
            const std::array<double, 3> eye{3 + 5 * motion, 10 - 5 * motion, 4 + 2 * motion};
            const std::array<double, 3> up{-0.6 - motion, -1 + 3 * motion, -0.8 - 0.4 * motion};
            const double length = std::sqrt(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]);
            for (int axis = 0; axis < 3; ++axis)
            {
                const timeline::Curve &curve = std::get<timeline::Curve>(document.lanes()[12 + axis].items().front());
                EXPECT_NEAR(up[axis] / length, curve.sample(time), 1e-12);
                EXPECT_LE(*curve.minimum(), curve.sample(time));
                EXPECT_GE(*curve.maximum(), curve.sample(time));
                EXPECT_TRUE(curve.samples().empty());
                EXPECT_EQ("true", curve.attributes().at("normalize"));
                EXPECT_NE(std::string::npos, curve.attributes().at("signal").find("keys"));
            }
            const double horizontal = std::hypot(eye[0], eye[2]);
            const timeline::Curve &pitch = std::get<timeline::Curve>(document.lanes()[0].items().front());
            const timeline::Curve &yaw = std::get<timeline::Curve>(document.lanes()[1].items().front());
            const timeline::Curve &distance = std::get<timeline::Curve>(document.lanes()[3].items().front());
            EXPECT_NEAR(std::atan2(eye[1], horizontal) * 180 / PI, pitch.sample(time), 1e-12);
            EXPECT_NEAR(-std::atan2(eye[0], eye[2]) * 180 / PI, yaw.sample(time), 1e-12);
            EXPECT_DOUBLE_EQ(std::round(std::hypot(horizontal, eye[1])), distance.sample(time));
            for (const timeline::Curve &curve : {pitch, yaw, distance})
            {
                EXPECT_LE(*curve.minimum(), curve.sample(time));
                EXPECT_GE(*curve.maximum(), curve.sample(time));
                EXPECT_TRUE(curve.samples().empty());
            }
        }
    }
}

TEST(Id3DView, validates_tolerance_boundaries_continuously_with_owned_hints)
{
    for (const std::string &fixture :
        {"tolerance", "near-parallel", "orientation-minimum", "near-vertical", "hint-cleanup"})
    {
        SCOPED_TRACE(fixture);
        const JsonImportResult imported = import_timeline_json("fixtures/id-3d-view-" + fixture + ".json");
        ASSERT_TRUE(imported.succeeded());
        ASSERT_TRUE(imported.diagnostics.empty());
        const timeline::Document document = *imported.document;
        const timeline::FrameGrid &grid = *document.frame_grid();
        for (int step = 0; step <= 100; ++step)
        {
            const timeline::Time time = grid.offset() +
                timeline::Duration::from_ticks((grid.frame_start(2) - grid.offset()).ticks() * step / 100);
            double squared = 0;
            const double fraction = step / 100.0;
            const std::array<double, 3> raw_hint = fixture == "tolerance"
                ? std::array<double, 3>{-0.6, -1 + (5 + fraction) * 1e-10, -0.8 - (1 + 0.2 * fraction) * 1e-10}
                : fixture == "near-parallel"       ? std::array<double, 3>{-1, -10 + (1 + fraction) * 1e-10, 0}
                : fixture == "orientation-minimum" ? std::array<double, 3>{0, 0.0625000000001, fraction}
                : fixture == "hint-cleanup"        ? std::array<double, 3>{0, 1.1e-12, -1}
                                                   : std::array<double, 3>{0, 1, 0};
            const double length =
                std::sqrt(raw_hint[0] * raw_hint[0] + raw_hint[1] * raw_hint[1] + raw_hint[2] * raw_hint[2]);
            for (int axis = 0; axis < 3; ++axis)
            {
                const timeline::Curve &hint = std::get<timeline::Curve>(document.lanes()[12 + axis].items().front());
                const double value = hint.sample(time);
                EXPECT_NEAR(raw_hint[axis] / length, value, 1e-14);
                squared += value * value;
                EXPECT_LE(*hint.minimum(), value);
                EXPECT_GE(*hint.maximum(), value);
                EXPECT_TRUE(hint.samples().empty());
                EXPECT_EQ("true", hint.attributes().at("normalize"));
                EXPECT_NE(std::string::npos, hint.attributes().at("signal").find("keys"));
            }
            EXPECT_NEAR(1, squared, 1e-12);
            for (int component = 0; component < 6; ++component)
            {
                const timeline::Curve &output = std::get<timeline::Curve>(document.lanes()[component].items().front());
                EXPECT_LE(*output.minimum(), output.sample(time));
                EXPECT_GE(*output.maximum(), output.sample(time));
                EXPECT_TRUE(output.samples().empty());
            }
        }
    }
}

TEST(Id3DView, rejects_roll_and_cleanup_neighbors_without_partial_documents)
{
    const JsonImportResult imported = import_timeline_json("fixtures/invalid-id-3d-view-tolerance.json");
    EXPECT_FALSE(imported.succeeded());
    EXPECT_FALSE(imported.document.has_value());
    ASSERT_EQ(7, timeline::size_cast(imported.diagnostics));
    for (int index = 0; index < 6; ++index)
    {
        SCOPED_TRACE(imported.diagnostics[index]);
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find("animation-" + std::to_string(index) + ":"));
    }
    EXPECT_NE(std::string::npos, imported.diagnostics.back().find("no supported animation tracks"));
}

TEST(Id3DView, rejects_roll_between_sampled_frames_and_unsafe_azimuth_intervals)
{
    // Both authored endpoints are coplanar; their interpolated midpoint is not.
    EXPECT_NEAR(0, 3 * -0.8 - 4 * -0.6, 1e-12);
    EXPECT_NEAR(0, 8 * -0.6 - 6 * -0.8, 1e-12);
    EXPECT_NEAR(-0.35, 5.5 * -0.7 - 5 * -0.7, 1e-12);
    const JsonImportResult imported = import_timeline_json("fixtures/invalid-id-3d-view-azimuth.json");
    EXPECT_FALSE(imported.succeeded());
    EXPECT_FALSE(imported.document.has_value());
    const std::array<std::string, 5> messages{
        "proportional", "orientation", "vertical", "source tolerance", "normalization"};
    ASSERT_EQ(timeline::size_cast(messages) + 1, timeline::size_cast(imported.diagnostics));
    for (int index = 0; index < timeline::size_cast(messages); ++index)
    {
        SCOPED_TRACE(imported.diagnostics[index]);
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find("animation-" + std::to_string(index) + ":"));
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find(messages[index]));
    }
}

TEST(Id3DView, rejects_unsafe_plane_jump_endpoints_and_mixed_motion)
{
    const JsonImportResult imported = import_timeline_json("fixtures/invalid-id-3d-view-plane-jump.json");
    EXPECT_FALSE(imported.succeeded());
    EXPECT_FALSE(imported.document.has_value());
    const std::array<std::string, 5> messages{
        "fixed vertical", "vertical", "vertical", "fixed vertical", "fixed vertical"};
    ASSERT_EQ(timeline::size_cast(messages) + 1, timeline::size_cast(imported.diagnostics));
    for (int index = 0; index < timeline::size_cast(messages); ++index)
    {
        SCOPED_TRACE(imported.diagnostics[index]);
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find("animation-" + std::to_string(index) + ":"));
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find(messages[index]));
    }
    EXPECT_NE(std::string::npos, imported.diagnostics.back().find("no supported animation tracks"));
}

TEST(Id3DView, diagnoses_oblique_roll_and_unsupported_motion_without_partial_lanes)
{
    const JsonImportResult imported = import_timeline_json("fixtures/invalid-id-3d-view-oblique.json");
    EXPECT_FALSE(imported.succeeded());
    const std::array<std::string, 5> messages{
        "fixed vertical", "fixed vertical", "orientation", "source tolerance", "orientation"};
    ASSERT_EQ(timeline::size_cast(messages) + 1, timeline::size_cast(imported.diagnostics));
    for (int index = 0; index < timeline::size_cast(messages); ++index)
    {
        SCOPED_TRACE(imported.diagnostics[index]);
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find("animation-" + std::to_string(index) + ":"));
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find(messages[index]));
    }
}

TEST(Id3DView, rejects_tilted_singularities_between_frames_and_at_held_endpoints)
{
    const JsonImportResult imported = import_timeline_json("fixtures/invalid-id-3d-view-tilted.json");
    EXPECT_FALSE(imported.succeeded());
    ASSERT_EQ(8, imported.diagnostics.size());
    EXPECT_NE(std::string::npos, imported.diagnostics.back().find("no supported animation tracks"));
    for (int index = 0; index < 7; ++index)
    {
        SCOPED_TRACE(imported.diagnostics[index]);
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find("animation-" + std::to_string(index) + ":"));
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find("view-up"));
    }
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
