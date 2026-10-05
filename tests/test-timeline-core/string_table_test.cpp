// Copyright (c) 2026 Richard Thomson

#include <timeline/Document.h>
#include <timeline/Layout.h>
#include <timeline/StringTable.h>

#include <gtest/gtest.h>

#include <stdexcept>
#include <utility>

using namespace timeline;

namespace
{

Document document_with_lane(std::string_view id)
{
    const FrameGrid grid(Timebase(30), 2, 30, 1);
    DocumentBuilder builder(Document(grid, 0, 0));
    Lane lane(builder.intern(id), builder.intern(id), builder.intern("events"), grid.offset(), grid.end_time());
    lane.add(Instant(builder.intern(std::string(id) + "-item"), builder.intern("event"), grid.offset()));
    builder.add_lane(std::move(lane));
    return std::move(builder).build();
}

StringId first_item_id(const Document &document, int lane)
{
    return std::get<Instant>(document.lanes()[lane].items().front()).id();
}

Document document_with_descriptions(std::string_view title, std::string_view description, std::string_view schema)
{
    StringTableBuilder strings;
    const Metadata metadata(strings.intern(title), strings.intern(description));
    const GenerationSummary generation(strings.intern("generator"), strings.intern("1.0"),
        {SourceReference(strings.intern("input"), strings.intern("source.json"))},
        {NamedCount(strings.intern("target"), 2)}, {NamedCount(strings.intern("source"), 3)});
    const SourceSummary summary(strings.intern(schema), 1, 2, 3, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
        std::nullopt, generation);
    DocumentBuilder builder(
        Document(FrameGrid(Timebase(30), 2, 30, 1), summary, 1, 2, metadata), std::move(strings).build());
    return std::move(builder).build();
}

} // namespace

TEST(StringId, comparesIntegerValues)
{
    EXPECT_TRUE(StringId{}.empty());
    EXPECT_EQ(StringId{7}, StringId{7});
    EXPECT_NE(StringId{7}, StringId{8});
    EXPECT_LT(StringId{7}, StringId{8});
}

TEST(StringTable, internsAndLooksUpDeduplicatedStrings)
{
    StringTableBuilder builder;
    const StringId first = builder.intern("lane");
    const StringId duplicate = builder.intern("lane");
    const StringId second = builder.intern("other");
    const StringTable table = std::move(builder).build();

    EXPECT_EQ(StringId{}, table.find(""));
    EXPECT_EQ(first, duplicate);
    EXPECT_NE(first, second);
    EXPECT_EQ("", table.lookup(StringId{}));
    EXPECT_EQ("lane", table.lookup(first));
    EXPECT_EQ(first, table.find("lane"));
    EXPECT_FALSE(table.find("missing"));
}

TEST(StringTable, documentCopiesRetainSharedStringLifetime)
{
    const Document copy = []
    {
        const Document original = document_with_lane("events");
        return original;
    }();
    const Document second_copy = copy;

    EXPECT_EQ("events", copy.strings().lookup(copy.lanes().front().id()));
    EXPECT_TRUE(second_copy.strings().shares_storage_with(copy.strings()));
    const Layout layout(copy, Viewport(200, 60, Time{}, Time::from_ticks(2)), LayoutMetrics(80, 20, 30, 2));
    const DisplayList display = layout.display_list();
    EXPECT_EQ("events", display.strings().lookup(copy.lanes().front().id()));
}

TEST(StringTable, documentCopiesRetainDescriptionIds)
{
    const Document original = document_with_descriptions("Original", "source.json", "schema");

    const Document copy = original;

    ASSERT_TRUE(copy.strings().shares_storage_with(original.strings()));
    EXPECT_EQ("Original", copy.strings().lookup(copy.metadata().title()));
    EXPECT_EQ("source.json", copy.strings().lookup(copy.metadata().description()));
    ASSERT_TRUE(copy.source_summary());
    EXPECT_EQ("schema", copy.strings().lookup(copy.source_summary()->schema()));
    ASSERT_TRUE(copy.source_summary()->generation_summary());
    const GenerationSummary &generation = *copy.source_summary()->generation_summary();
    EXPECT_EQ("generator", copy.strings().lookup(generation.generator_name()));
    EXPECT_EQ("1.0", copy.strings().lookup(generation.generator_version()));
    EXPECT_EQ("input", copy.strings().lookup(generation.source_references().front().role()));
    EXPECT_EQ("source.json", copy.strings().lookup(generation.source_references().front().location()));
    EXPECT_EQ("target", copy.strings().lookup(generation.target_counts().front().name()));
    EXPECT_EQ("source", copy.strings().lookup(generation.source_counts().front().name()));
}

