// Copyright (c) 2026 Richard Thomson

#include <timeline/Interaction.h>
#include <timeline/size_cast.h>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

using namespace timeline;

namespace
{

Time at(Ticks ticks)
{
    return Time::from_ticks(ticks);
}

Document framed_document()
{
    DocumentBuilder builder(Document(FrameGrid(Timebase(100), 10, 10, 1, at(50)), 0, 0));
    Lane lane(builder.intern("music"), builder.intern("Music"), builder.intern("events"), at(50), at(150));
    lane.add(Instant(builder.intern("beat"), builder.intern("beat"), at(80)));
    builder.add_lane(std::move(lane));
    builder.add_lane(Lane(builder.intern("other"), builder.intern("Other"), builder.intern("events"), at(50), at(150)));
    return std::move(builder).build();
}

HitResult beat_hit(const Document &document)
{
    const Lane &lane = document.lanes().front();
    const Instant &beat = std::get<Instant>(lane.items().front());
    return HitResult{StyleRole::INSTANT_MARKER, DisplayId{lane.id(), beat.id()}};
}

bool contains_style(const Layout &layout, StyleRole style)
{
    bool found = false;
    for (const Primitive &primitive : layout.display_list().primitives())
    {
        std::visit([&](const auto &value) { found |= value.style == style; }, primitive);
    }
    return found;
}

bool contains_style(const Layout &layout, StyleRole style, const DisplayId &id)
{
    bool found = false;
    for (const Primitive &primitive : layout.display_list().primitives())
    {
        std::visit([&](const auto &value)
            { found |= value.style == style && value.id.lane_id == id.lane_id && value.id.item_id == id.item_id; },
            primitive);
    }
    return found;
}

/// Fresh framed interaction state with canonical layout geometry.
///
class FramedInteractionTest : public testing::Test
{
protected:
    Layout make_layout() const
    {
        return Layout(m_document, m_viewport, m_metrics, m_interaction);
    }

    const Document m_document{framed_document()};
    Interaction m_interaction{m_document};
    const HitResult m_beat{beat_hit(m_document)};
    const Viewport m_viewport{400, 100, at(50), at(150)};
    const LayoutMetrics m_metrics{100, 20, 30, 4};
};

} // namespace

TEST_F(FramedInteractionTest, startsWithoutPlayhead)
{
    EXPECT_FALSE(m_interaction.playhead());
}

TEST_F(FramedInteractionTest, snapsPlayheadToNearestFrame)
{
    m_interaction.move_playhead(at(86));

    ASSERT_TRUE(m_interaction.playhead_frame());
    EXPECT_EQ(4, *m_interaction.playhead_frame());
    EXPECT_EQ(at(90), *m_interaction.playhead());
}

TEST_F(FramedInteractionTest, clampsPlayheadFrameToFirstFrame)
{
    m_interaction.move_playhead_frame(-100);

    EXPECT_EQ(0, *m_interaction.playhead_frame());
}

TEST_F(FramedInteractionTest, clampsPlayheadFrameToLastFrame)
{
    m_interaction.move_playhead_frame(100);

    EXPECT_EQ(9, *m_interaction.playhead_frame());
}

TEST_F(FramedInteractionTest, clampsPlayheadTimeToFirstFrame)
{
    m_interaction.move_playhead(at(-100));

    EXPECT_EQ(at(50), *m_interaction.playhead());
}

TEST_F(FramedInteractionTest, clampsPlayheadTimeToLastFrame)
{
    m_interaction.move_playhead(at(1000));

    EXPECT_EQ(at(140), *m_interaction.playhead());
}

TEST_F(FramedInteractionTest, formatsPopulatedStateWithDocumentContext)
{
    m_interaction.move_playhead_frame(1);
    m_interaction.select_range(at(60), at(80));

    const std::string text = to_string(m_document, m_interaction);

    EXPECT_NE(std::string::npos, text.find("Playhead: 0.6 seconds"));
    EXPECT_NE(std::string::npos, text.find("Playhead frame: 1"));
    EXPECT_NE(std::string::npos, text.find("Selected range: 0.6 to 0.8 seconds"));
    EXPECT_NE(std::string::npos, text.find("Selected frames: 1 to 3"));
}

TEST(Interaction, movesFramelessPlayheadWithoutSnapping)
{
    DocumentBuilder builder(Document(100));
    builder.add_lane(Lane(builder.intern("lane"), builder.intern("Lane"), builder.intern("events"), at(20), at(80)));
    const Document document = std::move(builder).build();
    Interaction interaction(document);

    interaction.move_playhead(at(37));

    EXPECT_EQ(at(37), *interaction.playhead());
    EXPECT_FALSE(interaction.playhead_frame());
}

