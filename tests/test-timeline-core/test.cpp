#include <timeline/Document.h>
#include <timeline/Time.h>

#include <gtest/gtest.h>

#include <stdexcept>

using namespace timeline;

TEST(Timebase, rejectsInvalidTickRate)
{
    EXPECT_THROW(Timebase(0), std::invalid_argument);
    EXPECT_THROW(Timebase(-1), std::invalid_argument);
}

TEST(Timebase, convertsExactRatiosToTicks)
{
    const Timebase timebase(24000);

    EXPECT_EQ(1000, timebase.duration_from_seconds_ratio(1, 24, TimeRounding::NEAREST).ticks());
    EXPECT_EQ(60000, timebase.time_from_seconds_ratio(5, 2, TimeRounding::NEAREST).ticks());
    EXPECT_DOUBLE_EQ(2.5, timebase.seconds(Time::from_ticks(60000)));
}

TEST(Timebase, roundsFractionalSecondsExplicitly)
{
    const Timebase timebase(1000);

    EXPECT_EQ(1234, timebase.time_from_seconds(1.2345, TimeRounding::FLOOR).ticks());
    EXPECT_EQ(1235, timebase.time_from_seconds(1.2345, TimeRounding::NEAREST).ticks());
    EXPECT_EQ(1235, timebase.time_from_seconds(1.2345, TimeRounding::CEIL).ticks());
}

TEST(FrameGrid, convertsCommonFrameRatesExactly)
{
    const Timebase timebase(24000);
    const FrameGrid grid(timebase, 10, 24, 1);

    EXPECT_EQ(10, grid.frame_count());
    EXPECT_EQ(1000, grid.frame_duration().ticks());
    EXPECT_EQ(0, grid.frame_start(0).ticks());
    EXPECT_EQ(5000, grid.frame_start(5).ticks());
    EXPECT_EQ(10000, grid.duration().ticks());
    EXPECT_EQ(10000, grid.end_time().ticks());
}

TEST(FrameGrid, handlesFinalFrameBoundary)
{
    const Timebase timebase(30);
    const FrameGrid grid(timebase, 100, 30, 1);

    EXPECT_EQ(99, grid.frame_start(99).ticks());
    EXPECT_THROW(grid.frame_start(100), std::out_of_range);
    EXPECT_EQ(99, *grid.frame_at_or_before(grid.end_time()));
    EXPECT_EQ(99, *grid.nearest_frame(Time::from_ticks(1000)));
}

TEST(FrameGrid, appliesOffset)
{
    const Timebase timebase(1000);
    const Time offset = timebase.time_from_seconds_ratio(1, 2, TimeRounding::NEAREST);
    const FrameGrid grid(timebase, 5, 25, 1, offset);

    EXPECT_EQ(500, grid.offset().ticks());
    EXPECT_EQ(40, grid.frame_duration().ticks());
    EXPECT_EQ(580, grid.frame_start(2).ticks());
    EXPECT_EQ(std::nullopt, grid.frame_at_or_before(Time::from_ticks(499)));
    EXPECT_EQ(0, *grid.frame_at_or_before(Time::from_ticks(500)));
    EXPECT_EQ(1, *grid.frame_at_or_before(Time::from_ticks(579)));
    EXPECT_EQ(2, *grid.frame_at_or_before(Time::from_ticks(580)));
}

TEST(FrameGrid, rejectsNonIntegralFrameDurations)
{
    const Timebase timebase(1000);

    EXPECT_THROW((FrameGrid(timebase, 10, 24, 1)), std::invalid_argument);
}

TEST(Document, constructsEmptyDocument)
{
    const Document document(1000);

    EXPECT_TRUE(document.is_valid());
    EXPECT_EQ(1000, document.timebase().ticks_per_second());
}

TEST(Document, rejectsInvalidTimebase)
{
    EXPECT_THROW(Document(0), std::invalid_argument);
    EXPECT_THROW(Document(-1), std::invalid_argument);
}

TEST(Document, preservesMetadata)
{
    const Metadata metadata("Demo", "Empty timeline");
    const Document document(1000, metadata);

    EXPECT_EQ("Demo", document.metadata().title());
    EXPECT_EQ("Empty timeline", document.metadata().description());
}

TEST(Document, reportsZeroLanes)
{
    const Document document(1000);

    EXPECT_TRUE(document.lanes_empty());
    EXPECT_EQ(0, document.lane_count());
}

