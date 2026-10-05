// Copyright (c) 2026 Richard Thomson

#include <timeline/Query.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>

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
    DocumentBuilder builder(Document(FrameGrid(Timebase(10), 4, 1, 1), 1, 2));

    Lane events(builder.intern("events"), builder.intern("Events"), builder.intern("events"), at(0), at(40));
    events.add(Instant(builder.intern("beat-10"), builder.intern("beat"), at(10)));
    events.add(Interval(builder.intern("phrase-8"), builder.intern("phrase"), at(8), at(18)));
    builder.add_lane(std::move(events));

    Lane envelopes(
        builder.intern("envelopes"), builder.intern("Envelopes"), builder.intern("envelopes"), at(0), at(40));
    envelopes.add(Envelope(builder.intern("pulse-10"), builder.intern("pulse"), at(10), lasting(2), lasting(6),
        lasting(2), {}, std::nullopt, {}));
    builder.add_lane(std::move(envelopes));

    Lane curves(builder.intern("curves"), builder.intern("Curves"), builder.intern("curves"), at(0), at(40));
    curves.add(Curve(builder.intern("rms"), builder.intern("rms"), {{at(0), 0.0}, {at(20), 1.0}, {at(30), 0.0}}));
    builder.add_lane(std::move(curves));

    Lane keyframes(
        builder.intern("keyframes"), builder.intern("Keyframes"), builder.intern("keyframes"), at(0), at(40));
    keyframes.add(Keyframe(builder.intern("zoom-0"), at(0), 0.0, KeyframeInterpolation::LINEAR, {}));
    keyframes.add(Keyframe(builder.intern("zoom-20"), at(20), 2.0));
    builder.add_lane(std::move(keyframes));

    builder.add_lane(Lane(builder.intern("empty"), builder.intern("Empty"), builder.intern("events"), at(0), at(40)));
    return std::move(builder).build();
}

const LaneInspection &lane_named(const std::vector<LaneInspection> &lanes, StringId id)
{
    const std::vector<LaneInspection>::const_iterator result =
        std::find_if(lanes.begin(), lanes.end(), [&id](const LaneInspection &lane) { return lane.id == id; });
    if (result == lanes.end())
    {
        throw std::runtime_error("missing lane inspection");
    }
    return *result;
}

StringId id_named(const Document &document, std::string_view name)
{
    return *document.strings().find(name);
}

} // namespace

TEST(InspectionItemType, convertsEveryValueToString)
{
    EXPECT_EQ("instant", to_string(InspectionItemType::INSTANT));
    EXPECT_EQ("interval", to_string(InspectionItemType::INTERVAL));
    EXPECT_EQ("envelope", to_string(InspectionItemType::ENVELOPE));
    EXPECT_EQ("curve", to_string(InspectionItemType::CURVE));
    EXPECT_EQ("keyframe", to_string(InspectionItemType::KEYFRAME));
    EXPECT_EQ("palette", to_string(InspectionItemType::PALETTE));
}

TEST(InspectionItemRole, convertsEveryValueToString)
{
    EXPECT_EQ("active", to_string(InspectionItemRole::ACTIVE));
    EXPECT_EQ("sampled", to_string(InspectionItemRole::SAMPLED));
    EXPECT_EQ("before", to_string(InspectionItemRole::BEFORE));
    EXPECT_EQ("after", to_string(InspectionItemRole::AFTER));
    EXPECT_EQ("exact", to_string(InspectionItemRole::EXACT));
}

TEST(Query, inspectsMixedDocumentAtFrame)
{
    const Document document = mixed_document();
    const std::optional<FrameInspection> inspection = inspect_frame(document, 1);

    ASSERT_TRUE(inspection.has_value());
    EXPECT_EQ(1, inspection->frame);
    EXPECT_EQ(10, inspection->time.ticks());
    ASSERT_EQ(5U, inspection->lanes.size());

    const LaneInspection &events = lane_named(inspection->lanes, id_named(document, "events"));
    EXPECT_EQ("Events", document.strings().lookup(events.label));
    EXPECT_EQ(2, events.item_count);
    ASSERT_EQ(2U, events.items.size());
    EXPECT_EQ(id_named(document, "beat-10"), events.items[0].id);
    EXPECT_EQ(InspectionItemType::INSTANT, events.items[0].type);
    EXPECT_EQ(InspectionItemRole::ACTIVE, events.items[0].role);
    EXPECT_EQ(id_named(document, "phrase-8"), events.items[1].id);

    const LaneInspection &curves = lane_named(inspection->lanes, id_named(document, "curves"));
    ASSERT_EQ(1U, curves.items.size());
    EXPECT_EQ(id_named(document, "rms"), curves.items[0].id);
    EXPECT_EQ(InspectionItemType::CURVE, curves.items[0].type);
    EXPECT_EQ(InspectionItemRole::SAMPLED, curves.items[0].role);
    ASSERT_TRUE(curves.items[0].value.has_value());
    EXPECT_DOUBLE_EQ(0.5, *curves.items[0].value);

    const LaneInspection &keyframes = lane_named(inspection->lanes, id_named(document, "keyframes"));
    ASSERT_EQ(2U, keyframes.items.size());
    EXPECT_EQ(id_named(document, "zoom-0"), keyframes.items[0].id);
    EXPECT_EQ(InspectionItemRole::BEFORE, keyframes.items[0].role);
    EXPECT_EQ(id_named(document, "zoom-20"), keyframes.items[1].id);
    EXPECT_EQ(InspectionItemRole::AFTER, keyframes.items[1].role);
}

TEST(Query, reportsEmptyLanesAndExactKeyframes)
{
    const Document document = mixed_document();
    const std::optional<FrameInspection> inspection = inspect_frame(document, 2);

    ASSERT_TRUE(inspection.has_value());
    EXPECT_TRUE(lane_named(inspection->lanes, id_named(document, "empty")).items.empty());
    const LaneInspection &keyframes = lane_named(inspection->lanes, id_named(document, "keyframes"));
    ASSERT_EQ(1U, keyframes.items.size());
    EXPECT_EQ(id_named(document, "zoom-20"), keyframes.items[0].id);
    EXPECT_EQ(InspectionItemRole::EXACT, keyframes.items[0].role);
}

TEST(Query, inspectsRangesAcrossLanes)
{
    const Document document = mixed_document();
    const RangeInspection inspection = inspect_range(document, at(9), at(11));

    EXPECT_EQ(9, inspection.start.ticks());
    EXPECT_EQ(11, inspection.end.ticks());
    EXPECT_EQ(2U, lane_named(inspection.lanes, id_named(document, "events")).items.size());
    EXPECT_EQ(1U, lane_named(inspection.lanes, id_named(document, "envelopes")).items.size());
    EXPECT_EQ(1U, lane_named(inspection.lanes, id_named(document, "curves")).items.size());
    EXPECT_TRUE(lane_named(inspection.lanes, id_named(document, "keyframes")).items.empty());
    EXPECT_THROW(inspect_range(mixed_document(), at(11), at(9)), std::invalid_argument);
}

TEST(Query, rejectsFramesWithoutAMatchingGridPosition)
{
    EXPECT_FALSE(inspect_frame(Document(10), 0).has_value());
    EXPECT_FALSE(inspect_frame(mixed_document(), -1).has_value());
    EXPECT_FALSE(inspect_frame(mixed_document(), 4).has_value());
}