TEST(Interaction, clampsFramelessRangeToContent)
{
    DocumentBuilder builder(Document(100));
    builder.add_lane(Lane(builder.intern("lane"), builder.intern("Lane"), builder.intern("events"), at(20), at(80)));
    const Document document = std::move(builder).build();
    Interaction interaction(document);

    interaction.select_range(at(100), at(-10));

    ASSERT_TRUE(interaction.selected_range());
    EXPECT_EQ(at(20), interaction.selected_range()->start());
    EXPECT_EQ(at(80), interaction.selected_range()->end());
    EXPECT_FALSE(interaction.selected_frames());
}

TEST(Interaction, ignoresPlayheadTimeForEmptyDocument)
{
    const Document document(100);
    Interaction interaction(document);

    interaction.move_playhead(at(10));

    EXPECT_FALSE(interaction.playhead());
}

TEST(Interaction, ignoresPlayheadFrameForEmptyDocument)
{
    const Document document(100);
    Interaction interaction(document);

    interaction.move_playhead_frame(10);

    EXPECT_FALSE(interaction.playhead());
}

TEST(Interaction, ignoresRangeSelectionForEmptyDocument)
{
    const Document document(100);
    Interaction interaction(document);

    interaction.select_range(at(0), at(10));

    EXPECT_FALSE(interaction.selected_range());
}

TEST(Interaction, ignoresPlayheadTimeForZeroFrameDocument)
{
    const Document document(FrameGrid(Timebase(100), 0, 10, 1), 0, 0);
    Interaction interaction(document);

    interaction.move_playhead(at(10));

    EXPECT_FALSE(interaction.playhead());
}

TEST(Interaction, ignoresPlayheadFrameForZeroFrameDocument)
{
    const Document document(FrameGrid(Timebase(100), 0, 10, 1), 0, 0);
    Interaction interaction(document);

    interaction.move_playhead_frame(3);

    EXPECT_FALSE(interaction.playhead());
}

TEST_F(FramedInteractionTest, selectsLaneQualifiedItem)
{
    m_interaction.select_hit(m_beat, false);

    EXPECT_EQ(m_beat.id.lane_id, *m_interaction.selected_lane());
    ASSERT_EQ(1, size_cast(m_interaction.selected_items()));
    EXPECT_TRUE(m_interaction.is_selected(m_beat.id));
}

TEST_F(FramedInteractionTest, selectsItemsAdditivelyAcrossLanes)
{
    const HitResult other{StyleRole::INSTANT_MARKER, DisplayId{m_document.lanes()[1].id(), m_beat.id.item_id}};
    m_interaction.select_hit(m_beat, false);

    m_interaction.select_hit(other, true);

    ASSERT_EQ(2, size_cast(m_interaction.selected_items()));
    EXPECT_TRUE(m_interaction.is_selected(m_beat.id));
    EXPECT_TRUE(m_interaction.is_selected(other.id));
}

TEST_F(FramedInteractionTest, togglesAdditiveItemSelection)
{
    const HitResult other{StyleRole::INSTANT_MARKER, DisplayId{m_document.lanes()[1].id(), m_beat.id.item_id}};
    m_interaction.select_hit(m_beat, false);
    m_interaction.select_hit(other, true);

    m_interaction.select_hit(m_beat, true);

    EXPECT_FALSE(m_interaction.is_selected(m_beat.id));
    EXPECT_TRUE(m_interaction.is_selected(other.id));
}

TEST_F(FramedInteractionTest, selectingLaneClearsItemSelection)
{
    m_interaction.select_hit(m_beat, false);
    const HitResult lane{StyleRole::LANE_LABEL, DisplayId{m_beat.id.lane_id, StringId{}}};

    m_interaction.select_hit(lane, false);

    EXPECT_TRUE(m_interaction.selected_items().empty());
    EXPECT_EQ(m_beat.id.lane_id, *m_interaction.selected_lane());
}

TEST_F(FramedInteractionTest, clearsLaneAndItemSelection)
{
    m_interaction.select_hit(m_beat, false);

    m_interaction.clear_selection();

    EXPECT_FALSE(m_interaction.selected_lane());
    EXPECT_TRUE(m_interaction.selected_items().empty());
}

TEST_F(FramedInteractionTest, selectsInclusiveFrameRangeInReverseDirection)
{
    m_interaction.select_range(at(113), at(66));

    ASSERT_TRUE(m_interaction.selected_frames());
    EXPECT_EQ(2, m_interaction.selected_frames()->first());
    EXPECT_EQ(6, m_interaction.selected_frames()->last());
    EXPECT_EQ(at(70), m_interaction.selected_range()->start());
    EXPECT_EQ(at(110), m_interaction.selected_range()->end());
}

