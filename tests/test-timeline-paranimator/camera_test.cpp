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
