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

constexpr StringId LANE_LABEL{10};
constexpr StringId LANE_KIND{11};
constexpr StringId BEAT_KIND{12};
constexpr StringId BEAT_LABEL{13};
constexpr StringId PHRASE_KIND{14};
constexpr StringId PHRASE_LABEL{15};
constexpr StringId PULSE_KIND{16};
constexpr StringId PULSE_LABEL{17};

} // namespace

TEST(Lane, preservesMixedItemsInInsertionOrder)
{
    Lane lane(StringId{1}, LANE_LABEL, LANE_KIND, at(0), at(100));
    lane.add(Instant(StringId{2}, BEAT_KIND, at(10), BEAT_LABEL, 0.75, {{"channel", "1"}}));
    lane.add(Interval(StringId{3}, PHRASE_KIND, at(20), at(50), PHRASE_LABEL, std::nullopt, {}));
    lane.add(Envelope(
        StringId{4}, PULSE_KIND, at(60), lasting(10), lasting(20), lasting(10), PULSE_LABEL, std::nullopt, {}));

    ASSERT_EQ(3, lane.item_count());
    ASSERT_EQ(3U, lane.items().size());
    EXPECT_TRUE(std::holds_alternative<Instant>(lane.items()[0]));
    EXPECT_TRUE(std::holds_alternative<Interval>(lane.items()[1]));
    EXPECT_TRUE(std::holds_alternative<Envelope>(lane.items()[2]));

    const auto &instant = std::get<Instant>(lane.items()[0]);
    EXPECT_EQ(StringId{2}, instant.id());
    EXPECT_EQ(BEAT_KIND, instant.kind());
    EXPECT_EQ(BEAT_LABEL, instant.label());
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

TEST(Lane, queriesItemsOverlappingATimeRange)
{
    Lane lane(StringId{1}, LANE_LABEL, LANE_KIND, at(0), at(100));
    lane.add(Instant(StringId{2}, BEAT_KIND, at(10)));
    lane.add(Interval(StringId{3}, PHRASE_KIND, at(20), at(50)));
    lane.add(Envelope(StringId{4}, PULSE_KIND, at(60), lasting(10), lasting(20), lasting(10), {}, std::nullopt, {}));

    const std::vector<Item> items = lane.items_in_range(at(15), at(65));

    ASSERT_EQ(2U, items.size());
    EXPECT_TRUE(std::holds_alternative<Interval>(items[0]));
    EXPECT_TRUE(std::holds_alternative<Envelope>(items[1]));
}

TEST(Lane, rejectsInvalidRangesAndItems)
{
    EXPECT_THROW(Lane(StringId{1}, LANE_LABEL, LANE_KIND, at(10), at(10)), std::invalid_argument);
    EXPECT_THROW(Interval(StringId{2}, PHRASE_KIND, at(20), at(20)), std::invalid_argument);
    EXPECT_THROW(Interval(StringId{2}, PHRASE_KIND, at(30), at(20)), std::invalid_argument);
    EXPECT_THROW(Envelope(StringId{2}, PULSE_KIND, at(10)), std::invalid_argument);
    EXPECT_THROW(
        Envelope(StringId{2}, PULSE_KIND, at(10), lasting(-1), std::nullopt, std::nullopt, {}, std::nullopt, {}),
        std::invalid_argument);

    Lane lane(StringId{1}, LANE_LABEL, LANE_KIND, at(0), at(100));
    EXPECT_THROW(lane.add(Instant(StringId{2}, BEAT_KIND, at(-1))), std::out_of_range);
    EXPECT_THROW(lane.add(Instant(StringId{3}, BEAT_KIND, at(100))), std::out_of_range);
    EXPECT_THROW(lane.add(Interval(StringId{4}, PHRASE_KIND, at(90), at(101))), std::out_of_range);
}
