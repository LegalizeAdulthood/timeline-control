// Copyright (c) 2026 Richard Thomson

#include <timeline/Attributes.h>
#include <timeline/Curve.h>
#include <timeline/Document.h>
#include <timeline/Envelope.h>
#include <timeline/Event.h>
#include <timeline/Interval.h>
#include <timeline/Keyframe.h>
#include <timeline/Palette.h>
#include <timeline/Query.h>
#include <timeline/size_cast.h>

#include <gtest/gtest.h>

#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace timeline;

TEST(Attributes, sortsByKeyAndFindsRepeatedValues)
{
    const StringId repeated{4};

    const Attributes attributes(
        std::vector<Attribute>{{StringId{3}, repeated}, {StringId{1}, repeated}, {StringId{2}, StringId{5}}});

    ASSERT_EQ(3, attributes.size());
    EXPECT_EQ(StringId{1}, attributes.values()[0].key());
    EXPECT_EQ(StringId{2}, attributes.values()[1].key());
    EXPECT_EQ(StringId{3}, attributes.values()[2].key());
    EXPECT_EQ(repeated, attributes.find(StringId{1}));
    EXPECT_FALSE(attributes.find(StringId{6}));
}

TEST(Attributes, rejectsEmptyAndDuplicateKeys)
{
    const std::vector<Attribute> empty_key{{StringId{}, StringId{1}}};
    const std::vector<Attribute> duplicate_key{{StringId{1}, StringId{2}}, {StringId{1}, StringId{3}}};

    const auto construct = [](const std::vector<Attribute> &values)
    {
        return Attributes(values);
    };

    EXPECT_THROW(construct(empty_key), std::invalid_argument);
    EXPECT_THROW(construct(duplicate_key), std::invalid_argument);
}

TEST(Attributes, supportsEmptySetsAndValues)
{
    StringTableBuilder strings;
    const StringId key = strings.intern("empty");
    const Attributes empty;

    const Attributes attributes(std::vector<Attribute>{{key, StringId{}}});
    const StringTable table = std::move(strings).build();

    EXPECT_TRUE(empty.empty());
    ASSERT_EQ(1, attributes.size());
    ASSERT_TRUE(attributes.find(*table.find("empty")));
    EXPECT_TRUE(table.lookup(*attributes.find(*table.find("empty"))).empty());
}

TEST(Attributes, surviveDocumentCopying)
{
    DocumentBuilder builder(Document(FrameGrid(Timebase(30), 2, 30, 1), 0, 0));
    const StringId key = builder.intern("channel");
    const StringId value = builder.intern("left");
    Lane lane(builder.intern("lane"), builder.intern("Lane"), builder.intern("events"), Time{}, Time::from_ticks(2));
    lane.add(Instant(builder.intern("item"), builder.intern("event"), Time{}, {}, std::nullopt,
        Attributes(std::vector<Attribute>{{key, value}})));
    builder.add_lane(std::move(lane));
    const Document document = std::move(builder).build();

    const Document copy = document;
    const Attributes &attributes = std::get<Instant>(copy.lanes().front().items().front()).attributes();

    ASSERT_TRUE(copy.strings().shares_storage_with(document.strings()));
    ASSERT_TRUE(attributes.find(*copy.strings().find("channel")));
    EXPECT_EQ("left", copy.strings().lookup(*attributes.find(*copy.strings().find("channel"))));
}

TEST(Attributes, remapAcrossDocumentStringTables)
{
    const Document first(FrameGrid(Timebase(30), 2, 30, 1), 0, 0);
    DocumentBuilder second_builder(Document(FrameGrid(Timebase(30), 2, 30, 1), 0, 0));
    second_builder.intern("unrelated");
    const StringId key = second_builder.intern("channel");
    const StringId value = second_builder.intern("right");
    Lane lane(second_builder.intern("lane"), second_builder.intern("Lane"), second_builder.intern("events"), Time{},
        Time::from_ticks(2));
    lane.add(Instant(second_builder.intern("item"), second_builder.intern("event"), Time{}, {}, std::nullopt,
        Attributes(std::vector<Attribute>{{key, value}})));
    second_builder.add_lane(std::move(lane));
    const Document second = std::move(second_builder).build();

    const Document combined = combine_documents(first, second);
    const Attributes &attributes = std::get<Instant>(combined.lanes().front().items().front()).attributes();

    ASSERT_TRUE(attributes.find(*combined.strings().find("channel")));
    EXPECT_EQ("right", combined.strings().lookup(*attributes.find(*combined.strings().find("channel"))));
}

TEST(Attributes, remainAttachedToEveryInspectedItemType)
{
    const FrameGrid grid(Timebase(30), 3, 30, 1);
    DocumentBuilder builder(Document(grid, 0, 0));
    const StringId key = builder.intern("source");
    const StringId value = builder.intern("fixture");
    const Attributes attributes(std::vector<Attribute>{{key, value}});
    const StringId kind = builder.intern("kind");
    Lane lane(builder.intern("lane"), builder.intern("Lane"), builder.intern("mixed"), grid.offset(), grid.end_time());
    lane.add(Instant(builder.intern("instant"), kind, grid.frame_start(1)).with_attributes(attributes));
    lane.add(Interval(builder.intern("interval"), kind, grid.offset(), grid.end_time()).with_attributes(attributes));
    lane.add(Envelope(builder.intern("envelope"), kind, grid.offset(), Duration::from_ticks(1), Duration::from_ticks(1),
        Duration::from_ticks(1), {}, std::nullopt, {})
            .with_attributes(attributes));
    lane.add(Curve(builder.intern("curve"), kind, {{grid.offset(), 0.0}, {grid.end_time(), 1.0}})
            .with_attributes(attributes));
    lane.add(Keyframe(builder.intern("keyframe"), grid.frame_start(1), 1.0).with_attributes(attributes));
    lane.add(PaletteCurve(
        builder.intern("palette"), kind, grid.offset(), grid.end_time(),
        [](Time) { return Palette{RgbColor(1, 2, 3)}; }, attributes));
    builder.add_lane(std::move(lane));
    const Document document = std::move(builder).build();

    const FrameInspection inspection = *inspect_frame(document, 1);

    ASSERT_EQ(6, size_cast(inspection.lanes.front().items));
    for (const InspectionItem &item : inspection.lanes.front().items)
    {
        EXPECT_EQ(value, item.attributes.find(key));
    }
}
