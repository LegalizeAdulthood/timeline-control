// Copyright (c) 2026 Richard Thomson

#include <timeline/ControlState.h>

#include <gtest/gtest.h>

#include <utility>

using namespace timeline;

namespace
{

Time at(Ticks ticks)
{
    return Time::from_ticks(ticks);
}

Document framed_document()
{
    DocumentBuilder builder(Document(FrameGrid(Timebase(100), 10, 10, 1), 0, 0));
    Lane lane(builder.intern("events"), builder.intern("Events"), builder.intern("events"), at(0), at(100));
    lane.add(Instant(builder.intern("beat"), builder.intern("beat"), at(50)));
    builder.add_lane(std::move(lane));
    return std::move(builder).build();
}

} // namespace

TEST(ControlState, startsWithoutDocument)
{
    const ControlState state;

    EXPECT_FALSE(state.document());
    EXPECT_FALSE(state.interaction());
    EXPECT_FALSE(state.navigation());
    EXPECT_FALSE(state.inspection());
    EXPECT_FALSE(state.hit_result());
    EXPECT_FALSE(state.layout());
    EXPECT_FALSE(state.viewport());
    EXPECT_FALSE(state.layout_metrics());
}

TEST(ControlState, initializesDocumentInteractionAndNavigation)
{
    const ControlState state(framed_document());

    EXPECT_TRUE(state.document());
    EXPECT_TRUE(state.interaction());
    EXPECT_TRUE(state.navigation());
}

TEST(ControlState, initializesInspectionAtFirstFrame)
{
    const ControlState state(framed_document());

    ASSERT_TRUE(state.inspection());
    EXPECT_EQ(0, state.inspection()->frame);
    EXPECT_EQ(at(0), state.inspection()->time);
}

TEST(ControlState, resetsDerivedStateForReplacementDocument)
{
    ControlState state(framed_document());
    state.hit_result() = HitResult{StyleRole::RULER, DisplayId{}};
    state.rebuild_layout(400, 100, LayoutMetrics(100, 20, 30, 4));

    state.set_document(Document(100));

    EXPECT_TRUE(state.document());
    EXPECT_TRUE(state.interaction());
    EXPECT_FALSE(state.navigation());
    EXPECT_FALSE(state.inspection());
    EXPECT_FALSE(state.hit_result());
    EXPECT_FALSE(state.layout());
    EXPECT_FALSE(state.viewport());
    EXPECT_FALSE(state.layout_metrics());
}

TEST(ControlState, rebuildsLayoutFromHostGeometry)
{
    ControlState state(framed_document());

    const bool rebuilt = state.rebuild_layout(400, 100, LayoutMetrics(100, 20, 30, 4));

    EXPECT_TRUE(rebuilt);
    ASSERT_TRUE(state.viewport());
    EXPECT_EQ(400, state.viewport()->width());
    EXPECT_EQ(100, state.viewport()->height());
    ASSERT_TRUE(state.layout_metrics());
    EXPECT_EQ(100, state.layout_metrics()->lane_label_width());
    EXPECT_TRUE(state.layout());
}

TEST(ControlState, leavesInvalidHostGeometryUnlaidOut)
{
    ControlState state(framed_document());

    const bool rebuilt = state.rebuild_layout(1, 20, LayoutMetrics(100, 20, 30, 4));

    EXPECT_FALSE(rebuilt);
    EXPECT_FALSE(state.layout());
    EXPECT_FALSE(state.viewport());
    EXPECT_FALSE(state.layout_metrics());
}
