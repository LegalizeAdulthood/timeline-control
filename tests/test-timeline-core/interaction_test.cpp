// Copyright (c) 2026 Richard Thomson

#include <timeline/Interaction.h>

#include <gtest/gtest.h>

#include <stdexcept>

using namespace timeline;

namespace
{

Time at(Ticks ticks)
{
    return Time::from_ticks(ticks);
}

Document framed_document()
{
    Document document(FrameGrid(Timebase(100), 10, 10, 1, at(50)), 0, 0);
    Lane lane("music", "Music", "events", at(50), at(150));
    lane.add(Instant("beat", "beat", at(80)));
    document.add_lane(std::move(lane));
    document.add_lane(Lane("other", "Other", "events", at(50), at(150)));
    return document;
}

} // namespace

TEST(Interaction, snaps_playhead_and_clamps_frame_movement)
{
    Interaction interaction(framed_document());
    EXPECT_FALSE(interaction.playhead());
    interaction.move_playhead(at(86));
    ASSERT_TRUE(interaction.playhead_frame());
    EXPECT_EQ(4, *interaction.playhead_frame());
    EXPECT_EQ(at(90), *interaction.playhead());
    interaction.move_playhead_frame(-100);
    EXPECT_EQ(0, *interaction.playhead_frame());
    interaction.move_playhead_frame(100);
    EXPECT_EQ(9, *interaction.playhead_frame());
    interaction.move_playhead(at(-100));
    EXPECT_EQ(at(50), *interaction.playhead());
    interaction.move_playhead(at(1000));
    EXPECT_EQ(at(140), *interaction.playhead());
}

TEST(Interaction, handles_frameless_and_empty_documents)
{
    Document document(100);
    document.add_lane(Lane("lane", "Lane", "events", at(20), at(80)));
    Interaction interaction(document);
    interaction.move_playhead(at(37));
    EXPECT_EQ(at(37), *interaction.playhead());
    EXPECT_FALSE(interaction.playhead_frame());
    interaction.select_range(at(100), at(-10));
    ASSERT_TRUE(interaction.selected_range());
    EXPECT_EQ(at(20), interaction.selected_range()->start());
    EXPECT_EQ(at(80), interaction.selected_range()->end());
    EXPECT_FALSE(interaction.selected_frames());

    Interaction empty(Document(100));
    empty.move_playhead(at(10));
    empty.move_playhead_frame(10);
    empty.select_range(at(0), at(10));
    EXPECT_FALSE(empty.playhead());
    EXPECT_FALSE(empty.selected_range());
    Interaction zero_frames(Document(FrameGrid(Timebase(100), 0, 10, 1), 0, 0));
    zero_frames.move_playhead_frame(3);
    zero_frames.move_playhead(at(10));
    EXPECT_FALSE(zero_frames.playhead());
}

TEST(Interaction, selects_and_toggles_lane_qualified_item_ids)
{
    Interaction interaction(framed_document());
    const HitResult beat{StyleRole::INSTANT_MARKER, DisplayId{"music", "beat"}};
    const HitResult other{StyleRole::INSTANT_MARKER, DisplayId{"other", "beat"}};
    interaction.select_hit(beat, false);
    EXPECT_EQ("music", *interaction.selected_lane());
    ASSERT_EQ(1, size_cast(interaction.selected_items()));
    interaction.select_hit(other, true);
    EXPECT_EQ(2, size_cast(interaction.selected_items()));
    EXPECT_TRUE(interaction.is_selected(beat.id));
    EXPECT_TRUE(interaction.is_selected(other.id));
    interaction.select_hit(beat, true);
    EXPECT_FALSE(interaction.is_selected(beat.id));
    EXPECT_TRUE(interaction.is_selected(other.id));
    interaction.select_hit(HitResult{StyleRole::LANE_LABEL, DisplayId{"music", ""}}, false);
    EXPECT_TRUE(interaction.selected_items().empty());
    EXPECT_EQ("music", *interaction.selected_lane());
    interaction.clear_selection();
    EXPECT_FALSE(interaction.selected_lane());
    EXPECT_TRUE(interaction.selected_items().empty());
}

TEST(Interaction, selects_inclusive_frame_ranges_in_either_direction)
{
    Interaction interaction(framed_document());
    interaction.select_range(at(113), at(66));
    ASSERT_TRUE(interaction.selected_frames());
    EXPECT_EQ(2, interaction.selected_frames()->first());
    EXPECT_EQ(6, interaction.selected_frames()->last());
    EXPECT_EQ(at(70), interaction.selected_range()->start());
    EXPECT_EQ(at(110), interaction.selected_range()->end());
    interaction.begin_range(at(80));
    interaction.extend_range(at(99));
    EXPECT_EQ(3, interaction.selected_frames()->first());
    EXPECT_EQ(5, interaction.selected_frames()->last());
    EXPECT_EQ(5, *interaction.playhead_frame());
    interaction.end_range();
    interaction.extend_range(at(130));
    EXPECT_EQ(5, interaction.selected_frames()->last());
    interaction.clear_selection();
    EXPECT_FALSE(interaction.selected_range());
    EXPECT_EQ(5, *interaction.playhead_frame());
}

