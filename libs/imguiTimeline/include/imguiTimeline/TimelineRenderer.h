// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/DisplayList.h>

#include <imgui.h>

#include <map>

namespace timeline_imgui
{

/// Application-selected packed colors for semantic style roles.
using StyleColors = std::map<timeline::StyleRole, ImU32>;

/// Maps a semantic drawing role to the supplied ImGui theme and focus state.
ImU32 style_colour(timeline::StyleRole role, const ImGuiStyle &style, bool focused);

/// Returns an application override or the native default for a style role.
ImU32 style_colour(timeline::StyleRole role, const ImGuiStyle &style, const StyleColors &style_colors, bool focused);

/// Delegates display-list primitives to ImGui, clipping labels to their column.
void draw_display_list(ImDrawList &draw_list, const timeline::DisplayList &display_list, ImVec2 origin,
    const ImGuiStyle &style, bool focused, int label_width);

/// Delegates display-list primitives to ImGui, clipping labels to their column.
void draw_display_list(ImDrawList &draw_list, const timeline::DisplayList &display_list, ImVec2 origin,
    const ImGuiStyle &style, const StyleColors &style_colors, bool focused, int label_width);

} // namespace timeline_imgui
