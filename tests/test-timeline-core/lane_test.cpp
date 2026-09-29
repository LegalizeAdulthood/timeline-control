// Copyright (c) 2026 Richard Thomson

#include <timeline/Lane.h>

#include <gtest/gtest.h>

#include <optional>
#include <stdexcept>
#include <variant>

using namespace timeline;

namespace
{

Time at(Ticks ticks)
{
    return Time::from_ticks(ticks);
}

Duration lasting(Ticks ticks)
{
    return Duration::from_ticks(ticks);
}

} // namespace

TEST(Lane, preserves_mixed_items_in_insertion_order)
{
    auto lane = Lane("music", "Music events", "events", at(0), at(100));
    lane.add(Instant("beat-1", "beat", at(10), "Beat", 0.75, {{"channel", "1"}}));
    lane.add(Interval("phrase-1", "phrase", at(20), at(50), "Phrase", std::nullopt, {}));
    lane.add(Envelope("pulse-1", "pulse", at(60), lasting(10), lasting(20), lasting(10), "Pulse", std::nullopt, {}));

    ASSERT_EQ(3, lane.item_count());
    ASSERT_EQ(3U, lane.items().size());
    EXPECT_TRUE(std::holds_alternative<Instant>(lane.items()[0]));
    EXPECT_TRUE(std::holds_alternative<Interval>(lane.items()[1]));
    EXPECT_TRUE(std::holds_alternative<Envelope>(lane.items()[2]));

    const auto &instant = std::get<Instant>(lane.items()[0]);
    EXPECT_EQ("beat-1", instant.id());
    EXPECT_EQ("beat", instant.kind());
    EXPECT_EQ("Beat", instant.label());
    EXPECT_DOUBLE_EQ(0.75, *instant.strength());
    EXPECT_EQ("1", instant.attributes().at("channel"));

    const auto &interval = std::get<Interval>(lane.items()[1]);
    EXPECT_EQ(30, interval.duration().ticks());

    const auto &envelope = std::get<Envelope>(lane.items()[2]);
    EXPECT_EQ(10, envelope.attack()->ticks());
    EXPECT_EQ(20, envelope.sustain()->ticks());
    EXPECT_EQ(10, envelope.decay()->ticks());
    EXPECT_EQ(100, envelope.end().ticks());
}

TEST(Lane, queries_items_overlapping_a_time_range)
{
    auto lane = Lane("music", "Music events", "events", at(0), at(100));
    lane.add(Instant("beat-1", "beat", at(10)));
    lane.add(Interval("phrase-1", "phrase", at(20), at(50)));
    lane.add(Envelope("pulse-1", "pulse", at(60), lasting(10), lasting(20), lasting(10), {}, std::nullopt, {}));

    const auto items = lane.items_in_range(at(15), at(65));

    ASSERT_EQ(2U, items.size());
    EXPECT_TRUE(std::holds_alternative<Interval>(items[0]));
    EXPECT_TRUE(std::holds_alternative<Envelope>(items[1]));
}

TEST(Lane, rejects_invalid_ranges_and_items)
{
    EXPECT_THROW(Lane("music", "Music events", "events", at(10), at(10)), std::invalid_argument);
    EXPECT_THROW(Interval("bad", "phrase", at(20), at(20)), std::invalid_argument);
    EXPECT_THROW(Interval("bad", "phrase", at(30), at(20)), std::invalid_argument);
    EXPECT_THROW(Envelope("bad", "pulse", at(10)), std::invalid_argument);
    EXPECT_THROW(Envelope("bad", "pulse", at(10), lasting(-1), std::nullopt, std::nullopt, {}, std::nullopt, {}),
        std::invalid_argument);

    auto lane = Lane("music", "Music events", "events", at(0), at(100));
    EXPECT_THROW(lane.add(Instant("early", "beat", at(-1))), std::out_of_range);
    EXPECT_THROW(lane.add(Instant("late", "beat", at(100))), std::out_of_range);
    EXPECT_THROW(lane.add(Interval("long", "phrase", at(90), at(101))), std::out_of_range);
}