TEST(Document, preservesFrameAndAuthoredContentSummary)
{
    const FrameGrid frame_grid(Timebase(24000), 3, 24, 1);
    const Document document(frame_grid, 2, 4);

    EXPECT_EQ(24000, document.timebase().ticks_per_second());
    ASSERT_TRUE(document.frame_grid().has_value());
    EXPECT_EQ(3, document.frame_grid()->frame_count());
    EXPECT_EQ(2, document.track_count());
    EXPECT_EQ(4, document.keyframe_count());
    EXPECT_TRUE(document.lanes_empty());
}

TEST(Document, preservesSourceSummary)
{
    const SourceSummary summary("par-beatdown.tracker-timeline", 1, 3, 12, std::optional<Ticks>{2},
        std::optional<Ticks>{8}, std::optional<Time>{Time::from_ticks(200)}, std::optional<Time>{Time::from_ticks(800)},
        std::optional<Duration>{Duration::from_ticks(25)}, std::nullopt);
    const Document document(Timebase(1000), summary);

    ASSERT_TRUE(document.source_summary().has_value());
    EXPECT_EQ("par-beatdown.tracker-timeline", document.source_summary()->schema());
    EXPECT_EQ(1, document.source_summary()->schema_version());
    EXPECT_EQ(3, document.source_summary()->feature_count());
    EXPECT_EQ(12, document.source_summary()->event_count());
    EXPECT_EQ(2, *document.source_summary()->first_frame());
    EXPECT_EQ(8, *document.source_summary()->last_frame());
    EXPECT_EQ(200, document.source_summary()->first_time()->ticks());
    EXPECT_EQ(800, document.source_summary()->last_time()->ticks());
    EXPECT_EQ(25, document.source_summary()->frame_offset()->ticks());
}

TEST(Document, preservesGenerationSummary)
{
    const GenerationSummary generation("beat-keys", "0.1.0",
        {SourceReference("base_animation", "base.json"), SourceReference("timeline", "music.json")},
        {NamedCount("camera.zoom", 3)}, {NamedCount("music.rms", 3)});
    const SourceSummary source("par-beatdown.beat-keys-overlay", 1, 0, 0, std::optional<Ticks>{0},
        std::optional<Ticks>{4}, std::nullopt, std::nullopt, std::nullopt, generation);
    const Document document(Timebase(120000), source, 1, 3);

    EXPECT_EQ(1, document.track_count());
    EXPECT_EQ(3, document.keyframe_count());
    ASSERT_TRUE(document.source_summary()->generation_summary().has_value());
    const GenerationSummary &summary = *document.source_summary()->generation_summary();
    EXPECT_EQ("beat-keys", summary.generator_name());
    EXPECT_EQ("0.1.0", summary.generator_version());
    ASSERT_EQ(2U, summary.source_references().size());
    EXPECT_EQ("base_animation", summary.source_references()[0].role());
    EXPECT_EQ("base.json", summary.source_references()[0].location());
    ASSERT_EQ(1U, summary.target_counts().size());
    EXPECT_EQ("camera.zoom", summary.target_counts()[0].name());
    EXPECT_EQ(3, summary.target_counts()[0].count());
    ASSERT_EQ(1U, summary.source_counts().size());
    EXPECT_EQ("music.rms", summary.source_counts()[0].name());
    EXPECT_EQ(3, summary.source_counts()[0].count());
}

TEST(Document, rejectsNegativeCounts)
{
    const SourceSummary source("schema", 1, 0, 0);
    const FrameGrid frame_grid(Timebase(1000), 1, 1, 1);

    EXPECT_THROW(NamedCount("items", -1), std::invalid_argument);
    EXPECT_THROW(SourceSummary("schema", -1, 0, 0), std::invalid_argument);
    EXPECT_THROW(SourceSummary("schema", 1, -1, 0), std::invalid_argument);
    EXPECT_THROW(SourceSummary("schema", 1, 0, -1), std::invalid_argument);
    EXPECT_THROW(Document(Timebase(1000), source, -1, 0), std::invalid_argument);
    EXPECT_THROW(Document(Timebase(1000), source, 0, -1), std::invalid_argument);
    EXPECT_THROW(Document(frame_grid, -1, 0), std::invalid_argument);
    EXPECT_THROW(Document(frame_grid, 0, -1), std::invalid_argument);
}