TEST(StringTable, combinesOverlappingAndDistinctDocumentDescriptions)
{
    const Document first = document_with_descriptions("First", "shared", "shared");
    const Document second = document_with_descriptions("Second", "distinct", "other-schema");

    const Document combined = combine_documents(first, second);

    EXPECT_EQ("First + Second", combined.strings().lookup(combined.metadata().title()));
    EXPECT_EQ("shared\ndistinct", combined.strings().lookup(combined.metadata().description()));
    ASSERT_TRUE(combined.source_summary());
    EXPECT_EQ("shared", combined.strings().lookup(combined.source_summary()->schema()));
    EXPECT_EQ(first.source_summary()->schema(), combined.source_summary()->schema());
}

TEST(StringTable, documentBuilderRejectsUnknownLaneIds)
{
    const FrameGrid grid(Timebase(30), 2, 30, 1);
    DocumentBuilder builder(Document(grid, 0, 0));

    EXPECT_THROW(builder.add_lane(Lane(StringId{1}, StringId{2}, StringId{3}, grid.offset(), grid.end_time())),
        std::invalid_argument);
}

TEST(StringTable, documentBuilderRejectsUnknownItemIds)
{
    const FrameGrid grid(Timebase(30), 2, 30, 1);
    DocumentBuilder builder(Document(grid, 0, 0));
    Lane lane(
        builder.intern("known"), builder.intern("Known"), builder.intern("events"), grid.offset(), grid.end_time());
    lane.add(Instant(StringId{100}, builder.intern("event"), grid.offset()));

    EXPECT_THROW(builder.add_lane(std::move(lane)), std::invalid_argument);
}

TEST(StringTable, combinesDocumentsWithIndependentTables)
{
    const Document first = document_with_lane("first");
    const Document second = document_with_lane("second");
    ASSERT_EQ(first.lanes().front().id(), second.lanes().front().id());

    const Document combined = combine_documents(first, second);

    ASSERT_EQ(2, combined.lane_count());
    EXPECT_EQ(first.lanes().front().id(), combined.lanes()[0].id());
    EXPECT_EQ("first", combined.strings().lookup(combined.lanes()[0].id()));
    EXPECT_EQ("added-1-second", combined.strings().lookup(combined.lanes()[1].id()));
    EXPECT_EQ("second", combined.strings().lookup(combined.lanes()[1].label()));
    EXPECT_EQ("events", combined.strings().lookup(combined.lanes()[1].kind()));
    EXPECT_NE(combined.lanes()[0].id(), combined.lanes()[1].id());
    EXPECT_EQ(first_item_id(first, 0), first_item_id(combined, 0));
    EXPECT_EQ("first-item", combined.strings().lookup(first_item_id(combined, 0)));
    EXPECT_EQ("second-item", combined.strings().lookup(first_item_id(combined, 1)));
    EXPECT_EQ("event", combined.strings().lookup(std::get<Instant>(combined.lanes()[1].items().front()).kind()));
    EXPECT_TRUE(std::get<Instant>(combined.lanes()[1].items().front()).label().empty());
    EXPECT_NE(first_item_id(combined, 0), first_item_id(combined, 1));
}

