// Copyright (c) 2026 Richard Thomson

#include <wxTimeline/wxTimelineRenderer.h>

#include <gtest/gtest.h>

#include <array>

using timeline::StyleRole;

namespace
{

const wxTimelinePalette LIGHT{wxColour(255, 255, 255), wxColour(0, 0, 0), wxColour(0, 100, 200)};
const wxTimelinePalette DARK{wxColour(24, 24, 24), wxColour(235, 235, 235), wxColour(70, 160, 230)};

} // namespace

TEST(WxPalette, uses_native_theme_colors_for_labels_and_selection)
{
    for (const wxTimelinePalette &palette : {LIGHT, DARK})
    {
        EXPECT_EQ(palette.foreground, timeline_style_colour(StyleRole::LANE_LABEL, palette, true));
        EXPECT_EQ(palette.foreground, timeline_style_colour(StyleRole::RULER_LABEL, palette, true));
        EXPECT_EQ(palette.highlight, timeline_style_colour(StyleRole::SELECTED_ITEM, palette, true));
        EXPECT_NE(palette.highlight, timeline_style_colour(StyleRole::SELECTED_ITEM, palette, false));
    }
}

TEST(WxPalette, adapts_every_style_to_light_and_dark_backgrounds)
{
    const std::array<StyleRole, 17> roles{StyleRole::RULER, StyleRole::RULER_LABEL, StyleRole::LANE_BACKGROUND,
        StyleRole::LANE_LABEL, StyleRole::INSTANT_MARKER, StyleRole::INTERVAL_SPAN, StyleRole::ENVELOPE_ATTACK,
        StyleRole::ENVELOPE_SUSTAIN, StyleRole::ENVELOPE_DECAY, StyleRole::CURVE, StyleRole::KEYFRAME_SEGMENT,
        StyleRole::KEYFRAME_MARKER, StyleRole::SELECTED_LANE, StyleRole::SELECTED_ITEM, StyleRole::SELECTED_RANGE,
        StyleRole::PLAYHEAD, StyleRole::PALETTE};
    for (const StyleRole role : roles)
    {
        const wxColour light = timeline_style_colour(role, LIGHT, true);
        const wxColour dark = timeline_style_colour(role, DARK, true);
        EXPECT_TRUE(light.IsOk());
        EXPECT_TRUE(dark.IsOk());
        EXPECT_NE(light, dark);
        EXPECT_NE(LIGHT.background, light);
        EXPECT_NE(DARK.background, dark);
    }
}
