// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/DisplayList.h>

#include <imgui.h>

namespace timeline_imgui
{

/// Maps a semantic drawing role to the supplied ImGui theme and focus state.
ImU32 style_colour(timeline::StyleRole role, const ImGuiStyle &style, bool focused);

/// Delegates display-list primitives to ImGui, clipping labels to their column.
void draw_display_list(ImDrawList &draw_list, const timeline::DisplayList &display_list, ImVec2 origin,
    const ImGuiStyle &style, bool focused, int label_width);

} // namespace timeline_imgui
