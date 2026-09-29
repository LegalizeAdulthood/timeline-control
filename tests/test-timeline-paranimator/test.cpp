// Copyright (c) 2026 Richard Thomson

#include <timelineParAnimator/TimelineJson.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <variant>

using namespace timeline_par_animator;

namespace
{

std::filesystem::path fixture_path(const char *name)
{
    return std::filesystem::path("fixtures") / name;
}

const JsonImportOptions TEST_OPTIONS{
    24000,
    24,
    1,
};

JsonImportOptions tracker_options()
{
    auto options = JsonImportOptions{};
    options.beat_keys_config_path = fixture_path("beat-keys/adapter.beat-keys.json");
    return options;
}

} // namespace

TEST(TimelineJson, imports_minimal_paranimator_config)
{
    const auto source_path = fixture_path("maxiter.json");
    auto result = import_timeline_json(source_path, TEST_OPTIONS);

    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.document.has_value());
    EXPECT_TRUE(result.diagnostics.empty());
    EXPECT_EQ("maxiter.json", result.document->metadata().title());
    EXPECT_EQ(source_path.string(), result.document->metadata().description());
    EXPECT_EQ(24000, result.document->timebase().ticks_per_second());
    ASSERT_TRUE(result.document->frame_grid().has_value());
    EXPECT_EQ(3, result.document->frame_grid()->frame_count());
    EXPECT_EQ(24, result.document->frame_grid()->frames_per_second_numerator());
    EXPECT_EQ(1, result.document->frame_grid()->frames_per_second_denominator());
    EXPECT_EQ(1, result.document->track_count());
    EXPECT_EQ(2, result.document->keyframe_count());
    EXPECT_TRUE(result.document->lanes_empty());
}

TEST(TimelineJson, reports_multi_track_counts)
{
    const auto result = import_timeline_json(fixture_path("multi-track.json"), TEST_OPTIONS);

    ASSERT_TRUE(result.succeeded());
    EXPECT_EQ(3, result.document->frame_grid()->frame_count());
    EXPECT_EQ(2, result.document->track_count());
    EXPECT_EQ(4, result.document->keyframe_count());
}

TEST(TimelineJson, rejects_invalid_schema)
{
    const auto result = import_timeline_json(fixture_path("invalid-schema.json"), TEST_OPTIONS);

    EXPECT_FALSE(result.succeeded());
    EXPECT_FALSE(result.document.has_value());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_NE(std::string::npos, result.diagnostics.front().find("unexpected"));
}

TEST(TimelineJson, rejects_empty_source_path)
{
    const auto result = import_timeline_json({}, TEST_OPTIONS);

    EXPECT_FALSE(result.succeeded());
    EXPECT_FALSE(result.document.has_value());
    ASSERT_EQ(1U, result.diagnostics.size());
    EXPECT_EQ("No JSON file was selected.", result.diagnostics.front());
}

