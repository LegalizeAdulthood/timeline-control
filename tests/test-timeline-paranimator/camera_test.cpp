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
#include <sstream>
#include <variant>

using namespace timeline_par_animator;

TEST(CameraImport, samples_owned_moving_look_compositions_like_paranimator)
{
    const std::array<std::string, 5> kinds{
        "keyed-bezier", "circle-keyed", "hold-spiral", "bezier-catmull", "ellipse-lissajous"};
    const auto sample = [](const timeline::Lane &lane, timeline::Time time)
    {
        return std::holds_alternative<timeline::Curve>(lane.items().front())
            ? std::get<timeline::Curve>(lane.items().front()).sample(time)
            : *lane.evaluate_keyframes(time);
    };
    for (const std::string &kind : kinds)
    {
        SCOPED_TRACE(kind);
        const timeline::Document document = [&kind]
        {
            const JsonImportResult imported = import_timeline_json("fixtures/camera2d-moving-" + kind + ".json");
            if (!imported.succeeded() || !imported.diagnostics.empty())
            {
                throw std::runtime_error("Camera2D moving look-at import failed");
            }
            return *imported.document;
        }();
        ASSERT_EQ(15, document.lane_count());
        const timeline::FrameGrid &grid = *document.frame_grid();
        std::ifstream golden("fixtures/gold-camera2d-moving-" + kind + ".par");
        ASSERT_TRUE(golden);
        int frame = 0;
        for (std::string line; std::getline(golden, line);)
        {
            const std::size_t start = line.find("center-mag=");
            if (start == std::string::npos)
            {
                continue;
            }
            std::string value = line.substr(start + 11);
            std::replace(value.begin(), value.end(), '/', ' ');
            std::istringstream values(value);
            std::array<double, 6> expected{0, 0, 1, 1, 0, 0};
            for (double &component : expected)
            {
                if (!(values >> component))
                {
                    break;
                }
            }
            const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(document, frame++);
            ASSERT_TRUE(inspection);
            for (int component = 0; component < 6; ++component)
            {
                ASSERT_TRUE(inspection->lanes[component].items.front().value);
                const double actual = *inspection->lanes[component].items.front().value;
                EXPECT_NEAR(0,
                    component == 4 ? std::remainder(actual - expected[component], 360) : actual - expected[component],
                    1e-9);
            }
        }
        EXPECT_EQ(5, frame);
        EXPECT_EQ(kind == "bezier-catmull" || kind == "ellipse-lissajous" ? 4 : 6, document.keyframe_count());
        const timeline::Curve &up_x = std::get<timeline::Curve>(document.lanes()[11].items().front());
        const timeline::Curve &up_y = std::get<timeline::Curve>(document.lanes()[12].items().front());
        EXPECT_EQ("eye-look-at", up_x.attributes().at("derived-from"));
        EXPECT_TRUE(up_x.samples().empty());
        for (int component = 6; component < 10; ++component)
        {
            const timeline::Item &item = document.lanes()[component].items().front();
            if (std::holds_alternative<timeline::Curve>(item))
            {
                const timeline::Curve &curve = std::get<timeline::Curve>(item);
                EXPECT_TRUE(curve.samples().empty());
                EXPECT_NE(std::string::npos, curve.attributes().at("signal").find("path"));
                EXPECT_FALSE(curve.attributes().at("path").empty());
            }
            else
            {
                EXPECT_FALSE(std::get<timeline::Keyframe>(item).attributes().at("signal").empty());
            }
        }
        for (int sixth_frame = 0; sixth_frame <= 24; ++sixth_frame)
        {
            const timeline::Time time =
                grid.offset() + timeline::Duration::from_ticks(sixth_frame * grid.frame_duration().ticks() / 6);
            const double dx = sample(document.lanes()[8], time) - sample(document.lanes()[6], time);
            const double dy = sample(document.lanes()[9], time) - sample(document.lanes()[7], time);
            EXPECT_NEAR(dx / std::hypot(dx, dy), up_x.sample(time), 1e-12);
            EXPECT_NEAR(dy / std::hypot(dx, dy), up_y.sample(time), 1e-12);
            for (int component = 6; component < 10; ++component)
            {
                const timeline::Item &item = document.lanes()[component].items().front();
                if (std::holds_alternative<timeline::Curve>(item))
                {
                    const timeline::Curve &curve = std::get<timeline::Curve>(item);
                    EXPECT_LE(*curve.minimum(), curve.sample(time));
                    EXPECT_GE(*curve.maximum(), curve.sample(time));
                }
            }
        }
        if (kind == "hold-spiral")
        {
            EXPECT_DOUBLE_EQ(1, sample(document.lanes()[6], grid.frame_start(3)));
            EXPECT_DOUBLE_EQ(-3, sample(document.lanes()[6], grid.frame_start(4)));
        }
        const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
        ASSERT_TRUE(music.succeeded());
        const timeline::Document combined = timeline::combine_documents(document, *music.document);
        EXPECT_EQ(19, combined.lane_count());
        const timeline::Layout layout(combined, timeline::Viewport(500, 600, grid.offset(), grid.end_time()),
            timeline::LayoutMetrics(100, 20, 30, 4));
        EXPECT_NE(std::string::npos, timeline::render_snapshot(layout.display_list()).find("animation-0-eye[0]"));
    }
}

TEST(CameraImport, diagnoses_whole_domain_moving_look_collisions)
{
    const JsonImportResult imported = import_timeline_json("fixtures/partial-camera2d-moving.json");
    ASSERT_TRUE(imported.succeeded());
    ASSERT_EQ(7, timeline::size_cast(imported.diagnostics));
    EXPECT_EQ(15, imported.document->lane_count());
    for (int index = 0; index < 7; ++index)
    {
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find("animation-" + std::to_string(index) + ":"));
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find("singular"));
    }
    EXPECT_NE(std::string::npos, imported.diagnostics[6].find("validation limit"));
    EXPECT_EQ("animation-7-center-mag[0]", imported.document->lanes().front().id());
}

