// Copyright (c) 2026 Richard Thomson

#include <timeline/Layout.h>
#include <timeline/Query.h>
#include <timeline/Snapshot.h>
#include <timelineParAnimator/TimelineJson.h>

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <variant>

using namespace timeline_par_animator;

TEST(AnimationImport, samples_constant_and_line_paths_like_paranimator)
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

TEST(AnimationImport, diagnoses_invalid_paths_without_discarding_a_valid_constant)
{
    const JsonImportResult result = import_timeline_json("fixtures/partial-paths.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(7, timeline::size_cast(result.diagnostics));
    ASSERT_EQ(1, result.document->lane_count());
    EXPECT_EQ("animation-7", result.document->lanes().front().id());
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

TEST(AnimationImport, rejects_paths_with_fewer_than_two_frames)
{
    const JsonImportResult result = import_timeline_json("fixtures/single-frame-path.json");
    EXPECT_FALSE(result.succeeded());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_NE(std::string::npos, result.diagnostics.front().find("at least two frames"));
}

TEST(AnimationImport, translates_destination_curve_to_outgoing_segment)
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

TEST(AnimationImport, splits_compound_parameters_without_losing_authored_values)
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

TEST(AnimationImport, retains_layer_identity_and_drives_display_and_inspection)
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

TEST(AnimationImport, combines_music_generated_output_and_animation_without_identity_collisions)
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

TEST(AnimationImport, rejects_incompatible_comparison_timing)
{
    const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
    JsonImportOptions options;
    options.frames_per_second_numerator = 10;
    const JsonImportResult animation = import_timeline_json("fixtures/maxiter.json", options);
    ASSERT_TRUE(music.succeeded());
    ASSERT_TRUE(animation.succeeded());
    EXPECT_THROW(timeline::combine_documents(*music.document, *animation.document), std::invalid_argument);
}

TEST(AnimationImport, preserves_categorical_values_as_held_spans_and_key_instants)
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

TEST(AnimationImport, retains_valid_tracks_with_indexed_diagnostics_for_invalid_keys)
{
    const JsonImportResult result = import_timeline_json("fixtures/partial-animation.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(1, result.document->lane_count());
    ASSERT_EQ(6, timeline::size_cast(result.diagnostics));
    for (int index = 0; index < 6; ++index)
    {
        EXPECT_NE(std::string::npos, result.diagnostics[index].find("animation-" + std::to_string(index)));
    }
    EXPECT_EQ("animation-6", result.document->lanes().front().id());
}

TEST(AnimationImport, rejects_unusable_animation_and_missing_catalogs)
{
    for (const char *name : {"invalid-animation.json", "missing-catalog.json"})
    {
        const JsonImportResult result = import_timeline_json(std::filesystem::path("fixtures") / name);
        EXPECT_FALSE(result.succeeded()) << name;
        EXPECT_FALSE(result.diagnostics.empty()) << name;
    }
}
