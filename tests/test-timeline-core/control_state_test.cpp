// Copyright (c) 2026 Richard Thomson

#include <timeline/ControlState.h>

#include <gtest/gtest.h>

#include <string>
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
    for (int lane_index = 0; lane_index < 6; ++lane_index)
    {
        Lane lane(builder.intern("lane-" + std::to_string(lane_index)),
            builder.intern("Lane " + std::to_string(lane_index)), builder.intern("events"), at(0), at(100));
        if (lane_index == 0)
        {
            lane.add(Instant(builder.intern("beat"), builder.intern("beat"), at(50)));
        }
        builder.add_lane(std::move(lane));
    }
    return std::move(builder).build();
}

/// Framed control state rebuilt with canonical host geometry.
///
class ControlStateCommandTest : public testing::Test
{
protected:
    ControlStateCommandTest()
    {
        m_state.rebuild_layout(400, 80, LayoutMetrics(100, 20, 30, 4));
    }

    Point frame_point(Ticks frame, int lane) const
    {
        return Point{frame_x(frame, *m_state.document()->frame_grid(), *m_state.viewport(), *m_state.layout_metrics()),
            *lane_y(lane, *m_state.viewport(), *m_state.layout_metrics()) +
                m_state.layout_metrics()->lane_height() / 2};
    }

    ControlState m_state{framed_document()};
};

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
    state.rebuild_layout(400, 100, LayoutMetrics(100, 20, 30, 4));
    state.hover_at(Point{0, 0}, 3, false);

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

TEST_F(ControlStateCommandTest, fitsCompleteView)
{
    m_state.zoom_by(2.0);

    m_state.fit_view();

    EXPECT_EQ(at(0), m_state.viewport()->start());
    EXPECT_EQ(at(100), m_state.viewport()->end());
    EXPECT_EQ(0, m_state.viewport()->first_lane());
}

TEST_F(ControlStateCommandTest, zoomsAroundViewportCenter)
{
    m_state.zoom_by(2.0);

    EXPECT_EQ(at(25), m_state.viewport()->start());
    EXPECT_EQ(at(75), m_state.viewport()->end());
}

TEST_F(ControlStateCommandTest, zoomsAtHostCoordinate)
{
    m_state.zoom_at(2.0, m_state.layout_metrics()->lane_label_width());

    EXPECT_EQ(at(0), m_state.viewport()->start());
    EXPECT_EQ(at(50), m_state.viewport()->end());
}

TEST_F(ControlStateCommandTest, scrollsByExactDuration)
{
    m_state.zoom_by(2.0);

    m_state.scroll_by(Duration::from_ticks(10));

    EXPECT_EQ(at(35), m_state.viewport()->start());
    EXPECT_EQ(at(85), m_state.viewport()->end());
}

TEST_F(ControlStateCommandTest, scrollsVisibleLanes)
{
    m_state.scroll_lanes(2);

    EXPECT_EQ(2, m_state.viewport()->first_lane());
}

TEST_F(ControlStateCommandTest, beginsSelectionAtHostCoordinate)
{
    const Point point = frame_point(5, 0);

    const bool range_started = m_state.begin_selection(point, 3, false);

    EXPECT_TRUE(range_started);
    ASSERT_FALSE(m_state.interaction()->selected_items().empty());
    EXPECT_EQ(*m_state.document()->strings().find("beat"), m_state.interaction()->selected_items().front().item_id);
    ASSERT_TRUE(m_state.interaction()->playhead_frame());
    EXPECT_EQ(5, *m_state.interaction()->playhead_frame());
    EXPECT_FALSE(m_state.interaction()->selected_frames());
}

TEST_F(ControlStateCommandTest, extendsSelectionRangeAtHostCoordinate)
{
    m_state.begin_selection(frame_point(2, 0), 3, false);

    m_state.extend_range(frame_point(7, 0));

    ASSERT_TRUE(m_state.interaction()->selected_frames());
    EXPECT_EQ(2, m_state.interaction()->selected_frames()->first());
    EXPECT_EQ(7, m_state.interaction()->selected_frames()->last());
}

TEST_F(ControlStateCommandTest, endsRangeExtension)
{
    m_state.begin_selection(frame_point(2, 0), 3, false);
    m_state.extend_range(frame_point(4, 0));

    m_state.end_range();
    m_state.step_playhead(1, true);

    ASSERT_TRUE(m_state.interaction()->selected_frames());
    EXPECT_EQ(4, m_state.interaction()->selected_frames()->first());
    EXPECT_EQ(5, m_state.interaction()->selected_frames()->last());
}

TEST_F(ControlStateCommandTest, clearsSelection)
{
    m_state.begin_selection(frame_point(5, 0), 3, false);

    m_state.clear_selection();

    EXPECT_FALSE(m_state.interaction()->selected_lane());
    EXPECT_TRUE(m_state.interaction()->selected_items().empty());
    EXPECT_FALSE(m_state.interaction()->selected_range());
}

TEST_F(ControlStateCommandTest, stepsPlayheadAndRevealsIt)
{
    m_state.zoom_by(4.0);

    m_state.step_playhead(9, false);

    ASSERT_TRUE(m_state.interaction()->playhead_frame());
    EXPECT_EQ(9, *m_state.interaction()->playhead_frame());
    ASSERT_TRUE(m_state.inspection());
    EXPECT_EQ(9, m_state.inspection()->frame);
    EXPECT_LE(*m_state.interaction()->playhead(), m_state.viewport()->end());
}

TEST_F(ControlStateCommandTest, updatesHoverHitAndInspection)
{
    m_state.hover_at(frame_point(5, 0), 3, true);

    ASSERT_TRUE(m_state.hit_result());
    EXPECT_EQ(*m_state.document()->strings().find("beat"), m_state.hit_result()->id.item_id);
    ASSERT_TRUE(m_state.inspection());
    EXPECT_EQ(5, m_state.inspection()->frame);
}

TEST_F(ControlStateCommandTest, preservesInspectionForSemanticCommand)
{
    m_state.hover_at(frame_point(5, 0), 3, false);

    ASSERT_TRUE(m_state.inspection());
    EXPECT_EQ(0, m_state.inspection()->frame);
}

TEST_F(ControlStateCommandTest, clearsHoverHit)
{
    m_state.hover_at(frame_point(5, 0), 3, true);

    m_state.clear_hover();

    EXPECT_FALSE(m_state.hit_result());
}
