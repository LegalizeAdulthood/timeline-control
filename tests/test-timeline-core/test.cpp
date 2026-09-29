#include <timeline/Time.h>

#include <gtest/gtest.h>

#include <stdexcept>

using namespace timeline;

TEST(Timebase, rejects_invalid_tick_rate)
{
    EXPECT_THROW(Timebase(0), std::invalid_argument);
    EXPECT_THROW(Timebase(-1), std::invalid_argument);
}

TEST(Timebase, converts_exact_ratios_to_ticks)
{
    auto const timebase = Timebase(24000);

    EXPECT_EQ(1000, timebase.duration_from_seconds_ratio(1, 24, TimeRounding::NEAREST).ticks());
    EXPECT_EQ(60000, timebase.time_from_seconds_ratio(5, 2, TimeRounding::NEAREST).ticks());
    EXPECT_DOUBLE_EQ(2.5, timebase.seconds(TimelineTime::from_ticks(60000)));
}

TEST(Timebase, rounds_fractional_seconds_explicitly)
{
    auto const timebase = Timebase(1000);

    EXPECT_EQ(1234, timebase.time_from_seconds(1.2345, TimeRounding::FLOOR).ticks());
    EXPECT_EQ(1235, timebase.time_from_seconds(1.2345, TimeRounding::NEAREST).ticks());
    EXPECT_EQ(1235, timebase.time_from_seconds(1.2345, TimeRounding::CEIL).ticks());
}

TEST(FrameGrid, converts_common_frame_rates_exactly)
{
    auto const timebase = Timebase(24000);
    auto const grid = FrameGrid(timebase, 10, 24, 1);

    EXPECT_EQ(10, grid.frame_count());
    EXPECT_EQ(1000, grid.frame_duration().ticks());
    EXPECT_EQ(0, grid.frame_start(0).ticks());
    EXPECT_EQ(5000, grid.frame_start(5).ticks());
    EXPECT_EQ(10000, grid.duration().ticks());
    EXPECT_EQ(10000, grid.end_time().ticks());
}

TEST(FrameGrid, handles_final_frame_boundary)
{
    auto const timebase = Timebase(30);
    auto const grid = FrameGrid(timebase, 100, 30, 1);

    EXPECT_EQ(99, grid.frame_start(99).ticks());
    EXPECT_THROW(grid.frame_start(100), std::out_of_range);
    EXPECT_EQ(99, *grid.frame_at_or_before(grid.end_time()));
    EXPECT_EQ(99, *grid.nearest_frame(TimelineTime::from_ticks(1000)));
}

TEST(FrameGrid, applies_offset)
{
    auto const timebase = Timebase(1000);
    auto const offset = timebase.time_from_seconds_ratio(1, 2, TimeRounding::NEAREST);
    auto const grid = FrameGrid(timebase, 5, 25, 1, offset);

    EXPECT_EQ(500, grid.offset().ticks());
    EXPECT_EQ(40, grid.frame_duration().ticks());
    EXPECT_EQ(580, grid.frame_start(2).ticks());
    EXPECT_EQ(std::nullopt, grid.frame_at_or_before(TimelineTime::from_ticks(499)));
    EXPECT_EQ(0, *grid.frame_at_or_before(TimelineTime::from_ticks(500)));
    EXPECT_EQ(1, *grid.frame_at_or_before(TimelineTime::from_ticks(579)));
    EXPECT_EQ(2, *grid.frame_at_or_before(TimelineTime::from_ticks(580)));
}

TEST(FrameGrid, rejects_non_integral_frame_durations)
{
    auto const timebase = Timebase(1000);

    EXPECT_THROW((FrameGrid(timebase, 10, 24, 1)), std::invalid_argument);
}