TEST_F(FramedInteractionTest, dragsInclusiveFrameRange)
{
    m_interaction.begin_range(at(80));

    m_interaction.extend_range(at(99));

    ASSERT_TRUE(m_interaction.selected_frames());
    EXPECT_EQ(3, m_interaction.selected_frames()->first());
    EXPECT_EQ(5, m_interaction.selected_frames()->last());
    EXPECT_EQ(5, *m_interaction.playhead_frame());
}

TEST_F(FramedInteractionTest, ignoresRangeExtensionAfterDragEnds)
{
    m_interaction.begin_range(at(80));
    m_interaction.extend_range(at(99));
    m_interaction.end_range();

    m_interaction.extend_range(at(130));

    ASSERT_TRUE(m_interaction.selected_frames());
    EXPECT_EQ(5, m_interaction.selected_frames()->last());
}

TEST_F(FramedInteractionTest, clearsRangeWithoutMovingPlayhead)
{
    m_interaction.select_range(at(70), at(110));
    m_interaction.move_playhead_frame(5);

    m_interaction.clear_selection();

    EXPECT_FALSE(m_interaction.selected_range());
    EXPECT_EQ(5, *m_interaction.playhead_frame());
}

TEST_F(FramedInteractionTest, preservesClickedItemUntilDragCrossesFrame)
{
    m_interaction.select_hit(m_beat, false);
    m_interaction.begin_range(at(80));

    m_interaction.extend_range(at(84));

    EXPECT_TRUE(m_interaction.is_selected(m_beat.id));
    EXPECT_FALSE(m_interaction.selected_range());
}

TEST_F(FramedInteractionTest, replacesClickedItemWithRangeAfterCrossingFrame)
{
    m_interaction.select_hit(m_beat, false);
    m_interaction.begin_range(at(80));

    m_interaction.extend_range(at(86));

    EXPECT_TRUE(m_interaction.selected_items().empty());
    ASSERT_TRUE(m_interaction.selected_frames());
    EXPECT_EQ(3, m_interaction.selected_frames()->first());
    EXPECT_EQ(4, m_interaction.selected_frames()->last());
}

TEST_F(FramedInteractionTest, clearsSelectionForEmptyHit)
{
    m_interaction.select_hit(m_beat, false);

    m_interaction.select_hit(std::nullopt, false);

    EXPECT_FALSE(m_interaction.selected_lane());
    EXPECT_TRUE(m_interaction.selected_items().empty());
}

TEST_F(FramedInteractionTest, resetsStateForReplacementDocument)
{
    m_interaction.select_hit(m_beat, false);
    m_interaction.move_playhead_frame(3);

    m_interaction = Interaction(m_document);

    EXPECT_FALSE(m_interaction.playhead());
    EXPECT_FALSE(m_interaction.selected_lane());
    EXPECT_TRUE(m_interaction.selected_items().empty());
}

TEST(TimeRange, rejectsReversedEndpoints)
{
    EXPECT_THROW(TimeRange(at(20), at(10)), std::invalid_argument);
}

TEST_F(FramedInteractionTest, extendsKeyboardRangeForwardFromStableAnchor)
{
    m_interaction.move_playhead_frame(3);

    m_interaction.step_playhead(1, true);
    m_interaction.step_playhead(1, true);

    ASSERT_TRUE(m_interaction.selected_frames());
    EXPECT_EQ(3, m_interaction.selected_frames()->first());
    EXPECT_EQ(5, m_interaction.selected_frames()->last());
}

TEST_F(FramedInteractionTest, extendsKeyboardRangeBackwardFromStableAnchor)
{
    m_interaction.move_playhead_frame(3);
    m_interaction.step_playhead(1, true);
    m_interaction.step_playhead(1, true);

    m_interaction.step_playhead(-4, true);

    ASSERT_TRUE(m_interaction.selected_frames());
    EXPECT_EQ(1, m_interaction.selected_frames()->first());
    EXPECT_EQ(3, m_interaction.selected_frames()->last());
}

TEST_F(FramedInteractionTest, reanchorsKeyboardRangeAfterLaneSelection)
{
    m_interaction.move_playhead_frame(1);
    m_interaction.select_hit(
        HitResult{StyleRole::LANE_LABEL, DisplayId{m_document.lanes().front().id(), StringId{}}}, false);

    m_interaction.step_playhead(1, true);

    ASSERT_TRUE(m_interaction.selected_frames());
    EXPECT_EQ(1, m_interaction.selected_frames()->first());
    EXPECT_EQ(2, m_interaction.selected_frames()->last());
}

TEST_F(FramedInteractionTest, clearsKeyboardRangeWhenSteppingWithoutExtension)
{
    m_interaction.move_playhead_frame(3);
    m_interaction.step_playhead(1, true);

    m_interaction.step_playhead(1, false);

    EXPECT_EQ(5, *m_interaction.playhead_frame());
    EXPECT_FALSE(m_interaction.selected_range());
}

