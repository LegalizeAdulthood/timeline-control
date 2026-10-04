// Copyright (c) 2026 Richard Thomson

#include <timeline/Query.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <string>

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

Document mixed_document()
{
    Document document(FrameGrid(Timebase(10), 4, 1, 1), 1, 2);

    Lane events("events", "Events", "events", at(0), at(40));
    events.add(Instant("beat-10", "beat", at(10)));
    events.add(Interval("phrase-8", "phrase", at(8), at(18)));
    document.add_lane(std::move(events));

    Lane envelopes("envelopes", "Envelopes", "envelopes", at(0), at(40));
    envelopes.add(Envelope("pulse-10", "pulse", at(10), lasting(2), lasting(6), lasting(2), {}, std::nullopt, {}));
    document.add_lane(std::move(envelopes));

    Lane curves("curves", "Curves", "curves", at(0), at(40));
    curves.add(Curve("rms", "rms", {{at(0), 0.0}, {at(20), 1.0}, {at(30), 0.0}}));
    document.add_lane(std::move(curves));

    Lane keyframes("keyframes", "Keyframes", "keyframes", at(0), at(40));
    keyframes.add(Keyframe("zoom-0", at(0), 0.0, KeyframeInterpolation::LINEAR, {}));
    keyframes.add(Keyframe("zoom-20", at(20), 2.0));
    document.add_lane(std::move(keyframes));

    document.add_lane(Lane("empty", "Empty", "events", at(0), at(40)));
    return document;
}

const LaneInspection &lane_named(const std::vector<LaneInspection> &lanes, const std::string &id)
{
    const std::vector<LaneInspection>::const_iterator result =
        std::find_if(lanes.begin(), lanes.end(), [&id](const LaneInspection &lane) { return lane.id == id; });
    if (result == lanes.end())
    {
        throw std::runtime_error("missing lane inspection");
    }
    return *result;
}

} // namespace

TEST(Query, inspectsMixedDocumentAtFrame)
{
    const std::optional<FrameInspection> inspection = inspect_frame(mixed_document(), 1);

    ASSERT_TRUE(inspection.has_value());
    EXPECT_EQ(1, inspection->frame);
    EXPECT_EQ(10, inspection->time.ticks());
    ASSERT_EQ(5U, inspection->lanes.size());

    const LaneInspection &events = lane_named(inspection->lanes, "events");
    EXPECT_EQ("Events", events.label);
    EXPECT_EQ(2, events.item_count);
    ASSERT_EQ(2U, events.items.size());
    EXPECT_EQ("beat-10", events.items[0].id);
    EXPECT_EQ(InspectionItemType::INSTANT, events.items[0].type);
    EXPECT_EQ(InspectionItemRole::ACTIVE, events.items[0].role);
    EXPECT_EQ("phrase-8", events.items[1].id);

    const LaneInspection &curves = lane_named(inspection->lanes, "curves");
    ASSERT_EQ(1U, curves.items.size());
    EXPECT_EQ("rms", curves.items[0].id);
    EXPECT_EQ(InspectionItemType::CURVE, curves.items[0].type);
    EXPECT_EQ(InspectionItemRole::SAMPLED, curves.items[0].role);
    ASSERT_TRUE(curves.items[0].value.has_value());
    EXPECT_DOUBLE_EQ(0.5, *curves.items[0].value);

    const LaneInspection &keyframes = lane_named(inspection->lanes, "keyframes");
    ASSERT_EQ(2U, keyframes.items.size());
    EXPECT_EQ("zoom-0", keyframes.items[0].id);
    EXPECT_EQ(InspectionItemRole::BEFORE, keyframes.items[0].role);
    EXPECT_EQ("zoom-20", keyframes.items[1].id);
    EXPECT_EQ(InspectionItemRole::AFTER, keyframes.items[1].role);
}

TEST(Query, reportsEmptyLanesAndExactKeyframes)
{
    const std::optional<FrameInspection> inspection = inspect_frame(mixed_document(), 2);

    ASSERT_TRUE(inspection.has_value());
    EXPECT_TRUE(lane_named(inspection->lanes, "empty").items.empty());
    const LaneInspection &keyframes = lane_named(inspection->lanes, "keyframes");
    ASSERT_EQ(1U, keyframes.items.size());
    EXPECT_EQ("zoom-20", keyframes.items[0].id);
    EXPECT_EQ(InspectionItemRole::EXACT, keyframes.items[0].role);
}

TEST(Query, inspectsRangesAcrossLanes)
{
    const RangeInspection inspection = inspect_range(mixed_document(), at(9), at(11));

    EXPECT_EQ(9, inspection.start.ticks());
    EXPECT_EQ(11, inspection.end.ticks());
    EXPECT_EQ(2U, lane_named(inspection.lanes, "events").items.size());
    EXPECT_EQ(1U, lane_named(inspection.lanes, "envelopes").items.size());
    EXPECT_EQ(1U, lane_named(inspection.lanes, "curves").items.size());
    EXPECT_TRUE(lane_named(inspection.lanes, "keyframes").items.empty());
    EXPECT_THROW(inspect_range(mixed_document(), at(11), at(9)), std::invalid_argument);
}

TEST(Query, rejectsFramesWithoutAMatchingGridPosition)
{
    EXPECT_FALSE(inspect_frame(Document(10), 0).has_value());
    EXPECT_FALSE(inspect_frame(mixed_document(), -1).has_value());
    EXPECT_FALSE(inspect_frame(mixed_document(), 4).has_value());
}
