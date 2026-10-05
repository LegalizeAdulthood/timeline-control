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

} // namespace

TEST(Interaction, startsWithoutPlayhead)
{
    Interaction interaction(framed_document());

    EXPECT_FALSE(interaction.playhead());
}

TEST(Interaction, snapsPlayheadToNearestFrame)
{
    Interaction interaction(framed_document());

    interaction.move_playhead(at(86));

    ASSERT_TRUE(interaction.playhead_frame());
    EXPECT_EQ(4, *interaction.playhead_frame());
    EXPECT_EQ(at(90), *interaction.playhead());
}

TEST(Interaction, clampsPlayheadFrameToFirstFrame)
{
    Interaction interaction(framed_document());

    interaction.move_playhead_frame(-100);

    EXPECT_EQ(0, *interaction.playhead_frame());
}

TEST(Interaction, clampsPlayheadFrameToLastFrame)
{
    Interaction interaction(framed_document());

    interaction.move_playhead_frame(100);

    EXPECT_EQ(9, *interaction.playhead_frame());
}

TEST(Interaction, clampsPlayheadTimeToFirstFrame)
{
    Interaction interaction(framed_document());

    interaction.move_playhead(at(-100));

    EXPECT_EQ(at(50), *interaction.playhead());
}

TEST(Interaction, clampsPlayheadTimeToLastFrame)
{
    Interaction interaction(framed_document());

    interaction.move_playhead(at(1000));

    EXPECT_EQ(at(140), *interaction.playhead());
}

