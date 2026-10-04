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
    builder.add_lane(Lane(builder.intern(id), std::string(id), "events", grid.offset(), grid.end_time()));
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

    EXPECT_EQ("events", copy.strings().lookup(copy.lanes().front().id()));
    const Layout layout(copy, Viewport(200, 60, Time{}, Time::from_ticks(2)), LayoutMetrics(80, 20, 30, 2));
    const DisplayList display = layout.display_list();
    EXPECT_TRUE(display.strings().shares_storage_with(copy.strings()));
    EXPECT_EQ("events", display.strings().lookup(copy.lanes().front().id()));
}

TEST(StringTable, documentBuilderRejectsUnknownLaneIds)
{
    const FrameGrid grid(Timebase(30), 2, 30, 1);
    DocumentBuilder builder(Document(grid, 0, 0));

    EXPECT_THROW(builder.add_lane(Lane(StringId{1}, "Unknown", "events", grid.offset(), grid.end_time())),
        std::invalid_argument);
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
    EXPECT_NE(combined.lanes()[0].id(), combined.lanes()[1].id());
}

TEST(StringTable, combinesDocumentsThatShareATableWithoutRemappingOriginalIds)
{
    StringTableBuilder strings;
    const StringId first_id = strings.intern("first");
    const StringId second_id = strings.intern("second");
    const StringTable table = std::move(strings).build();
    const FrameGrid grid(Timebase(30), 2, 30, 1);
    DocumentBuilder first_builder(Document(grid, 0, 0), table);
    first_builder.add_lane(Lane(first_id, "First", "events", grid.offset(), grid.end_time()));
    const Document first = std::move(first_builder).build();
    DocumentBuilder second_builder(Document(grid, 0, 0), table);
    second_builder.add_lane(Lane(second_id, "Second", "events", grid.offset(), grid.end_time()));
    const Document second = std::move(second_builder).build();

    ASSERT_TRUE(first.strings().shares_storage_with(second.strings()));
    const Document combined = combine_documents(first, second);

    EXPECT_EQ(first_id, combined.lanes()[0].id());
    EXPECT_EQ("first", combined.strings().lookup(first_id));
    EXPECT_EQ("added-1-second", combined.strings().lookup(combined.lanes()[1].id()));
}
