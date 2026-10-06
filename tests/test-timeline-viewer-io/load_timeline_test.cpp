// Copyright (c) 2026 Richard Thomson

#include <timelineViewer/load_timeline.h>

#include <timeline/size_cast.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace
{

const std::filesystem::path FIXTURES(TIMELINE_TEST_FIXTURE_DIR);

timeline_viewer::LoadResult load_animation()
{
    const std::optional<timeline::Document> document;
    const std::vector<timeline_par_animator::BeatKeysMapping> mappings;
    return timeline_viewer::load_timeline(FIXTURES / "extreme-normalized-vectors.json", false, document, mappings);
}

TEST(ViewerIoLoad, replacesDocument)
{
    const std::optional<timeline::Document> document;
    const std::vector<timeline_par_animator::BeatKeysMapping> mappings;

    const timeline_viewer::LoadResult result =
        timeline_viewer::load_timeline(FIXTURES / "extreme-normalized-vectors.json", false, document, mappings);

    ASSERT_TRUE(result.succeeded());
    EXPECT_EQ(timeline_viewer::LoadOutcome::LOADED, result.outcome);
    EXPECT_EQ(25, result.document->lane_count());
}

TEST(ViewerIoLoad, appendsDocument)
{
    const timeline_viewer::LoadResult initial = load_animation();
    ASSERT_TRUE(initial.succeeded());

    const timeline_viewer::LoadResult result = timeline_viewer::load_timeline(
        FIXTURES / "beat-keys/rms.beat-keys.json", true, initial.document, initial.mappings);

    ASSERT_TRUE(result.succeeded());
    EXPECT_EQ(29, result.document->lane_count());
}

TEST(ViewerIoLoad, reportsImportFailure)
{
    const std::optional<timeline::Document> document;
    const std::vector<timeline_par_animator::BeatKeysMapping> mappings;

    const timeline_viewer::LoadResult result =
        timeline_viewer::load_timeline(FIXTURES / "invalid-schema.json", false, document, mappings);

    EXPECT_FALSE(result.succeeded());
    EXPECT_EQ(timeline_viewer::LoadOutcome::IMPORT_FAILED, result.outcome);
    EXPECT_FALSE(result.document);
    EXPECT_FALSE(result.diagnostics.empty());
}

TEST(ViewerIoLoad, reportsCompositionFailure)
{
    const std::optional<timeline::Document> document(std::in_place, timeline::Timebase(120000));
    const std::vector<timeline_par_animator::BeatKeysMapping> mappings;

    const timeline_viewer::LoadResult result =
        timeline_viewer::load_timeline(FIXTURES / "extreme-normalized-vectors.json", true, document, mappings);

    EXPECT_FALSE(result.succeeded());
    EXPECT_EQ(timeline_viewer::LoadOutcome::COMPOSITION_FAILED, result.outcome);
    EXPECT_FALSE(result.document);
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ("combined timelines require matching timebases and frame rates", result.diagnostics.back());
}

TEST(ViewerIoLoad, discoversBeatKeysCompanion)
{
    const std::optional<timeline::Document> document;
    const std::vector<timeline_par_animator::BeatKeysMapping> mappings;

    const timeline_viewer::LoadResult result =
        timeline_viewer::load_timeline(FIXTURES / "beat-keys/timeline-features.json", false, document, mappings);

    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.document->frame_grid());
    EXPECT_EQ(30, result.document->frame_grid()->frames_per_second_numerator());
}

TEST(ViewerIoLoad, resetsMappingsForReplacement)
{
    const std::optional<timeline::Document> document;
    const std::vector<timeline_par_animator::BeatKeysMapping> mappings;
    const timeline_viewer::LoadResult initial =
        timeline_viewer::load_timeline(FIXTURES / "beat-keys/rms.beat-keys.json", false, document, mappings);
    ASSERT_TRUE(initial.succeeded());
    ASSERT_EQ(1, timeline::size_cast(initial.mappings));

    const timeline_viewer::LoadResult result = timeline_viewer::load_timeline(
        FIXTURES / "extreme-normalized-vectors.json", false, initial.document, initial.mappings);

    ASSERT_TRUE(result.succeeded());
    EXPECT_TRUE(result.mappings.empty());
}

TEST(ViewerIoLoad, accumulatesMappingsForAppend)
{
    const timeline_viewer::LoadResult animation = load_animation();
    ASSERT_TRUE(animation.succeeded());
    const timeline_viewer::LoadResult first = timeline_viewer::load_timeline(
        FIXTURES / "beat-keys/rms.beat-keys.json", true, animation.document, animation.mappings);
    ASSERT_TRUE(first.succeeded());

    const timeline_viewer::LoadResult second = timeline_viewer::load_timeline(
        FIXTURES / "beat-keys/peak.beat-keys.json", true, first.document, first.mappings);

    ASSERT_TRUE(second.succeeded());
    EXPECT_EQ(2, timeline::size_cast(second.mappings));
}

TEST(ViewerIoLoad, inheritsTimingForAppend)
{
    const timeline::FrameGrid grid(timeline::Timebase(24000), 120, 24000, 1001);
    const std::optional<timeline::Document> document(std::in_place, grid, 0, 0);
    const std::vector<timeline_par_animator::BeatKeysMapping> mappings;

    const timeline_viewer::LoadResult result =
        timeline_viewer::load_timeline(FIXTURES / "extreme-normalized-vectors.json", true, document, mappings);

    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.document->frame_grid());
    EXPECT_EQ(24000, result.document->timebase().ticks_per_second());
    EXPECT_EQ(24000, result.document->frame_grid()->frames_per_second_numerator());
    EXPECT_EQ(1001, result.document->frame_grid()->frames_per_second_denominator());
}

} // namespace