TEST(Interaction, formatsPopulatedStateWithDocumentContext)
{
    const Document document = framed_document();
    Interaction interaction(document);
    interaction.move_playhead_frame(1);
    interaction.select_range(at(60), at(80));

    const std::string text = to_string(document, interaction);

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

TEST(Interaction, selectsLaneQualifiedItem)
{
    const Document document = framed_document();
    Interaction interaction(document);
    const HitResult beat = beat_hit(document);

    interaction.select_hit(beat, false);

    EXPECT_EQ(beat.id.lane_id, *interaction.selected_lane());
    ASSERT_EQ(1, size_cast(interaction.selected_items()));
    EXPECT_TRUE(interaction.is_selected(beat.id));
}

TEST(Interaction, selectsItemsAdditivelyAcrossLanes)
{
    const Document document = framed_document();
    Interaction interaction(document);
    const HitResult beat = beat_hit(document);
    const HitResult other{StyleRole::INSTANT_MARKER, DisplayId{document.lanes()[1].id(), beat.id.item_id}};
    interaction.select_hit(beat, false);

    interaction.select_hit(other, true);

    ASSERT_EQ(2, size_cast(interaction.selected_items()));
    EXPECT_TRUE(interaction.is_selected(beat.id));
    EXPECT_TRUE(interaction.is_selected(other.id));
}

TEST(Interaction, togglesAdditiveItemSelection)
{
    const Document document = framed_document();
    Interaction interaction(document);
    const HitResult beat = beat_hit(document);
    const HitResult other{StyleRole::INSTANT_MARKER, DisplayId{document.lanes()[1].id(), beat.id.item_id}};
    interaction.select_hit(beat, false);
    interaction.select_hit(other, true);

    interaction.select_hit(beat, true);

    EXPECT_FALSE(interaction.is_selected(beat.id));
    EXPECT_TRUE(interaction.is_selected(other.id));
}

TEST(Interaction, selectingLaneClearsItemSelection)
{
    const Document document = framed_document();
    Interaction interaction(document);
    const HitResult beat = beat_hit(document);
    interaction.select_hit(beat, false);
    const HitResult lane{StyleRole::LANE_LABEL, DisplayId{beat.id.lane_id, StringId{}}};

    interaction.select_hit(lane, false);

    EXPECT_TRUE(interaction.selected_items().empty());
    EXPECT_EQ(beat.id.lane_id, *interaction.selected_lane());
}

TEST(Interaction, clearsLaneAndItemSelection)
{
    const Document document = framed_document();
    Interaction interaction(document);
    const HitResult beat = beat_hit(document);
    interaction.select_hit(beat, false);

    interaction.clear_selection();

    EXPECT_FALSE(interaction.selected_lane());
    EXPECT_TRUE(interaction.selected_items().empty());
}

TEST(Interaction, selectsInclusiveFrameRangeInReverseDirection)
{
    Interaction interaction(framed_document());

    interaction.select_range(at(113), at(66));

    ASSERT_TRUE(interaction.selected_frames());
    EXPECT_EQ(2, interaction.selected_frames()->first());
    EXPECT_EQ(6, interaction.selected_frames()->last());
    EXPECT_EQ(at(70), interaction.selected_range()->start());
    EXPECT_EQ(at(110), interaction.selected_range()->end());
}

TEST(Interaction, dragsInclusiveFrameRange)
{
    Interaction interaction(framed_document());
    interaction.begin_range(at(80));

    interaction.extend_range(at(99));

    ASSERT_TRUE(interaction.selected_frames());
    EXPECT_EQ(3, interaction.selected_frames()->first());
    EXPECT_EQ(5, interaction.selected_frames()->last());
    EXPECT_EQ(5, *interaction.playhead_frame());
}

TEST(Interaction, ignoresRangeExtensionAfterDragEnds)
{
    Interaction interaction(framed_document());
    interaction.begin_range(at(80));
    interaction.extend_range(at(99));
    interaction.end_range();

    interaction.extend_range(at(130));

    ASSERT_TRUE(interaction.selected_frames());
    EXPECT_EQ(5, interaction.selected_frames()->last());
}

TEST(Interaction, clearsRangeWithoutMovingPlayhead)
{
    Interaction interaction(framed_document());
    interaction.select_range(at(70), at(110));
    interaction.move_playhead_frame(5);

    interaction.clear_selection();

    EXPECT_FALSE(interaction.selected_range());
    EXPECT_EQ(5, *interaction.playhead_frame());
}

TEST(Interaction, preservesClickedItemUntilDragCrossesFrame)
{
    const Document document = framed_document();
    Interaction interaction(document);
    const HitResult beat = beat_hit(document);
    interaction.select_hit(beat, false);
    interaction.begin_range(at(80));

    interaction.extend_range(at(84));

    EXPECT_TRUE(interaction.is_selected(beat.id));
    EXPECT_FALSE(interaction.selected_range());
}

TEST(Interaction, replacesClickedItemWithRangeAfterCrossingFrame)
{
    const Document document = framed_document();
    Interaction interaction(document);
    interaction.select_hit(beat_hit(document), false);
    interaction.begin_range(at(80));

    interaction.extend_range(at(86));

    EXPECT_TRUE(interaction.selected_items().empty());
    ASSERT_TRUE(interaction.selected_frames());
    EXPECT_EQ(3, interaction.selected_frames()->first());
    EXPECT_EQ(4, interaction.selected_frames()->last());
}

TEST(Interaction, clearsSelectionForEmptyHit)
{
    const Document document = framed_document();
    Interaction interaction(document);
    interaction.select_hit(beat_hit(document), false);

    interaction.select_hit(std::nullopt, false);

    EXPECT_FALSE(interaction.selected_lane());
    EXPECT_TRUE(interaction.selected_items().empty());
}

TEST(Interaction, resetsStateForReplacementDocument)
{
    const Document document = framed_document();
    Interaction interaction(document);
    interaction.select_hit(beat_hit(document), false);
    interaction.move_playhead_frame(3);

    interaction = Interaction(document);

    EXPECT_FALSE(interaction.playhead());
    EXPECT_FALSE(interaction.selected_lane());
    EXPECT_TRUE(interaction.selected_items().empty());
}

TEST(TimeRange, rejectsReversedEndpoints)
{
    EXPECT_THROW(TimeRange(at(20), at(10)), std::invalid_argument);
}

TEST(Interaction, extendsKeyboardRangeForwardFromStableAnchor)
{
    Interaction interaction(framed_document());
    interaction.move_playhead_frame(3);

    interaction.step_playhead(1, true);
    interaction.step_playhead(1, true);

    ASSERT_TRUE(interaction.selected_frames());
    EXPECT_EQ(3, interaction.selected_frames()->first());
    EXPECT_EQ(5, interaction.selected_frames()->last());
}

TEST(Interaction, extendsKeyboardRangeBackwardFromStableAnchor)
{
    Interaction interaction(framed_document());
    interaction.move_playhead_frame(3);
    interaction.step_playhead(1, true);
    interaction.step_playhead(1, true);

    interaction.step_playhead(-4, true);

    ASSERT_TRUE(interaction.selected_frames());
    EXPECT_EQ(1, interaction.selected_frames()->first());
    EXPECT_EQ(3, interaction.selected_frames()->last());
}

TEST(Interaction, reanchorsKeyboardRangeAfterLaneSelection)
{
    const Document document = framed_document();
    Interaction interaction(document);
    interaction.move_playhead_frame(1);
    interaction.select_hit(
        HitResult{StyleRole::LANE_LABEL, DisplayId{document.lanes().front().id(), StringId{}}}, false);

    interaction.step_playhead(1, true);

    ASSERT_TRUE(interaction.selected_frames());
    EXPECT_EQ(1, interaction.selected_frames()->first());
    EXPECT_EQ(2, interaction.selected_frames()->last());
}

TEST(Interaction, clearsKeyboardRangeWhenSteppingWithoutExtension)
{
    Interaction interaction(framed_document());
    interaction.move_playhead_frame(3);
    interaction.step_playhead(1, true);

    interaction.step_playhead(1, false);

    EXPECT_EQ(5, *interaction.playhead_frame());
    EXPECT_FALSE(interaction.selected_range());
}

TEST(Interaction, clampsKeyboardStepToFirstFrame)
{
    Interaction interaction(framed_document());
    interaction.move_playhead_frame(3);

    interaction.step_playhead(-100, false);

    EXPECT_EQ(0, *interaction.playhead_frame());
}

TEST(Interaction, clampsKeyboardStepToLastFrame)
{
    Interaction interaction(framed_document());
    interaction.move_playhead_frame(3);

    interaction.step_playhead(100, false);

    EXPECT_EQ(9, *interaction.playhead_frame());
}

TEST(Interaction, rendersSelectedItem)
{
    const Document document = framed_document();
    Interaction interaction(document);
    const HitResult beat = beat_hit(document);
    interaction.select_hit(beat, false);

    const Layout layout(document, Viewport(400, 100, at(50), at(150)), LayoutMetrics(100, 20, 30, 4), interaction);

    EXPECT_TRUE(contains_style(layout, StyleRole::SELECTED_ITEM, beat.id));
}

TEST(Interaction, rendersSelectedLane)
{
    const Document document = framed_document();
    Interaction interaction(document);
    const HitResult beat = beat_hit(document);
    interaction.select_hit(beat, false);

    const Layout layout(document, Viewport(400, 100, at(50), at(150)), LayoutMetrics(100, 20, 30, 4), interaction);

    EXPECT_TRUE(contains_style(layout, StyleRole::SELECTED_LANE, DisplayId{beat.id.lane_id, StringId{}}));
}

TEST(Interaction, rendersPlayhead)
{
    const Document document = framed_document();
    Interaction interaction(document);
    interaction.move_playhead_frame(3);

    const Layout layout(document, Viewport(400, 100, at(50), at(150)), LayoutMetrics(100, 20, 30, 4), interaction);

    EXPECT_TRUE(contains_style(layout, StyleRole::PLAYHEAD));
}

TEST(Interaction, hitsPlayheadAtRuler)
{
    const Document document = framed_document();
    Interaction interaction(document);
    interaction.move_playhead_frame(3);
    const Layout layout(document, Viewport(400, 100, at(50), at(150)), LayoutMetrics(100, 20, 30, 4), interaction);

    const std::optional<HitResult> hit = layout.hit_test(Point{190, 10}, 3);

    ASSERT_TRUE(hit);
    EXPECT_EQ(StyleRole::PLAYHEAD, hit->style);
}

TEST(Interaction, preservesItemHitIdentityUnderSelection)
{
    const Document document = framed_document();
    Interaction interaction(document);
    interaction.select_hit(beat_hit(document), false);
    const Layout layout(document, Viewport(400, 100, at(50), at(150)), LayoutMetrics(100, 20, 30, 4), interaction);

    const std::optional<HitResult> center = layout.hit_test(Point{190, 30}, 0);
    const std::optional<HitResult> edge = layout.hit_test(Point{189, 30}, 0);

    ASSERT_TRUE(center);
    ASSERT_TRUE(edge);
    EXPECT_EQ(StyleRole::INSTANT_MARKER, center->style);
    EXPECT_EQ(StyleRole::INSTANT_MARKER, edge->style);
}

TEST(Interaction, keepsSelectionStateThroughZoom)
{
    const Document document = framed_document();
    Interaction interaction(document);
    const HitResult beat = beat_hit(document);
    interaction.select_hit(beat, false);
    interaction.select_range(at(70), at(110));
    interaction.move_playhead_frame(3);
    Navigation navigation(at(50), at(150), 2);

    navigation.zoom_by(2.0, at(100));

    ASSERT_TRUE(interaction.selected_lane());
    ASSERT_TRUE(interaction.selected_frames());
    EXPECT_EQ(beat.id.lane_id, *interaction.selected_lane());
    EXPECT_EQ(3, *interaction.playhead_frame());
    EXPECT_EQ(2, interaction.selected_frames()->first());
}

TEST(Interaction, keepsSelectionStateThroughLaneScroll)
{
    const Document document = framed_document();
    Interaction interaction(document);
    const HitResult beat = beat_hit(document);
    interaction.select_hit(beat, false);
    interaction.select_range(at(70), at(110));
    interaction.move_playhead_frame(3);
    Navigation navigation(at(50), at(150), 2);

    navigation.scroll_to_lane(1, 1);

    ASSERT_TRUE(interaction.selected_lane());
    ASSERT_TRUE(interaction.selected_frames());
    EXPECT_EQ(beat.id.lane_id, *interaction.selected_lane());
    EXPECT_EQ(3, *interaction.playhead_frame());
    EXPECT_EQ(2, interaction.selected_frames()->first());
}

TEST(Interaction, rendersSelectedRangeAfterNavigation)
{
    const Document document = framed_document();
    Interaction interaction(document);
    interaction.select_range(at(70), at(110));
    Navigation navigation(at(50), at(150), 2);
    navigation.zoom_by(2.0, at(100));
    navigation.scroll_to_lane(1, 1);

    const Layout layout(document, navigation.viewport(400, 50), LayoutMetrics(100, 20, 30, 4), interaction);

    EXPECT_TRUE(contains_style(layout, StyleRole::SELECTED_RANGE));
}

TEST(Interaction, omitsPlayheadOutsideViewport)
{
    const Document document = framed_document();
    Interaction interaction(document);
    interaction.move_playhead_frame(3);

    const Layout layout(document, Viewport(400, 50, at(120), at(150), 1), LayoutMetrics(100, 20, 30, 4), interaction);

    EXPECT_FALSE(contains_style(layout, StyleRole::PLAYHEAD));
}