TEST(Interaction, preserves_clicked_items_until_a_drag_crosses_a_frame)
{
    const Document document = framed_document();
    Interaction interaction(document);
    const HitResult beat{StyleRole::INSTANT_MARKER, DisplayId{"music", "beat"}};
    interaction.select_hit(beat, false);
    interaction.begin_range(at(80));
    interaction.extend_range(at(84));
    EXPECT_TRUE(interaction.is_selected(beat.id));
    EXPECT_FALSE(interaction.selected_range());
    interaction.extend_range(at(86));
    EXPECT_TRUE(interaction.selected_items().empty());
    EXPECT_EQ(3, interaction.selected_frames()->first());
    EXPECT_EQ(4, interaction.selected_frames()->last());
    interaction.end_range();
    interaction.select_hit(std::nullopt, false);
    EXPECT_FALSE(interaction.selected_range());
    EXPECT_FALSE(interaction.selected_lane());
    interaction = Interaction(document);
    EXPECT_FALSE(interaction.playhead());
    EXPECT_TRUE(interaction.selected_items().empty());
    EXPECT_THROW(TimeRange(at(20), at(10)), std::invalid_argument);
}

TEST(Interaction, extends_keyboard_ranges_from_a_stable_anchor)
{
    Interaction interaction(framed_document());
    interaction.move_playhead_frame(3);
    interaction.step_playhead(1, true);
    interaction.step_playhead(1, true);
    EXPECT_EQ(3, interaction.selected_frames()->first());
    EXPECT_EQ(5, interaction.selected_frames()->last());
    interaction.step_playhead(-4, true);
    EXPECT_EQ(1, interaction.selected_frames()->first());
    EXPECT_EQ(3, interaction.selected_frames()->last());
    interaction.select_hit(HitResult{StyleRole::LANE_LABEL, DisplayId{"music", ""}}, false);
    interaction.step_playhead(1, true);
    EXPECT_EQ(1, interaction.selected_frames()->first());
    EXPECT_EQ(2, interaction.selected_frames()->last());
    interaction.step_playhead(-100, false);
    EXPECT_EQ(0, *interaction.playhead_frame());
    EXPECT_FALSE(interaction.selected_range());
    interaction.step_playhead(100, false);
    EXPECT_EQ(9, *interaction.playhead_frame());
}

TEST(Interaction, renders_selection_without_changing_hit_identity)
{
    const Document document = framed_document();
    Interaction interaction(document);
    const HitResult beat{StyleRole::INSTANT_MARKER, DisplayId{"music", "beat"}};
    interaction.select_hit(beat, false);
    interaction.move_playhead_frame(3);
    const LayoutMetrics metrics(100, 20, 30, 4);
    const Viewport viewport(400, 100, at(50), at(150));
    const Layout layout(document, viewport, metrics, interaction);
    bool selected_marker = false;
    bool selected_lane = false;
    bool playhead = false;
    for (const Primitive &primitive : layout.display_list().primitives())
    {
        std::visit(
            [&](const auto &value)
            {
                selected_marker |= value.style == StyleRole::SELECTED_ITEM && value.id.item_id == "beat";
                selected_lane |= value.style == StyleRole::SELECTED_LANE && value.id.lane_id == "music";
                playhead |= value.style == StyleRole::PLAYHEAD;
            },
            primitive);
    }
    EXPECT_TRUE(selected_marker);
    EXPECT_TRUE(selected_lane);
    EXPECT_TRUE(playhead);
    const std::optional<HitResult> hit = layout.hit_test(Point{190, 10}, 3);
    ASSERT_TRUE(hit);
    EXPECT_EQ(StyleRole::PLAYHEAD, hit->style);
    EXPECT_EQ(StyleRole::INSTANT_MARKER, layout.hit_test(Point{190, 30}, 0)->style);
    EXPECT_EQ(StyleRole::INSTANT_MARKER, layout.hit_test(Point{189, 30}, 0)->style);
}

TEST(Interaction, keeps_selection_through_zoom_and_lane_scroll)
{
    const Document document = framed_document();
    Interaction interaction(document);
    interaction.select_hit(HitResult{StyleRole::INSTANT_MARKER, DisplayId{"music", "beat"}}, false);
    interaction.select_range(at(70), at(110));
    interaction.move_playhead_frame(3);
    Navigation navigation(at(50), at(150), 2);
    navigation.zoom_by(2.0, at(100));
    navigation.scroll_to_lane(1, 1);
    const Layout layout(document, navigation.viewport(400, 50), LayoutMetrics(100, 20, 30, 4), interaction);
    EXPECT_EQ("music", *interaction.selected_lane());
    EXPECT_EQ(3, *interaction.playhead_frame());
    EXPECT_EQ(2, interaction.selected_frames()->first());
    bool range_visible = false;
    for (const Primitive &primitive : layout.display_list().primitives())
    {
        std::visit([&](const auto &value) { range_visible |= value.style == StyleRole::SELECTED_RANGE; }, primitive);
    }
    EXPECT_TRUE(range_visible);
    const Layout hidden(document, Viewport(400, 50, at(120), at(150), 1), LayoutMetrics(100, 20, 30, 4), interaction);
    for (const Primitive &primitive : hidden.display_list().primitives())
    {
        std::visit([](const auto &value) { EXPECT_NE(StyleRole::PLAYHEAD, value.style); }, primitive);
    }
}