TEST(CameraImport, samples_owned_remaining_eye_paths_like_paranimator)
{
    const std::array<std::string, 4> kinds{"lissajous-independent", "lissajous-slow-y", "bezier", "catmull-rom"};
    for (const std::string &kind : kinds)
    {
        SCOPED_TRACE(kind);
        const timeline::Document document = [&kind]
        {
            const JsonImportResult imported = import_timeline_json("fixtures/camera2d-eye-" + kind + ".json");
            if (!imported.succeeded() || !imported.diagnostics.empty())
            {
                throw std::runtime_error("Camera2D remaining eye path import failed");
            }
            return *imported.document;
        }();
        ASSERT_EQ(15, document.lane_count());
        EXPECT_EQ(6, document.keyframe_count());
        const timeline::FrameGrid &grid = *document.frame_grid();
        std::ifstream golden("fixtures/gold-camera2d-eye-" + kind + ".par");
        ASSERT_TRUE(golden);
        int frame = 0;
        for (std::string line; std::getline(golden, line);)
        {
            const std::size_t start = line.find("center-mag=");
            if (start == std::string::npos)
            {
                continue;
            }
            std::string value = line.substr(start + 11);
            std::replace(value.begin(), value.end(), '/', ' ');
            std::istringstream values(value);
            std::array<double, 6> expected{0, 0, 1, 1, 0, 0};
            for (double &component : expected)
            {
                if (!(values >> component))
                {
                    break;
                }
            }
            const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(document, frame);
            ASSERT_TRUE(inspection);
            for (int component = 0; component < 6; ++component)
            {
                ASSERT_TRUE(inspection->lanes[component].items.front().value);
                const double actual = *inspection->lanes[component].items.front().value;
                const double difference =
                    component == 4 ? std::remainder(actual - expected[component], 360) : actual - expected[component];
                EXPECT_NEAR(0, difference, 1e-9);
            }
            ++frame;
        }
        EXPECT_EQ(5, frame);
        const timeline::Curve &eye_x = std::get<timeline::Curve>(document.lanes()[8].items().front());
        const timeline::Curve &eye_y = std::get<timeline::Curve>(document.lanes()[9].items().front());
        const timeline::Curve &up_x = std::get<timeline::Curve>(document.lanes()[11].items().front());
        const timeline::Curve &up_y = std::get<timeline::Curve>(document.lanes()[12].items().front());
        EXPECT_TRUE(eye_x.samples().empty());
        EXPECT_TRUE(up_x.samples().empty());
        const std::string path_kind = kind.find("lissajous") == 0 ? "lissajous" : kind;
        EXPECT_NE(std::string::npos, eye_x.attributes().at("path").find(path_kind));
        EXPECT_NE(std::string::npos, eye_x.attributes().at("signal").find(path_kind));
        for (int sixth_frame = 0; sixth_frame <= 24; ++sixth_frame)
        {
            const timeline::Time time =
                grid.offset() + timeline::Duration::from_ticks(sixth_frame * grid.frame_duration().ticks() / 6);
            const double dx = eye_x.sample(time) - *document.lanes()[6].evaluate_keyframes(time);
            const double dy = eye_y.sample(time) - *document.lanes()[7].evaluate_keyframes(time);
            EXPECT_NEAR(dx / std::hypot(dx, dy), up_x.sample(time), 1e-12);
            EXPECT_NEAR(dy / std::hypot(dx, dy), up_y.sample(time), 1e-12);
            EXPECT_LE(*eye_x.minimum(), eye_x.sample(time));
            EXPECT_GE(*eye_x.maximum(), eye_x.sample(time));
            EXPECT_LE(*eye_y.minimum(), eye_y.sample(time));
            EXPECT_GE(*eye_y.maximum(), eye_y.sample(time));
        }
        if (kind == "bezier")
        {
            EXPECT_DOUBLE_EQ(1, eye_x.sample(grid.frame_start(2)));
            EXPECT_DOUBLE_EQ(0.25, eye_y.sample(grid.frame_start(2)));
        }
        if (kind == "catmull-rom")
        {
            EXPECT_DOUBLE_EQ(0, eye_x.sample(grid.frame_start(2)));
            EXPECT_DOUBLE_EQ(2.125, eye_y.sample(grid.frame_start(2)));
        }
        const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
        ASSERT_TRUE(music.succeeded());
        const timeline::Document combined = timeline::combine_documents(document, *music.document);
        EXPECT_EQ(19, combined.lane_count());
        const timeline::Layout layout(combined, timeline::Viewport(500, 600, grid.offset(), grid.end_time()),
            timeline::LayoutMetrics(100, 20, 30, 4));
        EXPECT_NE(std::string::npos, timeline::render_snapshot(layout.display_list()).find("animation-0-eye[0]-path"));
    }
}