TEST_F(FramedInteractionTest, clampsKeyboardStepToFirstFrame)
{
    m_interaction.move_playhead_frame(3);

    m_interaction.step_playhead(-100, false);

    EXPECT_EQ(0, *m_interaction.playhead_frame());
}

TEST_F(FramedInteractionTest, clampsKeyboardStepToLastFrame)
{
    m_interaction.move_playhead_frame(3);

    m_interaction.step_playhead(100, false);

    EXPECT_EQ(9, *m_interaction.playhead_frame());
}

TEST_F(FramedInteractionTest, rendersSelectedItem)
{
    m_interaction.select_hit(m_beat, false);

    const Layout layout = make_layout();

    EXPECT_TRUE(contains_style(layout, StyleRole::SELECTED_ITEM, m_beat.id));
}

TEST_F(FramedInteractionTest, rendersSelectedLane)
{
    m_interaction.select_hit(m_beat, false);

    const Layout layout = make_layout();

    EXPECT_TRUE(contains_style(layout, StyleRole::SELECTED_LANE, DisplayId{m_beat.id.lane_id, StringId{}}));
}

TEST_F(FramedInteractionTest, rendersPlayhead)
{
    m_interaction.move_playhead_frame(3);

    const Layout layout = make_layout();

    EXPECT_TRUE(contains_style(layout, StyleRole::PLAYHEAD));
}

TEST_F(FramedInteractionTest, hitsPlayheadAtRuler)
{
    m_interaction.move_playhead_frame(3);
    const Layout layout = make_layout();

    const std::optional<HitResult> hit = layout.hit_test(Point{190, 10}, 3);

    ASSERT_TRUE(hit);
    EXPECT_EQ(StyleRole::PLAYHEAD, hit->style);
}

TEST_F(FramedInteractionTest, preservesItemHitIdentityUnderSelection)
{
    m_interaction.select_hit(m_beat, false);
    const Layout layout = make_layout();

    const std::optional<HitResult> center = layout.hit_test(Point{190, 30}, 0);
    const std::optional<HitResult> edge = layout.hit_test(Point{189, 30}, 0);

    ASSERT_TRUE(center);
    ASSERT_TRUE(edge);
    EXPECT_EQ(StyleRole::INSTANT_MARKER, center->style);
    EXPECT_EQ(StyleRole::INSTANT_MARKER, edge->style);
}

TEST_F(FramedInteractionTest, keepsSelectionStateThroughZoom)
{
    m_interaction.select_hit(m_beat, false);
    m_interaction.select_range(at(70), at(110));
    m_interaction.move_playhead_frame(3);
    Navigation navigation(at(50), at(150), 2);

    navigation.zoom_by(2.0, at(100));

    ASSERT_TRUE(m_interaction.selected_lane());
    ASSERT_TRUE(m_interaction.selected_frames());
    EXPECT_EQ(m_beat.id.lane_id, *m_interaction.selected_lane());
    EXPECT_EQ(3, *m_interaction.playhead_frame());
    EXPECT_EQ(2, m_interaction.selected_frames()->first());
}

TEST_F(FramedInteractionTest, keepsSelectionStateThroughLaneScroll)
{
    m_interaction.select_hit(m_beat, false);
    m_interaction.select_range(at(70), at(110));
    m_interaction.move_playhead_frame(3);
    Navigation navigation(at(50), at(150), 2);

    navigation.scroll_to_lane(1, 1);

    ASSERT_TRUE(m_interaction.selected_lane());
    ASSERT_TRUE(m_interaction.selected_frames());
    EXPECT_EQ(m_beat.id.lane_id, *m_interaction.selected_lane());
    EXPECT_EQ(3, *m_interaction.playhead_frame());
    EXPECT_EQ(2, m_interaction.selected_frames()->first());
}

TEST_F(FramedInteractionTest, rendersSelectedRangeAfterNavigation)
{
    m_interaction.select_range(at(70), at(110));
    Navigation navigation(at(50), at(150), 2);
    navigation.zoom_by(2.0, at(100));
    navigation.scroll_to_lane(1, 1);

    const Layout layout(m_document, navigation.viewport(400, 50), m_metrics, m_interaction);

    EXPECT_TRUE(contains_style(layout, StyleRole::SELECTED_RANGE));
}

TEST_F(FramedInteractionTest, omitsPlayheadOutsideViewport)
{
    m_interaction.move_playhead_frame(3);

    const Layout layout(m_document, Viewport(400, 50, at(120), at(150), 1), m_metrics, m_interaction);

    EXPECT_FALSE(contains_style(layout, StyleRole::PLAYHEAD));
}