TEST(TimelineJson, imports_tracker_timeline_with_adjacent_config)
{
    const auto result = import_timeline_json(fixture_path("beat-keys/timeline-events.json"), tracker_options());

    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.document->source_summary().has_value());
    const auto &summary = *result.document->source_summary();
    EXPECT_EQ("par-beatdown.tracker-timeline", summary.schema());
    EXPECT_EQ(1, summary.schema_version());
    EXPECT_EQ(0, summary.feature_count());
    EXPECT_EQ(6, summary.event_count());
    EXPECT_EQ(0, *summary.first_frame());
    EXPECT_EQ(4, *summary.last_frame());
    EXPECT_EQ(0, summary.first_time()->ticks());
    EXPECT_EQ(16000, summary.last_time()->ticks());
    EXPECT_EQ(0, summary.frame_offset()->ticks());
    ASSERT_TRUE(result.document->frame_grid().has_value());
    EXPECT_EQ(5, result.document->frame_grid()->frame_count());
    EXPECT_EQ(30, result.document->frame_grid()->frames_per_second_numerator());
    EXPECT_EQ(1, result.document->frame_grid()->frames_per_second_denominator());
    ASSERT_EQ(1, result.document->lane_count());
    const auto &lane = result.document->lanes().front();
    EXPECT_EQ("tracker-events", lane.id());
    EXPECT_EQ("Music events", lane.label());
    ASSERT_EQ(6, lane.item_count());
    EXPECT_EQ("note", std::get<timeline::Instant>(lane.items()[0]).kind());
    EXPECT_EQ(0, std::get<timeline::Instant>(lane.items()[0]).time().ticks());
    EXPECT_EQ("effect", std::get<timeline::Instant>(lane.items()[2]).kind());
    EXPECT_EQ(8000, std::get<timeline::Instant>(lane.items()[2]).time().ticks());
    EXPECT_EQ("row", std::get<timeline::Instant>(lane.items()[3]).kind());
    EXPECT_EQ(12000, std::get<timeline::Instant>(lane.items()[4]).time().ticks());
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TimelineJson, imports_full_tracker_timeline_summary)
{
    const auto result = import_timeline_json(fixture_path("par-beatdown/gold-write-timeline-clock.json"));

    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.document->source_summary().has_value());
    const auto &summary = *result.document->source_summary();
    EXPECT_EQ(0, summary.feature_count());
    EXPECT_EQ(1891, summary.event_count());
    EXPECT_EQ(0, *summary.first_frame());
    EXPECT_EQ(4908, *summary.last_frame());
    EXPECT_EQ(0, summary.first_time()->ticks());
    EXPECT_EQ(19631430, summary.last_time()->ticks());
    ASSERT_TRUE(result.document->frame_grid().has_value());
    EXPECT_EQ(4909, result.document->frame_grid()->frame_count());
    EXPECT_EQ(30, result.document->frame_grid()->frames_per_second_numerator());
    EXPECT_EQ(1, result.document->frame_grid()->frames_per_second_denominator());
    ASSERT_EQ(1, result.document->lane_count());
    EXPECT_EQ(1891, result.document->lanes().front().item_count());
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TimelineJson, imports_tracker_rms_curve)
{
    const auto result = import_timeline_json(fixture_path("par-beatdown/gold-write-windowed-features.json"));

    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.document->frame_grid().has_value());
    EXPECT_EQ(46, result.document->frame_grid()->frame_count());
    ASSERT_EQ(1, result.document->lane_count());
    const auto &lane = result.document->lanes().front();
    EXPECT_EQ("tracker-rms", lane.id());
    EXPECT_EQ("RMS", lane.label());
    ASSERT_EQ(1, lane.item_count());
    const auto &curve = std::get<timeline::Curve>(lane.items().front());
    ASSERT_EQ(4, curve.sample_count());
    EXPECT_EQ(60000, curve.samples()[1].time().ticks());
    EXPECT_DOUBLE_EQ(0.11958, curve.samples()[1].value());
    EXPECT_DOUBLE_EQ(0.0, *curve.minimum());
    EXPECT_DOUBLE_EQ(1.0, *curve.maximum());
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TimelineJson, preserves_tracker_diagnostics_outside_core)
{
    const auto result = import_timeline_json(fixture_path("beat-keys/timeline-diagnostics.json"));

    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.document->source_summary().has_value());
    EXPECT_EQ(3U, result.diagnostics.size());
    EXPECT_EQ("Warning: tempo was approximated", result.diagnostics[0]);
    EXPECT_EQ("Unsupported: effect command 0x7f", result.diagnostics[1]);
    EXPECT_EQ("Log: loaded fixture", result.diagnostics[2]);
}