TEST(CameraImport, diagnoses_remaining_eye_path_collisions_and_validation_limits)
{
    const JsonImportResult result = import_timeline_json("fixtures/partial-camera2d-eye-paths.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(11, timeline::size_cast(result.diagnostics));
    ASSERT_EQ(30, result.document->lane_count());
    for (int index = 0; index < 11; ++index)
    {
        EXPECT_NE(std::string::npos,
            result.diagnostics[index].find("animation-" + std::to_string(index == 10 ? 11 : index) + ":"));
    }
    for (int index = 0; index < 8; ++index)
    {
        EXPECT_NE(std::string::npos, result.diagnostics[index].find("singular"));
    }
    EXPECT_NE(std::string::npos, result.diagnostics[10].find("validation limit"));
    EXPECT_EQ("animation-10-center-mag[0]", result.document->lanes().front().id());
    EXPECT_EQ("animation-12-center-mag[0]", result.document->lanes()[15].id());
}

TEST(CameraImport, samples_owned_equal_frequency_lissajous_eyes_like_paranimator)
{
    const std::array<std::string, 3> kinds{"centered", "phase", "partial"};
    for (const std::string &kind : kinds)
    {
        SCOPED_TRACE(kind);
        const timeline::Document document = [&kind]
        {
            const JsonImportResult imported = import_timeline_json("fixtures/camera2d-eye-lissajous-" + kind + ".json");
            if (!imported.succeeded() || !imported.diagnostics.empty())
            {
                throw std::runtime_error("Camera2D Lissajous eye import failed");
            }
            return *imported.document;
        }();
        ASSERT_EQ(15, document.lane_count());
        EXPECT_EQ(6, document.keyframe_count());
        const timeline::FrameGrid &grid = *document.frame_grid();
        std::ifstream golden("fixtures/gold-camera2d-eye-lissajous-" + kind + ".par");
        ASSERT_TRUE(golden);
        int frame = 0;
        for (std::string line; std::getline(golden, line);)
        {
            const std::size_t start = line.find("center-mag=");
            if (start == std::string::npos)
            {
                continue;
            }
            std::string value = line.substr(start + 11);
            std::replace(value.begin(), value.end(), '/', ' ');
            std::istringstream values(value);
            std::array<double, 6> expected{0, 0, 1, 1, 0, 0};
            for (double &component : expected)
            {
                if (!(values >> component))
                {
                    break;
                }
            }
            const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(document, frame);
            ASSERT_TRUE(inspection);
            for (int component = 0; component < 6; ++component)
            {
                ASSERT_TRUE(inspection->lanes[component].items.front().value);
                const double actual = *inspection->lanes[component].items.front().value;
                const double difference =
                    component == 4 ? std::remainder(actual - expected[component], 360) : actual - expected[component];
                EXPECT_NEAR(0, difference, 1e-9);
            }
            ++frame;
        }
        EXPECT_EQ(5, frame);
        const timeline::Curve &eye_x = std::get<timeline::Curve>(document.lanes()[8].items().front());
        const timeline::Curve &eye_y = std::get<timeline::Curve>(document.lanes()[9].items().front());
        const timeline::Curve &up_x = std::get<timeline::Curve>(document.lanes()[11].items().front());
        const timeline::Curve &up_y = std::get<timeline::Curve>(document.lanes()[12].items().front());
        EXPECT_TRUE(eye_x.samples().empty());
        EXPECT_TRUE(up_x.samples().empty());
        EXPECT_NE(std::string::npos, eye_x.attributes().at("path").find("x-frequency"));
        EXPECT_NE(std::string::npos, eye_x.attributes().at("signal").find("lissajous"));
        if (kind == "phase")
        {
            EXPECT_NE(std::string::npos, eye_x.attributes().at("path").find("phase"));
            EXPECT_NEAR(1 - std::sqrt(2.0), eye_x.sample(grid.frame_start(1)), 1e-12);
            EXPECT_NEAR(-1, eye_y.sample(grid.frame_start(1)), 1e-12);
        }
        for (int half_frame = 0; half_frame <= 8; ++half_frame)
        {
            const timeline::Time time =
                grid.offset() + timeline::Duration::from_ticks(half_frame * grid.frame_duration().ticks() / 2);
            const double dx = eye_x.sample(time) - *document.lanes()[6].evaluate_keyframes(time);
            const double dy = eye_y.sample(time) - *document.lanes()[7].evaluate_keyframes(time);
            EXPECT_NEAR(dx / std::hypot(dx, dy), up_x.sample(time), 1e-12);
            EXPECT_NEAR(dy / std::hypot(dx, dy), up_y.sample(time), 1e-12);
            EXPECT_LE(*eye_x.minimum(), eye_x.sample(time));
            EXPECT_GE(*eye_x.maximum(), eye_x.sample(time));
            EXPECT_LE(*eye_y.minimum(), eye_y.sample(time));
            EXPECT_GE(*eye_y.maximum(), eye_y.sample(time));
        }
        const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
        ASSERT_TRUE(music.succeeded());
        const timeline::Document combined = timeline::combine_documents(document, *music.document);
        EXPECT_EQ(19, combined.lane_count());
        const timeline::Layout layout(combined, timeline::Viewport(500, 600, grid.offset(), grid.end_time()),
            timeline::LayoutMetrics(100, 20, 30, 4));
        EXPECT_NE(std::string::npos, timeline::render_snapshot(layout.display_list()).find("animation-0-eye[0]-path"));
    }
}

TEST(CameraImport, diagnoses_lissajous_eye_collisions_and_invalid_or_pending_inputs)
{
    const JsonImportResult result = import_timeline_json("fixtures/partial-camera2d-eye-lissajous.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(7, timeline::size_cast(result.diagnostics));
    ASSERT_EQ(30, result.document->lane_count());
    for (int index = 0; index < 7; ++index)
    {
        EXPECT_NE(std::string::npos,
            result.diagnostics[index].find("animation-" + std::to_string(index == 6 ? 7 : index) + ":"));
    }
    EXPECT_NE(std::string::npos, result.diagnostics[0].find("singular"));
    EXPECT_NE(std::string::npos, result.diagnostics[1].find("singular"));
    EXPECT_NE(std::string::npos, result.diagnostics[5].find("singular"));
    EXPECT_EQ("animation-6-center-mag[0]", result.document->lanes().front().id());
    EXPECT_EQ("animation-8-center-mag[0]", result.document->lanes()[15].id());
}

TEST(CameraImport, samples_owned_spiral_eyes_like_paranimator)
{
    const std::array<std::string, 4> kinds{"expanding", "shrinking", "offset", "stationary"};
    for (const std::string &kind : kinds)
    {
        SCOPED_TRACE(kind);
        const timeline::Document document = [&kind]
        {
            const JsonImportResult imported = import_timeline_json("fixtures/camera2d-eye-spiral-" + kind + ".json");
            if (!imported.succeeded() || !imported.diagnostics.empty())
            {
                throw std::runtime_error("Camera2D spiral eye import failed");
            }
            return *imported.document;
        }();
        ASSERT_EQ(15, document.lane_count());
        EXPECT_EQ(6, document.keyframe_count());
        const timeline::FrameGrid &grid = *document.frame_grid();
        std::ifstream golden("fixtures/gold-camera2d-eye-spiral-" + kind + ".par");
        ASSERT_TRUE(golden);
        int frame = 0;
        for (std::string line; std::getline(golden, line);)
        {
            const std::size_t start = line.find("center-mag=");
            if (start == std::string::npos)
            {
                continue;
            }
            std::string value = line.substr(start + 11);
            std::replace(value.begin(), value.end(), '/', ' ');
            std::istringstream values(value);
            std::array<double, 6> expected{0, 0, 1, 1, 0, 0};
            for (double &component : expected)
            {
                if (!(values >> component))
                {
                    break;
                }
            }
            const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(document, frame);
            ASSERT_TRUE(inspection);
            for (int component = 0; component < 6; ++component)
            {
                ASSERT_TRUE(inspection->lanes[component].items.front().value);
                const double actual = *inspection->lanes[component].items.front().value;
                // Signed near-zero components can put the same orientation at either end of the angle range.
                const double difference =
                    component == 4 ? std::remainder(actual - expected[component], 360) : actual - expected[component];
                EXPECT_NEAR(0, difference, 1e-9);
            }
            ++frame;
        }
        EXPECT_EQ(5, frame);
        const timeline::Curve &eye_x = std::get<timeline::Curve>(document.lanes()[8].items().front());
        const timeline::Curve &eye_y = std::get<timeline::Curve>(document.lanes()[9].items().front());
        const timeline::Curve &up_x = std::get<timeline::Curve>(document.lanes()[11].items().front());
        const timeline::Curve &up_y = std::get<timeline::Curve>(document.lanes()[12].items().front());
        EXPECT_TRUE(eye_x.samples().empty());
        EXPECT_TRUE(up_x.samples().empty());
        EXPECT_NE(std::string::npos, eye_x.attributes().at("path").find("from-radius"));
        EXPECT_NE(std::string::npos, eye_x.attributes().at("signal").find("spiral"));
        for (int half_frame = 0; half_frame <= 8; ++half_frame)
        {
            const timeline::Time time =
                grid.offset() + timeline::Duration::from_ticks(half_frame * grid.frame_duration().ticks() / 2);
            const double dx = eye_x.sample(time) - *document.lanes()[6].evaluate_keyframes(time);
            const double dy = eye_y.sample(time) - *document.lanes()[7].evaluate_keyframes(time);
            EXPECT_NEAR(dx / std::hypot(dx, dy), up_x.sample(time), 1e-12);
            EXPECT_NEAR(dy / std::hypot(dx, dy), up_y.sample(time), 1e-12);
            EXPECT_LE(*eye_x.minimum(), eye_x.sample(time));
            EXPECT_GE(*eye_x.maximum(), eye_x.sample(time));
            EXPECT_LE(*eye_y.minimum(), eye_y.sample(time));
            EXPECT_GE(*eye_y.maximum(), eye_y.sample(time));
        }
        const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
        ASSERT_TRUE(music.succeeded());
        const timeline::Document combined = timeline::combine_documents(document, *music.document);
        EXPECT_EQ(19, combined.lane_count());
        const timeline::Layout layout(combined, timeline::Viewport(500, 600, grid.offset(), grid.end_time()),
            timeline::LayoutMetrics(100, 20, 30, 4));
        EXPECT_NE(std::string::npos, timeline::render_snapshot(layout.display_list()).find("animation-0-eye[0]-path"));
    }
}

TEST(CameraImport, diagnoses_spiral_eye_collisions_and_invalid_or_pending_inputs)
{
    const JsonImportResult result = import_timeline_json("fixtures/partial-camera2d-eye-spirals.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(7, timeline::size_cast(result.diagnostics));
    ASSERT_EQ(30, result.document->lane_count());
    for (int index = 0; index < 7; ++index)
    {
        EXPECT_NE(std::string::npos, result.diagnostics[index].find("animation-" + std::to_string(index) + ":"));
    }
    for (int index = 0; index < 3; ++index)
    {
        EXPECT_NE(std::string::npos, result.diagnostics[index].find("singular"));
    }
    EXPECT_EQ("animation-7-center-mag[0]", result.document->lanes().front().id());
    EXPECT_EQ("animation-8-center-mag[0]", result.document->lanes()[15].id());
}

TEST(CameraImport, evaluates_owned_offset_eye_orbits_like_paranimator)
{
    const std::array<std::string, 3> kinds{"circle", "ellipse", "partial"};
    for (const std::string &kind : kinds)
    {
        SCOPED_TRACE(kind);
        const timeline::Document document = [&kind]
        {
            const JsonImportResult imported = import_timeline_json("fixtures/camera2d-offset-" + kind + ".json");
            if (!imported.succeeded() || !imported.diagnostics.empty())
            {
                throw std::runtime_error("Camera2D offset orbit import failed");
            }
            return *imported.document;
        }();
        ASSERT_EQ(15, document.lane_count());
        EXPECT_EQ(6, document.keyframe_count());
        const timeline::FrameGrid &grid = *document.frame_grid();
        std::ifstream golden("fixtures/gold-camera2d-offset-" + kind + ".par");
        ASSERT_TRUE(golden);
        int frame = 0;
        for (std::string line; std::getline(golden, line);)
        {
            const std::size_t start = line.find("center-mag=");
            if (start == std::string::npos)
            {
                continue;
            }
            std::string value = line.substr(start + 11);
            std::replace(value.begin(), value.end(), '/', ' ');
            std::istringstream values(value);
            std::array<double, 6> expected{0, 0, 1, 1, 0, 0};
            for (double &component : expected)
            {
                if (!(values >> component))
                {
                    break;
                }
            }
            const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(document, frame);
            ASSERT_TRUE(inspection);
            for (int component = 0; component < 6; ++component)
            {
                ASSERT_TRUE(inspection->lanes[component].items.front().value);
                EXPECT_NEAR(expected[component], *inspection->lanes[component].items.front().value, 1e-9);
            }
            ++frame;
        }
        EXPECT_EQ(5, frame);
        const timeline::Curve &eye_x = std::get<timeline::Curve>(document.lanes()[8].items().front());
        const timeline::Curve &eye_y = std::get<timeline::Curve>(document.lanes()[9].items().front());
        const timeline::Curve &up_x = std::get<timeline::Curve>(document.lanes()[11].items().front());
        const timeline::Curve &up_y = std::get<timeline::Curve>(document.lanes()[12].items().front());
        EXPECT_TRUE(eye_x.samples().empty());
        EXPECT_TRUE(up_x.samples().empty());
        EXPECT_NE(std::string::npos, eye_x.attributes().at("signal").find("path"));
        for (int half_frame = 0; half_frame <= 8; ++half_frame)
        {
            const timeline::Time time =
                grid.offset() + timeline::Duration::from_ticks(half_frame * grid.frame_duration().ticks() / 2);
            const double dx = eye_x.sample(time) - *document.lanes()[6].evaluate_keyframes(time);
            const double dy = eye_y.sample(time) - *document.lanes()[7].evaluate_keyframes(time);
            EXPECT_NEAR(dx / std::hypot(dx, dy), up_x.sample(time), 1e-12);
            EXPECT_NEAR(dy / std::hypot(dx, dy), up_y.sample(time), 1e-12);
            EXPECT_LE(*eye_x.minimum(), eye_x.sample(time));
            EXPECT_GE(*eye_x.maximum(), eye_x.sample(time));
        }
        const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
        ASSERT_TRUE(music.succeeded());
        const timeline::Document combined = timeline::combine_documents(document, *music.document);
        EXPECT_EQ(19, combined.lane_count());
        const timeline::Layout layout(combined, timeline::Viewport(500, 600, grid.offset(), grid.end_time()),
            timeline::LayoutMetrics(100, 20, 30, 4));
        EXPECT_NE(std::string::npos, timeline::render_snapshot(layout.display_list()).find("animation-0-eye[0]-path"));
    }
}

TEST(CameraImport, diagnoses_offset_orbit_collisions_between_frames_and_pending_motion)
{
    const JsonImportResult result = import_timeline_json("fixtures/partial-camera2d-offset-orbits.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(6, timeline::size_cast(result.diagnostics));
    ASSERT_EQ(30, result.document->lane_count());
    for (int index = 0; index < 6; ++index)
    {
        EXPECT_NE(std::string::npos,
            result.diagnostics[index].find("animation-" + std::to_string(index >= 4 ? index + 1 : index) + ":"));
    }
    for (int index = 0; index < 4; ++index)
    {
        EXPECT_NE(std::string::npos, result.diagnostics[index].find("singular"));
    }
    EXPECT_EQ("animation-4-center-mag[0]", result.document->lanes().front().id());
    EXPECT_EQ("animation-7-center-mag[0]", result.document->lanes()[15].id());
}

TEST(CameraImport, samples_owned_curved_look_at_like_paranimator_with_full_path_bounds)
{
    const std::array<std::string, 6> kinds{"circle", "ellipse", "lissajous", "spiral", "bezier", "catmull-rom"};
    for (const std::string &kind : kinds)
    {
        SCOPED_TRACE(kind);
        const timeline::Document document = [&kind]
        {
            const JsonImportResult imported = import_timeline_json("fixtures/camera2d-look-" + kind + ".json");
            if (!imported.succeeded() || !imported.diagnostics.empty())
            {
                throw std::runtime_error("Camera2D curved look-at import failed");
            }
            return *imported.document;
        }();
        ASSERT_EQ(11, document.lane_count());
        EXPECT_EQ(4, document.keyframe_count());
        const timeline::FrameGrid &grid = *document.frame_grid();
        std::ifstream golden("fixtures/gold-camera2d-look-" + kind + ".par");
        ASSERT_TRUE(golden);
        int frame = 0;
        for (std::string line; std::getline(golden, line);)
        {
            const std::size_t start = line.find("center-mag=");
            if (start == std::string::npos)
            {
                continue;
            }
            std::string value = line.substr(start + 11);
            std::replace(value.begin(), value.end(), '/', ' ');
            std::istringstream values(value);
            std::array<double, 6> expected{0, 0, 1, 1, 0, 0};
            for (double &component : expected)
            {
                if (!(values >> component))
                {
                    break;
                }
            }
            const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(document, frame);
            ASSERT_TRUE(inspection);
            for (int component = 0; component < 6; ++component)
            {
                ASSERT_FALSE(inspection->lanes[component].items.empty());
                ASSERT_TRUE(inspection->lanes[component].items.front().value);
                EXPECT_NEAR(expected[component], *inspection->lanes[component].items.front().value, 1e-10);
            }
            ++frame;
        }
        EXPECT_EQ(5, frame);
        for (int component = 0; component < 2; ++component)
        {
            const timeline::Curve &center = std::get<timeline::Curve>(document.lanes()[component].items().front());
            const timeline::Curve &look = std::get<timeline::Curve>(document.lanes()[component + 6].items().front());
            EXPECT_TRUE(center.samples().empty());
            EXPECT_TRUE(look.samples().empty());
            EXPECT_EQ(look.minimum(), center.minimum());
            EXPECT_EQ(look.maximum(), center.maximum());
            ASSERT_TRUE(center.minimum());
            ASSERT_TRUE(center.maximum());
            EXPECT_NE(std::string::npos, look.attributes().at("path").find(kind));
            EXPECT_NE(std::string::npos, look.attributes().at("signal").find(kind));
            const timeline::Time between =
                grid.offset() + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2);
            EXPECT_DOUBLE_EQ(look.sample(between), center.sample(between));
            EXPECT_LE(*center.minimum(), center.sample(grid.frame_start(2)));
            EXPECT_GE(*center.maximum(), center.sample(grid.frame_start(2)));
        }
        const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
        ASSERT_TRUE(music.succeeded());
        const timeline::Document combined = timeline::combine_documents(document, *music.document);
        EXPECT_EQ(15, combined.lane_count());
        const timeline::Layout layout(combined, timeline::Viewport(500, 600, grid.offset(), grid.end_time()),
            timeline::LayoutMetrics(100, 20, 30, 4));
        EXPECT_NE(
            std::string::npos, timeline::render_snapshot(layout.display_list()).find("animation-0-look-at[0]-path"));
    }
}

TEST(CameraImport, diagnoses_invalid_curved_look_at_and_retains_eye_compositions)
{
    const JsonImportResult result = import_timeline_json("fixtures/partial-camera2d-look-paths.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(7, timeline::size_cast(result.diagnostics));
    ASSERT_EQ(26, result.document->lane_count());
    for (int index = 0; index < 7; ++index)
    {
        EXPECT_NE(std::string::npos,
            result.diagnostics[index].find("animation-" + std::to_string(index == 6 ? 7 : index) + ":"));
    }
    EXPECT_EQ("animation-6-center-mag[0]", result.document->lanes().front().id());
    EXPECT_EQ("animation-8-center-mag[0]", result.document->lanes()[15].id());
}

TEST(CameraImport, composes_straight_paths_like_paranimator_and_owns_their_recipes)
{
    const std::array<std::string, 2> fixtures{"camera2d-straight-paths", "camera2d-straight-eye"};
    for (const std::string &fixture : fixtures)
    {
        SCOPED_TRACE(fixture);
        const timeline::Document document = [&fixture]
        {
            const JsonImportResult imported = import_timeline_json("fixtures/" + fixture + ".json");
            if (!imported.succeeded() || !imported.diagnostics.empty())
            {
                throw std::runtime_error("Camera2D straight paths import failed");
            }
            return *imported.document;
        }();
        ASSERT_EQ(fixture == fixtures.front() ? 11 : 15, document.lane_count());
        const timeline::FrameGrid &grid = *document.frame_grid();
        std::ifstream golden("fixtures/gold-" + fixture + ".par");
        ASSERT_TRUE(golden);
        int frame = 0;
        for (std::string line; std::getline(golden, line);)
        {
            const std::size_t start = line.find("center-mag=");
            if (start == std::string::npos)
            {
                continue;
            }
            std::string value = line.substr(start + 11);
            std::replace(value.begin(), value.end(), '/', ' ');
            std::istringstream values(value);
            std::array<double, 6> expected{0, 0, 1, 1, 0, 0};
            for (double &component : expected)
            {
                if (!(values >> component))
                {
                    break;
                }
            }
            const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(document, frame);
            ASSERT_TRUE(inspection);
            for (int component = 0; component < 6; ++component)
            {
                ASSERT_FALSE(inspection->lanes[component].items.empty());
                ASSERT_TRUE(inspection->lanes[component].items.front().value);
                EXPECT_NEAR(expected[component], *inspection->lanes[component].items.front().value, 1e-10);
            }
            ++frame;
        }
        EXPECT_EQ(5, frame);
        const timeline::Keyframe &look = std::get<timeline::Keyframe>(document.lanes()[6].items().front());
        EXPECT_NE(std::string::npos, look.attributes().at("path").find("line"));
        EXPECT_NE(std::string::npos, look.attributes().at("signal").find("path"));
        EXPECT_EQ("animation-0-look-at-key-0", look.id());
        const timeline::Time between =
            grid.offset() + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2);
        EXPECT_DOUBLE_EQ(fixture == fixtures.front() ? 0.5 : 0.25, *document.lanes()[6].evaluate_keyframes(between));
        if (fixture != fixtures.front())
        {
            const timeline::Keyframe &eye = std::get<timeline::Keyframe>(document.lanes()[8].items().front());
            EXPECT_NE(std::string::npos, eye.attributes().at("path").find("constant"));
            EXPECT_DOUBLE_EQ(0, *document.lanes()[8].evaluate_keyframes(between));
        }
        const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
        ASSERT_TRUE(music.succeeded());
        const timeline::Document combined = timeline::combine_documents(document, *music.document);
        EXPECT_EQ(document.lane_count() + 4, combined.lane_count());
        const timeline::Layout layout(combined, timeline::Viewport(500, 600, grid.offset(), grid.end_time()),
            timeline::LayoutMetrics(100, 20, 30, 4));
        EXPECT_NE(std::string::npos, timeline::render_snapshot(layout.display_list()).find("animation-0-look-at"));
    }
}

TEST(CameraImport, diagnoses_invalid_straight_compositions_without_partial_camera_lanes)
{
    const JsonImportResult result = import_timeline_json("fixtures/partial-camera2d-straight-paths.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(7, timeline::size_cast(result.diagnostics));
    ASSERT_EQ(11, result.document->lane_count());
    for (int index = 0; index < 7; ++index)
    {
        EXPECT_NE(std::string::npos, result.diagnostics[index].find("animation-" + std::to_string(index) + ":"));
    }
    EXPECT_EQ("animation-7-center-mag[0]", result.document->lanes().front().id());
}

TEST(CameraImport, normalizes_tiny_authored_view_up_before_cleaning_components)
{
    const JsonImportResult result = import_timeline_json("fixtures/camera2d-tiny-view-up.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.diagnostics.empty());
    const timeline::FrameGrid &grid = *result.document->frame_grid();
    const timeline::Curve &up = std::get<timeline::Curve>(result.document->lanes()[9].items().front());
    EXPECT_DOUBLE_EQ(1, up.sample(grid.frame_start(2)));
    const timeline::Curve &rotation = std::get<timeline::Curve>(result.document->lanes()[4].items().front());
    EXPECT_DOUBLE_EQ(0, rotation.sample(grid.frame_start(2)));
}

TEST(CameraImport, derives_direction_from_analytic_eye_like_paranimator)
{
    const JsonImportResult result = import_timeline_json("fixtures/camera2d-eye-center-mag.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.diagnostics.empty());
    ASSERT_EQ(13, result.document->lane_count());
    EXPECT_EQ(4, result.document->keyframe_count());
    const timeline::FrameGrid &grid = *result.document->frame_grid();
    std::ifstream golden("fixtures/gold-camera2d-eye-center-mag.par");
    ASSERT_TRUE(golden);
    int frame = 0;
    for (std::string line; std::getline(golden, line);)
    {
        const std::size_t start = line.find("center-mag=");
        if (start == std::string::npos)
        {
            continue;
        }
        std::string value = line.substr(start + 11);
        std::replace(value.begin(), value.end(), '/', ' ');
        std::istringstream values(value);
        std::array<double, 6> expected{0, 0, 1, 1, 0, 0};
        for (double &component : expected)
        {
            if (!(values >> component))
            {
                break;
            }
        }
        const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, frame);
        ASSERT_TRUE(inspection);
        for (int component = 0; component < 6; ++component)
        {
            ASSERT_TRUE(inspection->lanes[component].items.front().value);
            EXPECT_NEAR(expected[component], *inspection->lanes[component].items.front().value, 1e-11);
        }
        ++frame;
    }
    EXPECT_EQ(5, frame);
    const timeline::Curve &eye = std::get<timeline::Curve>(result.document->lanes()[8].items().front());
    EXPECT_TRUE(eye.samples().empty());
    EXPECT_EQ("animation-0-eye[0]", result.document->lanes()[8].id());
    EXPECT_NE(std::string::npos, eye.attributes().at("path").find("circle"));
    const timeline::Curve &up = std::get<timeline::Curve>(result.document->lanes()[11].items().front());
    EXPECT_EQ("eye-look-at", up.attributes().at("derived-from"));
    EXPECT_DOUBLE_EQ(-1, up.sample(grid.frame_start(1)));
    const timeline::Time between = grid.offset() + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2);
    EXPECT_NEAR(-std::sqrt(0.5), eye.sample(between), 1e-12);
    const timeline::Layout layout(*result.document, timeline::Viewport(500, 460, grid.offset(), grid.end_time()),
        timeline::LayoutMetrics(100, 20, 30, 4));
    const std::optional<timeline::HitResult> hit = layout.hit_test({180, 286}, 2);
    ASSERT_TRUE(hit);
    EXPECT_EQ("animation-0-eye[0]-path", hit->id.item_id);
    EXPECT_NE(std::string::npos, timeline::render_snapshot(layout.display_list()).find("derived-view-up"));
}

TEST(CameraImport, preserves_eye_precedence_and_owned_orbits_and_keyed_motion)
{
    const timeline::Document document = []
    {
        const JsonImportResult imported = import_timeline_json("fixtures/camera2d-eye-variants.json");
        if (!imported.succeeded() || !imported.diagnostics.empty())
        {
            throw std::runtime_error("Camera2D eye variants import failed");
        }
        return *imported.document;
    }();
    ASSERT_EQ(28, document.lane_count());
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Curve &rotation = std::get<timeline::Curve>(document.lanes()[4].items().front());
    EXPECT_DOUBLE_EQ(90, rotation.sample(grid.frame_start(1)));
    const timeline::Curve &eye = std::get<timeline::Curve>(document.lanes()[8].items().front());
    EXPECT_DOUBLE_EQ(4, eye.sample(grid.frame_start(1)));
    EXPECT_EQ("orbit.eye", eye.attributes().at("parameter"));
    const timeline::Curve &authored_up = std::get<timeline::Curve>(document.lanes()[13].items().front());
    EXPECT_DOUBLE_EQ(0, authored_up.sample(grid.frame_start(1)));
    EXPECT_EQ("orbit.view-up", authored_up.attributes().at("parameter"));
    const timeline::Curve &moving_rotation = std::get<timeline::Curve>(document.lanes()[19].items().front());
    EXPECT_DOUBLE_EQ(0, moving_rotation.sample(grid.frame_start(2)));
    const timeline::Curve &moving_up = std::get<timeline::Curve>(document.lanes()[26].items().front());
    EXPECT_DOUBLE_EQ(0, moving_up.sample(grid.frame_start(2)));
    EXPECT_DOUBLE_EQ(2, *document.lanes()[21].evaluate_keyframes(grid.frame_start(2)));
    EXPECT_DOUBLE_EQ(2, *document.lanes()[23].evaluate_keyframes(grid.frame_start(2)));
    const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
    ASSERT_TRUE(music.succeeded());
    const timeline::Document combined = timeline::combine_documents(*music.document, document);
    EXPECT_EQ(32, combined.lane_count());
    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(combined, 1);
    ASSERT_TRUE(inspection);
    EXPECT_DOUBLE_EQ(90, *inspection->lanes[8].items.front().value);
}

TEST(CameraImport, diagnoses_singular_eye_directions_and_pending_compositions)
{
    const JsonImportResult result = import_timeline_json("fixtures/partial-camera2d-eye.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(8, timeline::size_cast(result.diagnostics));
    ASSERT_EQ(13, result.document->lane_count());
    for (int index = 0; index < 8; ++index)
    {
        EXPECT_NE(std::string::npos, result.diagnostics[index].find("animation-" + std::to_string(index) + ":"));
    }
    EXPECT_EQ("animation-8-center-mag[0]", result.document->lanes().front().id());
}

TEST(CameraImport, evaluates_center_mag_and_nested_keys_like_paranimator)
{
    const JsonImportResult result = import_timeline_json("fixtures/camera2d-center-mag.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.diagnostics.empty());
    ASSERT_EQ(11, result.document->lane_count());
    EXPECT_EQ(1, result.document->track_count());
    EXPECT_EQ(6, result.document->keyframe_count());
    const timeline::FrameGrid &grid = *result.document->frame_grid();
    std::ifstream golden("fixtures/gold-camera2d-center-mag.par");
    ASSERT_TRUE(golden);
    int frame = 0;
    for (std::string line; std::getline(golden, line);)
    {
        const std::size_t start = line.find("center-mag=");
        if (start == std::string::npos)
        {
            continue;
        }
        std::string value = line.substr(start + 11);
        std::replace(value.begin(), value.end(), '/', ' ');
        std::istringstream values(value);
        const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, frame);
        ASSERT_TRUE(inspection);
        for (int component = 0; component < 3; ++component)
        {
            double expected = 0;
            ASSERT_TRUE(values >> expected);
            ASSERT_EQ(1, timeline::size_cast(inspection->lanes[component].items));
            ASSERT_TRUE(inspection->lanes[component].items.front().value);
            EXPECT_NEAR(expected, *inspection->lanes[component].items.front().value, 1e-11);
        }
        ASSERT_TRUE(inspection->lanes[10].value);
        EXPECT_NEAR(3 * std::pow(0.5, frame / 2.0), *inspection->lanes[10].value, 1e-12);
        ++frame;
    }
    EXPECT_EQ(3, frame);
    const timeline::Curve &magnification = std::get<timeline::Curve>(result.document->lanes()[2].items().front());
    EXPECT_EQ(timeline::CurveInterpolation::ANALYTIC, magnification.interpolation());
    EXPECT_TRUE(magnification.samples().empty());
    EXPECT_EQ(grid.frame_start(2), magnification.end());
    EXPECT_EQ("center-mag", magnification.attributes().at("parameter"));
    EXPECT_EQ("magnification", magnification.attributes().at("component"));
    EXPECT_EQ("Mandel_Demo", magnification.attributes().at("source-entry"));
    EXPECT_NE(std::string::npos, magnification.attributes().at("camera2d").find("geometric"));
    EXPECT_EQ("animation-0-look-at[0]", result.document->lanes()[6].id());
    const timeline::Layout layout(*result.document, timeline::Viewport(500, 380, grid.offset(), grid.end_time()),
        timeline::LayoutMetrics(100, 20, 30, 4));
    const std::optional<timeline::HitResult> hit = layout.hit_test({233, 35}, 2);
    ASSERT_TRUE(hit);
    EXPECT_EQ("animation-0-center-mag[0]-camera", hit->id.item_id);
    EXPECT_NE(std::string::npos, timeline::render_snapshot(layout.display_list()).find("camera / height"));
}

TEST(CameraImport, normalizes_interpolated_view_up_and_preserves_source_stretch)
{
    const timeline::Document document = []
    {
        const JsonImportResult imported = import_timeline_json("fixtures/camera2d-keyed-variants.json");
        if (!imported.succeeded() || !imported.diagnostics.empty())
        {
            throw std::runtime_error("Camera2D import failed");
        }
        return *imported.document;
    }();
    ASSERT_EQ(22, document.lane_count());
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Curve &rotation = std::get<timeline::Curve>(document.lanes()[4].items().front());
    const timeline::Curve &up_x = std::get<timeline::Curve>(document.lanes()[8].items().front());
    const timeline::Curve &up_y = std::get<timeline::Curve>(document.lanes()[9].items().front());
    EXPECT_NEAR(45, rotation.sample(grid.frame_start(1)), 1e-12);
    EXPECT_NEAR(std::sqrt(0.5), up_x.sample(grid.frame_start(1)), 1e-12);
    EXPECT_NEAR(std::sqrt(0.5), up_y.sample(grid.frame_start(1)), 1e-12);
    EXPECT_NEAR(90, rotation.sample(grid.frame_start(2)), 1e-12);
    const timeline::Curve &magnification = std::get<timeline::Curve>(document.lanes()[2].items().front());
    EXPECT_DOUBLE_EQ(2, magnification.sample(grid.offset()));
    EXPECT_NEAR(4.0 / 3.0, magnification.sample(grid.frame_start(1)), 1e-12);
    EXPECT_DOUBLE_EQ(1, magnification.sample(grid.frame_start(2)));
    const timeline::Curve &stretch = std::get<timeline::Curve>(document.lanes()[3].items().front());
    EXPECT_DOUBLE_EQ(-2, stretch.sample(grid.offset()));
    const timeline::Curve &held = std::get<timeline::Curve>(document.lanes()[13].items().front());
    EXPECT_DOUBLE_EQ(2, held.sample(grid.frame_start(1)));
    EXPECT_DOUBLE_EQ(1, held.sample(grid.frame_start(2)));
    const timeline::Time between = grid.offset() + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2);
    EXPECT_NEAR(18.43494882292201, rotation.sample(between), 1e-12);
    EXPECT_NE(std::string::npos, up_x.attributes().at("signal").find("0/2"));
    const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
    ASSERT_TRUE(music.succeeded());
    const timeline::Document combined = timeline::combine_documents(*music.document, document);
    EXPECT_EQ(26, combined.lane_count());
    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(combined, 1);
    ASSERT_TRUE(inspection);
    ASSERT_TRUE(inspection->lanes[8].items.front().value);
    EXPECT_NEAR(45, *inspection->lanes[8].items.front().value, 1e-12);
}

TEST(CameraImport, diagnoses_invalid_and_pending_forms_without_partial_camera_lanes)
{
    const JsonImportResult result = import_timeline_json("fixtures/partial-camera2d.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(14, timeline::size_cast(result.diagnostics));
    ASSERT_EQ(11, result.document->lane_count());
    for (int index = 0; index < 14; ++index)
    {
        EXPECT_NE(std::string::npos, result.diagnostics[index].find("animation-" + std::to_string(index) + ":"));
    }
    EXPECT_EQ("animation-14-center-mag[0]", result.document->lanes().front().id());
}

TEST(CameraImport, resolves_layer_views_and_diagnoses_unusable_source_entries)
{
    const JsonImportResult result = import_timeline_json("fixtures/camera2d-layer-sources.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(22, result.document->lane_count());
    ASSERT_EQ(3, timeline::size_cast(result.diagnostics));
    for (int index = 0; index < 3; ++index)
    {
        EXPECT_NE(
            std::string::npos, result.diagnostics[index].find("animation-layer-" + std::to_string(index + 2) + "-0:"));
    }
    const timeline::FrameGrid &grid = *result.document->frame_grid();
    const timeline::Curve &stretch = std::get<timeline::Curve>(result.document->lanes()[3].items().front());
    EXPECT_DOUBLE_EQ(-2, stretch.sample(grid.offset()));
    EXPECT_EQ("camera-0", stretch.attributes().at("layer"));
    const timeline::Curve &default_stretch = std::get<timeline::Curve>(result.document->lanes()[14].items().front());
    EXPECT_DOUBLE_EQ(1, default_stretch.sample(grid.offset()));
    const timeline::Curve &magnification = std::get<timeline::Curve>(result.document->lanes()[13].items().front());
    EXPECT_NEAR(std::sqrt(2.0), magnification.sample(grid.frame_start(1)), 1e-12);
    EXPECT_EQ("Zero_Demo", magnification.attributes().at("source-entry"));
}
