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

const TimelineJsonImportOptions TEST_OPTIONS{
    24000,
    24,
    1,
};

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
