// Copyright (c) 2026 Richard Thomson

#include <imguiTimeline/TimelineRenderer.h>

#include <timeline/DisplayListRenderer.h>
#include <timeline/size_cast.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string_view>
#include <vector>

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

/// Adapts toolkit-neutral display-list operations to an ImGui draw list.
///
class ImGuiDisplayListRenderer final : public timeline::DisplayListRenderer
{
public:
    ImGuiDisplayListRenderer(ImDrawList &draw_list, ImVec2 origin, const ImGuiStyle &style,
        const StyleColors &style_colors, bool focused, int label_width);

    void draw_line(const timeline::Line &line) override;
    void fill_rectangle(const timeline::Rectangle &rectangle) override;
    void draw_text(const timeline::Text &text, std::string_view value) override;
    void draw_marker(const timeline::Marker &marker) override;
    void draw_polyline(const timeline::Polyline &polyline) override;
    void draw_swatch(const timeline::Swatch &swatch) override;

private:
    ImU32 colour(timeline::StyleRole role) const
    {
        return style_colour(role, m_style, m_style_colors, m_focused);
    }

    ImDrawList &m_draw_list;
    ImVec2 m_origin;
    const ImGuiStyle &m_style;
    const StyleColors &m_style_colors;
    bool m_focused;
    int m_label_width;
    float m_stroke_width;
};

} // namespace

ImU32 style_colour(timeline::StyleRole role, const ImGuiStyle &style, bool focused)
{
    return style_colour(role, style, StyleColors{}, focused);
}

ImU32 style_colour(timeline::StyleRole role, const ImGuiStyle &style, const StyleColors &style_colors, bool focused)
{
    const StyleColors::const_iterator found = style_colors.find(role);
    if (found != style_colors.end())
    {
        return found->second;
    }
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

namespace
{

ImGuiDisplayListRenderer::ImGuiDisplayListRenderer(ImDrawList &draw_list, ImVec2 origin, const ImGuiStyle &style,
    const StyleColors &style_colors, bool focused, int label_width) :
    m_draw_list(draw_list),
    m_origin(origin),
    m_style(style),
    m_style_colors(style_colors),
    m_focused(focused),
    m_label_width(label_width),
    m_stroke_width(std::max(1.0F, ImGui::GetFontSize() / 13.0F))
{
}

void ImGuiDisplayListRenderer::draw_line(const timeline::Line &line)
{
    m_draw_list.AddLine(screen_point(m_origin, line.x1, line.y1), screen_point(m_origin, line.x2, line.y2),
        colour(line.style), m_stroke_width);
}

void ImGuiDisplayListRenderer::fill_rectangle(const timeline::Rectangle &rectangle)
{
    m_draw_list.AddRectFilled(screen_point(m_origin, rectangle.x, rectangle.y),
        screen_point(m_origin, rectangle.x + rectangle.width, rectangle.y + rectangle.height), colour(rectangle.style));
}

void ImGuiDisplayListRenderer::draw_text(const timeline::Text &text, std::string_view value)
{
    const ImVec4 clip(m_origin.x, -FLT_MAX, m_origin.x + static_cast<float>(m_label_width), FLT_MAX);
    m_draw_list.AddText(ImGui::GetFont(), ImGui::GetFontSize(), screen_point(m_origin, text.x, text.y),
        colour(text.style), value.data(), value.data() + value.size(), 0.0F,
        text.style == timeline::StyleRole::LANE_LABEL ? &clip : nullptr);
}

void ImGuiDisplayListRenderer::draw_marker(const timeline::Marker &marker)
{
    m_draw_list.AddRectFilled(screen_point(m_origin, marker.x, marker.y),
        screen_point(m_origin, marker.x + marker.width, marker.y + marker.height), colour(marker.style));
}

void ImGuiDisplayListRenderer::draw_polyline(const timeline::Polyline &polyline)
{
    std::vector<ImVec2> points;
    points.reserve(polyline.points.size());
    for (const timeline::Point &point : polyline.points)
    {
        points.push_back(screen_point(m_origin, point.x, point.y));
    }
    if (timeline::size_cast(points) >= 2)
    {
        m_draw_list.AddPolyline(
            points.data(), timeline::size_cast(points), colour(polyline.style), 0, 2.0F * m_stroke_width);
    }
}

void ImGuiDisplayListRenderer::draw_swatch(const timeline::Swatch &swatch)
{
    ImU32 fill = colour(swatch.style);
    if (m_style_colors.find(swatch.style) == m_style_colors.end())
    {
        fill = IM_COL32(swatch.color.red(), swatch.color.green(), swatch.color.blue(),
            static_cast<int>(std::lround(255.0F * m_style.Alpha)));
    }
    m_draw_list.AddRectFilled(screen_point(m_origin, swatch.x, swatch.y),
        screen_point(m_origin, swatch.x + swatch.width, swatch.y + swatch.height), fill);
}

} // namespace

void draw_display_list(ImDrawList &draw_list, const timeline::DisplayList &display_list, ImVec2 origin,
    const ImGuiStyle &style, bool focused, int label_width)
{
    draw_display_list(draw_list, display_list, origin, style, StyleColors{}, focused, label_width);
}

void draw_display_list(ImDrawList &draw_list, const timeline::DisplayList &display_list, ImVec2 origin,
    const ImGuiStyle &style, const StyleColors &style_colors, bool focused, int label_width)
{
    ImGuiDisplayListRenderer renderer(draw_list, origin, style, style_colors, focused, label_width);
    timeline::render_display_list(renderer, display_list);
}

} // namespace timeline_imgui
