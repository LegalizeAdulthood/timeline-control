// Copyright (c) 2026 Richard Thomson

#include <imguiTimeline/TimelineRenderer.h>

#include <timeline/size_cast.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <type_traits>

namespace timeline_imgui
{

namespace
{

ImVec2 screen_point(ImVec2 origin, int x, int y)
{
    return ImVec2(origin.x + static_cast<float>(x), origin.y + static_cast<float>(y));
}

ImVec4 mix(const ImVec4 &first, const ImVec4 &second, float weight)
{
    return ImVec4(first.x * weight + second.x * (1.0F - weight), first.y * weight + second.y * (1.0F - weight),
        first.z * weight + second.z * (1.0F - weight), 1.0F);
}

} // namespace

ImU32 style_colour(timeline::StyleRole role, const ImGuiStyle &style, bool focused)
{
    const ImVec4 &foreground = style.Colors[ImGuiCol_Text];
    const ImVec4 &background = style.Colors[ImGuiCol_WindowBg];
    const ImVec4 &selection = focused ? style.Colors[ImGuiCol_HeaderActive] : foreground;
    ImVec4 colour = foreground;
    switch (role)
    {
    case timeline::StyleRole::RULER:
        colour = mix(foreground, background, 0.6F);
        break;
    case timeline::StyleRole::LANE_BACKGROUND:
        colour = mix(foreground, background, 0.06F);
        break;
    case timeline::StyleRole::INSTANT_MARKER:
    case timeline::StyleRole::KEYFRAME_MARKER:
    case timeline::StyleRole::ENVELOPE_ATTACK:
        colour = style.Colors[ImGuiCol_PlotHistogram];
        break;
    case timeline::StyleRole::INTERVAL_SPAN:
        colour = style.Colors[ImGuiCol_Header];
        break;
    case timeline::StyleRole::ENVELOPE_SUSTAIN:
        colour = style.Colors[ImGuiCol_CheckMark];
        break;
    case timeline::StyleRole::CURVE:
    case timeline::StyleRole::ENVELOPE_DECAY:
        colour = style.Colors[ImGuiCol_PlotLines];
        break;
    case timeline::StyleRole::KEYFRAME_SEGMENT:
        colour = style.Colors[ImGuiCol_SliderGrab];
        break;
    case timeline::StyleRole::SELECTED_LANE:
        colour = mix(selection, background, 0.25F);
        break;
    case timeline::StyleRole::SELECTED_ITEM:
        colour = focused ? selection : mix(foreground, background, 0.65F);
        break;
    case timeline::StyleRole::SELECTED_RANGE:
        colour = mix(selection, background, 0.18F);
        break;
    case timeline::StyleRole::PLAYHEAD:
        colour = style.Colors[ImGuiCol_NavCursor];
        break;
    case timeline::StyleRole::RULER_LABEL:
    case timeline::StyleRole::LANE_LABEL:
    case timeline::StyleRole::PALETTE:
        break;
    }
    colour.w *= style.Alpha;
    return ImGui::ColorConvertFloat4ToU32(colour);
}

void draw_display_list(ImDrawList &draw_list, const timeline::DisplayList &display_list, ImVec2 origin,
    const ImGuiStyle &style, bool focused, int label_width)
{
    const float stroke_width = std::max(1.0F, ImGui::GetFontSize() / 13.0F);
    for (const timeline::Primitive &primitive : display_list.primitives())
    {
        std::visit(
            [&](const auto &value)
            {
                using Value = std::decay_t<decltype(value)>;
                const ImU32 colour = style_colour(value.style, style, focused);
                if constexpr (std::is_same_v<Value, timeline::Line>)
                {
                    draw_list.AddLine(screen_point(origin, value.x1, value.y1),
                        screen_point(origin, value.x2, value.y2), colour, stroke_width);
                }
                else if constexpr (std::is_same_v<Value, timeline::Polyline>)
                {
                    std::vector<ImVec2> points;
                    points.reserve(value.points.size());
                    for (const timeline::Point &point : value.points)
                    {
                        points.push_back(screen_point(origin, point.x, point.y));
                    }
                    if (timeline::size_cast(points) >= 2)
                    {
                        draw_list.AddPolyline(
                            points.data(), timeline::size_cast(points), colour, 0, 2.0F * stroke_width);
                    }
                }
                else if constexpr (std::is_same_v<Value, timeline::Text>)
                {
                    const ImVec4 clip(origin.x, -FLT_MAX, origin.x + static_cast<float>(label_width), FLT_MAX);
                    draw_list.AddText(ImGui::GetFont(), ImGui::GetFontSize(), screen_point(origin, value.x, value.y),
                        colour, value.value.data(), value.value.data() + value.value.size(), 0.0F,
                        value.style == timeline::StyleRole::LANE_LABEL ? &clip : nullptr);
                }
                else
                {
                    ImU32 fill = colour;
                    if constexpr (std::is_same_v<Value, timeline::Swatch>)
                    {
                        fill = IM_COL32(value.color.red(), value.color.green(), value.color.blue(),
                            static_cast<int>(std::lround(255.0F * style.Alpha)));
                    }
                    draw_list.AddRectFilled(screen_point(origin, value.x, value.y),
                        screen_point(origin, value.x + value.width, value.y + value.height), fill);
                }
            },
            primitive);
    }
}

} // namespace timeline_imgui
