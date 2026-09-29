// Copyright (c) 2026 Richard Thomson

#include <timelineParAnimator/TimelineJson.h>

#include <gtest/gtest.h>

#include <filesystem>

using namespace timeline_par_animator;

TEST(TimelineJson, constructs_empty_document_for_selected_path)
{
    const auto source_path = std::filesystem::path("fixtures/example.json");
    auto result = import_timeline_json(source_path);

    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.document.has_value());
    EXPECT_TRUE(result.diagnostics.empty());
    EXPECT_EQ("example.json", result.document->metadata().title());
    EXPECT_EQ(source_path.string(), result.document->metadata().description());
    EXPECT_EQ(120000, result.document->timebase().ticks_per_second());
    EXPECT_TRUE(result.document->lanes_empty());
}

TEST(TimelineJson, rejects_empty_source_path)
{
    const auto result = import_timeline_json({});

    EXPECT_FALSE(result.succeeded());
    EXPECT_FALSE(result.document.has_value());
    ASSERT_EQ(1U, result.diagnostics.size());
    EXPECT_EQ("No JSON file was selected.", result.diagnostics.front());
}