TEST(TimelineJson, imports_row_pulse_overlay_summary)
{
    const auto result = import_timeline_json(fixture_path("beat-keys/gold-write-row-pulses.json"));

    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.document->source_summary().has_value());
    const auto &source = *result.document->source_summary();
    EXPECT_EQ("par-beatdown.beat-keys-overlay", source.schema());
    EXPECT_EQ(0, *source.first_frame());
    EXPECT_EQ(4, *source.last_frame());
    ASSERT_TRUE(source.generation_summary().has_value());
    const auto &generation = *source.generation_summary();
    EXPECT_EQ("beat-keys", generation.generator_name());
    EXPECT_EQ("0.1.0", generation.generator_version());
    ASSERT_EQ(3U, generation.source_references().size());
    EXPECT_EQ("base_animation", generation.source_references()[0].role());
    EXPECT_EQ("tests/beat-keys/base-animation.json", generation.source_references()[0].location());
    EXPECT_EQ("timeline", generation.source_references()[1].role());
    EXPECT_EQ("tests/beat-keys/timeline-events.json", generation.source_references()[1].location());
    EXPECT_EQ("adapter_config", generation.source_references()[2].role());
    EXPECT_EQ("tests/beat-keys/row-pulses.beat-keys.json", generation.source_references()[2].location());
    ASSERT_EQ(1U, generation.target_counts().size());
    EXPECT_EQ("row.flash", generation.target_counts()[0].name());
    EXPECT_EQ(4, generation.target_counts()[0].count());
    ASSERT_EQ(1U, generation.source_counts().size());
    EXPECT_EQ("music.row_pulse", generation.source_counts()[0].name());
    EXPECT_EQ(4, generation.source_counts()[0].count());
    EXPECT_EQ(1, result.document->track_count());
    EXPECT_EQ(4, result.document->keyframe_count());
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TimelineJson, imports_rms_overlay_summary)
{
    const auto result = import_timeline_json(fixture_path("beat-keys/gold-write-rms-keyframes.json"));

    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.document->source_summary().has_value());
    const auto &source = *result.document->source_summary();
    ASSERT_TRUE(source.generation_summary().has_value());
    const auto &generation = *source.generation_summary();
    ASSERT_EQ(3U, generation.target_counts().size());
    EXPECT_EQ("camera.zoom", generation.target_counts()[0].name());
    EXPECT_EQ(3, generation.target_counts()[0].count());
    EXPECT_EQ("color.brightness", generation.target_counts()[1].name());
    EXPECT_EQ(3, generation.target_counts()[1].count());
    EXPECT_EQ("layer.opacity", generation.target_counts()[2].name());
    EXPECT_EQ(3, generation.target_counts()[2].count());
    ASSERT_EQ(1U, generation.source_counts().size());
    EXPECT_EQ("music.rms", generation.source_counts()[0].name());
    EXPECT_EQ(9, generation.source_counts()[0].count());
    EXPECT_EQ(3, result.document->track_count());
    EXPECT_EQ(9, result.document->keyframe_count());
    ASSERT_TRUE(result.document->frame_grid().has_value());
    EXPECT_EQ(5, result.document->frame_grid()->frame_count());
    ASSERT_EQ(3, result.document->lane_count());
    const auto &zoom = result.document->lanes()[0];
    EXPECT_EQ("camera.zoom", zoom.id());
    EXPECT_EQ("camera.zoom", zoom.label());
    ASSERT_EQ(3, zoom.item_count());
    const auto &first = std::get<timeline::Keyframe>(zoom.items()[0]);
    EXPECT_EQ(0, first.time().ticks());
    EXPECT_DOUBLE_EQ(0.25, first.value());
    EXPECT_EQ(timeline::KeyframeInterpolation::HOLD, first.interpolation());
    EXPECT_EQ("replace", first.attributes().at("operation"));
    EXPECT_EQ("music.rms", first.attributes().at("source"));
    EXPECT_DOUBLE_EQ(0.25, *zoom.evaluate_keyframes(result.document->frame_grid()->frame_start(1)));
    EXPECT_TRUE(result.diagnostics.empty());
}