TEST(StringTable, combinesDocumentsThatShareATableWithoutRemappingOriginalIds)
{
    StringTableBuilder strings;
    const StringId first_id = strings.intern("first");
    const StringId second_id = strings.intern("second");
    const StringId first_item = strings.intern("first-item");
    const StringId second_item = strings.intern("second-item");
    const StringId first_label = strings.intern("First");
    const StringId second_label = strings.intern("Second");
    const StringId events_kind = strings.intern("events");
    const StringId event_kind = strings.intern("event");
    const StringTable table = std::move(strings).build();
    const FrameGrid grid(Timebase(30), 2, 30, 1);
    DocumentBuilder first_builder(Document(grid, 0, 0), table);
    Lane first_lane(first_id, first_label, events_kind, grid.offset(), grid.end_time());
    first_lane.add(Instant(first_item, event_kind, grid.offset()));
    first_builder.add_lane(std::move(first_lane));
    const Document first = std::move(first_builder).build();
    DocumentBuilder second_builder(Document(grid, 0, 0), table);
    Lane second_lane(second_id, second_label, events_kind, grid.offset(), grid.end_time());
    second_lane.add(Instant(second_item, event_kind, grid.offset()));
    second_builder.add_lane(std::move(second_lane));
    const Document second = std::move(second_builder).build();

    ASSERT_TRUE(first.strings().shares_storage_with(second.strings()));
    const Document combined = combine_documents(first, second);

    EXPECT_EQ(first_id, combined.lanes()[0].id());
    EXPECT_EQ("first", combined.strings().lookup(first_id));
    EXPECT_EQ("added-1-second", combined.strings().lookup(combined.lanes()[1].id()));
    EXPECT_EQ(first_item, first_item_id(combined, 0));
    EXPECT_EQ(second_item, first_item_id(combined, 1));
}

TEST(StringTable, combiningDocumentsPreservesLaneEvaluators)
{
    const FrameGrid grid(Timebase(30), 2, 30, 1);
    const Document first = document_with_lane("first");
    DocumentBuilder builder(Document(grid, 0, 2));
    Lane lane(
        builder.intern("keys"), builder.intern("Keys"), builder.intern("keyframes"), grid.offset(), grid.end_time());
    lane.add(Keyframe(builder.intern("first-key"), grid.offset(), 1.0));
    lane.add(Keyframe(builder.intern("last-key"), grid.frame_start(1), 2.0));
    lane.set_keyframe_evaluator([](Time) { return 3.0; });
    lane.set_keyframe_output_evaluator([](Time, double value) { return value * 2.0; });
    builder.add_lane(std::move(lane));
    const Document second = std::move(builder).build();

    const Document combined = combine_documents(first, second);

    ASSERT_EQ(2, combined.lane_count());
    EXPECT_DOUBLE_EQ(3.0, *combined.lanes()[1].evaluate_keyframes(grid.offset()));
    EXPECT_DOUBLE_EQ(6.0, *combined.lanes()[1].evaluate_keyframe_output(grid.offset()));
}

TEST(StringTable, storesLongRepeatedAndEmptyDisplayStrings)
{
    constexpr std::string_view LONG_LABEL =
        "A deliberately long lane label that exercises presentation-boundary lookup without copying";
    const FrameGrid grid(Timebase(30), 2, 30, 1);
    DocumentBuilder builder(Document(grid, 0, 0));
    const StringId label = builder.intern(LONG_LABEL);
    const StringId kind = builder.intern("source-defined-kind");
    Lane lane(builder.intern("lane"), label, kind, grid.offset(), grid.end_time());
    lane.add(Instant(builder.intern("labeled"), kind, grid.offset(), label, std::nullopt, {}));
    lane.add(Instant(builder.intern("empty"), kind, grid.frame_start(1)));
    builder.add_lane(std::move(lane));

    const Document document = std::move(builder).build();
    const Lane &stored_lane = document.lanes().front();
    const Instant &labeled = std::get<Instant>(stored_lane.items()[0]);
    const Instant &empty = std::get<Instant>(stored_lane.items()[1]);

    EXPECT_EQ(LONG_LABEL, document.strings().lookup(stored_lane.label()));
    EXPECT_EQ(stored_lane.label(), labeled.label());
    EXPECT_EQ(stored_lane.kind(), labeled.kind());
    EXPECT_EQ("source-defined-kind", document.strings().lookup(labeled.kind()));
    EXPECT_TRUE(empty.label().empty());
    EXPECT_EQ("", document.strings().lookup(empty.label()));
}
