// Copyright (c) 2026 Richard Thomson

#include <timelineParAnimator/TimelineJson.h>

#include <gtest/gtest.h>

#include <filesystem>

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
    EXPECT_EQ(1U, result.document->track_count());
    EXPECT_EQ(2U, result.document->keyframe_count());
    EXPECT_TRUE(result.document->lanes_empty());
}

TEST(TimelineJson, reports_multi_track_counts)
{
    const auto result = import_timeline_json(fixture_path("multi-track.json"), TEST_OPTIONS);

    ASSERT_TRUE(result.succeeded());
    EXPECT_EQ(3, result.document->frame_grid()->frame_count());
    EXPECT_EQ(2U, result.document->track_count());
    EXPECT_EQ(4U, result.document->keyframe_count());
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
    EXPECT_EQ(1U, summary.schema_version());
    EXPECT_EQ(0U, summary.feature_count());
    EXPECT_EQ(6U, summary.event_count());
    EXPECT_EQ(0, *summary.first_frame());
    EXPECT_EQ(4, *summary.last_frame());
    EXPECT_EQ(0, summary.first_time()->ticks());
    EXPECT_EQ(16000, summary.last_time()->ticks());
    EXPECT_EQ(0, summary.frame_offset()->ticks());
    ASSERT_TRUE(result.document->frame_grid().has_value());
    EXPECT_EQ(5, result.document->frame_grid()->frame_count());
    EXPECT_EQ(30, result.document->frame_grid()->frames_per_second_numerator());
    EXPECT_EQ(1, result.document->frame_grid()->frames_per_second_denominator());
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TimelineJson, imports_full_tracker_timeline_summary)
{
    const auto result = import_timeline_json(fixture_path("par-beatdown/gold-write-timeline-clock.json"));

    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.document->source_summary().has_value());
    const auto &summary = *result.document->source_summary();
    EXPECT_EQ(0U, summary.feature_count());
    EXPECT_EQ(1891U, summary.event_count());
    EXPECT_EQ(0, *summary.first_frame());
    EXPECT_EQ(4908, *summary.last_frame());
    EXPECT_EQ(0, summary.first_time()->ticks());
    EXPECT_EQ(19631430, summary.last_time()->ticks());
    ASSERT_TRUE(result.document->frame_grid().has_value());
    EXPECT_EQ(4909, result.document->frame_grid()->frame_count());
    EXPECT_EQ(30, result.document->frame_grid()->frames_per_second_numerator());
    EXPECT_EQ(1, result.document->frame_grid()->frames_per_second_denominator());
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
