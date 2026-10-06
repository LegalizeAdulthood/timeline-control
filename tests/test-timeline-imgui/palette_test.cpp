// Copyright (c) 2026 Richard Thomson

#include <imguiTimeline/TimelineRenderer.h>

#include <gtest/gtest.h>

#include <array>

using timeline::StyleRole;

TEST(ImGuiPalette, usesTheSuppliedThemeForLabels)
{
    ImGuiStyle style;
    style.Colors[ImGuiCol_Text] = ImVec4(0.8F, 0.2F, 0.4F, 1.0F);
    const ImU32 expected = ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Text]);
    EXPECT_EQ(expected, timeline_imgui::style_colour(StyleRole::RULER_LABEL, style, true));
    EXPECT_EQ(expected, timeline_imgui::style_colour(StyleRole::LANE_LABEL, style, false));
}

TEST(ImGuiPalette, adaptsEveryRoleToLightAndDarkThemes)
{
    const std::array<StyleRole, 17> roles{StyleRole::RULER, StyleRole::RULER_LABEL, StyleRole::LANE_BACKGROUND,
        StyleRole::LANE_LABEL, StyleRole::INSTANT_MARKER, StyleRole::INTERVAL_SPAN, StyleRole::ENVELOPE_ATTACK,
        StyleRole::ENVELOPE_SUSTAIN, StyleRole::ENVELOPE_DECAY, StyleRole::CURVE, StyleRole::KEYFRAME_SEGMENT,
        StyleRole::KEYFRAME_MARKER, StyleRole::SELECTED_LANE, StyleRole::SELECTED_ITEM, StyleRole::SELECTED_RANGE,
        StyleRole::PLAYHEAD, StyleRole::PALETTE};
    ImGuiStyle light;
    ImGuiStyle dark;
    ImGui::StyleColorsLight(&light);
    ImGui::StyleColorsDark(&dark);
    for (const StyleRole role : roles)
    {
        EXPECT_NE(0U, timeline_imgui::style_colour(role, light, true));
        EXPECT_NE(0U, timeline_imgui::style_colour(role, dark, true));
    }
    EXPECT_NE(timeline_imgui::style_colour(StyleRole::LANE_BACKGROUND, light, true),
        timeline_imgui::style_colour(StyleRole::LANE_BACKGROUND, dark, true));
}

TEST(ImGuiPalette, distinguishesFocusedSelectionAndHonorsStyleAlpha)
{
    ImGuiStyle style;
    for (const StyleRole role : {StyleRole::SELECTED_ITEM, StyleRole::SELECTED_LANE, StyleRole::SELECTED_RANGE})
    {
        EXPECT_NE(timeline_imgui::style_colour(role, style, true), timeline_imgui::style_colour(role, style, false));
    }
    style.Alpha = 0.5F;
    const ImVec4 colour =
        ImGui::ColorConvertU32ToFloat4(timeline_imgui::style_colour(StyleRole::LANE_LABEL, style, true));
    EXPECT_NEAR(0.5F, colour.w, 1.0F / 255.0F);
}

TEST(ImGuiPalette, usesApplicationStyleColor)
{
    timeline_imgui::StyleColors colors;
    colors.emplace(StyleRole::CURVE, IM_COL32(12, 34, 56, 78));

    const ImU32 actual = timeline_imgui::style_colour(StyleRole::CURVE, ImGuiStyle{}, colors, false);

    EXPECT_EQ(IM_COL32(12, 34, 56, 78), actual);
}
