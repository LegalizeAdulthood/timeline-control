// Copyright (c) 2026 Richard Thomson

#include <wxTimeline/wxTimelineRenderer.h>

#include <timeline/size_cast.h>

#include <wx/brush.h>
#include <wx/colour.h>
#include <wx/pen.h>
#include <wx/settings.h>

#include <algorithm>
#include <type_traits>
#include <variant>

namespace
{

wxColour mix(const wxColour &first, const wxColour &second, int weight)
{
    return wxColour((first.Red() * weight + second.Red() * (100 - weight)) / 100,
        (first.Green() * weight + second.Green() * (100 - weight)) / 100,
        (first.Blue() * weight + second.Blue() * (100 - weight)) / 100);
}

} // namespace

wxColour timeline_style_colour(timeline::StyleRole style, const wxTimelinePalette &palette, bool focused)
{
    const bool dark = palette.background.GetLuminance() < 0.5;
    switch (style)
    {
    case timeline::StyleRole::RULER:
        return mix(palette.foreground, palette.background, 60);
    case timeline::StyleRole::RULER_LABEL:
        return palette.foreground;
    case timeline::StyleRole::LANE_BACKGROUND:
        return mix(palette.foreground, palette.background, 6);
    case timeline::StyleRole::LANE_LABEL:
        return palette.foreground;
    case timeline::StyleRole::INSTANT_MARKER:
        return dark ? wxColour(240, 110, 100) : wxColour(196, 58, 48);
    case timeline::StyleRole::INTERVAL_SPAN:
        return dark ? wxColour(80, 190, 180) : wxColour(32, 126, 120);
    case timeline::StyleRole::ENVELOPE_ATTACK:
        return dark ? wxColour(235, 190, 80) : wxColour(170, 110, 25);
    case timeline::StyleRole::ENVELOPE_SUSTAIN:
        return dark ? wxColour(115, 195, 130) : wxColour(67, 132, 78);
    case timeline::StyleRole::ENVELOPE_DECAY:
        return dark ? wxColour(115, 160, 230) : wxColour(66, 100, 166);
    case timeline::StyleRole::CURVE:
        return dark ? wxColour(195, 135, 220) : wxColour(126, 72, 154);
    case timeline::StyleRole::KEYFRAME_SEGMENT:
        return dark ? wxColour(100, 160, 230) : wxColour(47, 95, 164);
    case timeline::StyleRole::KEYFRAME_MARKER:
        return dark ? wxColour(245, 185, 90) : wxColour(190, 105, 20);
    case timeline::StyleRole::SELECTED_LANE:
        return mix(focused ? palette.highlight : palette.foreground, palette.background, 25);
    case timeline::StyleRole::SELECTED_ITEM:
        return focused ? palette.highlight : mix(palette.foreground, palette.background, 65);
    case timeline::StyleRole::SELECTED_RANGE:
        return mix(focused ? palette.highlight : palette.foreground, palette.background, 18);
    case timeline::StyleRole::PLAYHEAD:
        return dark ? wxColour(245, 100, 120) : wxColour(185, 35, 55);
    }
    return palette.foreground;
}

void draw_timeline_display_list(wxDC &dc, const timeline::DisplayList &display_list, wxPoint origin)
{
    const wxTimelinePalette palette{
        dc.GetBackground().GetColour(), dc.GetTextForeground(), wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)};
    draw_timeline_display_list(dc, display_list, origin, palette, 1, true);
}

void draw_timeline_display_list(wxDC &dc, const timeline::DisplayList &display_list, wxPoint origin,
    const wxTimelinePalette &palette, int stroke_width, bool focused)
{
    stroke_width = std::max(1, stroke_width);
    for (const timeline::Primitive &primitive : display_list.primitives())
    {
        std::visit(
            [&](const auto &value)
            {
                using Value = std::decay_t<decltype(value)>;
                const wxColour colour = timeline_style_colour(value.style, palette, focused);
                if constexpr (std::is_same_v<Value, timeline::Line>)
                {
                    dc.SetPen(wxPen(colour, stroke_width));
                    dc.DrawLine(origin.x + value.x1, origin.y + value.y1, origin.x + value.x2, origin.y + value.y2);
                }
                else if constexpr (std::is_same_v<Value, timeline::Rectangle> ||
                    std::is_same_v<Value, timeline::Marker>)
                {
                    dc.SetPen(*wxTRANSPARENT_PEN);
                    dc.SetBrush(wxBrush(colour));
                    dc.DrawRectangle(origin.x + value.x, origin.y + value.y, value.width, value.height);
                }
                else if constexpr (std::is_same_v<Value, timeline::Polyline>)
                {
                    std::vector<wxPoint> points{};
                    points.reserve(value.points.size());
                    for (const timeline::Point &point : value.points)
                    {
                        points.emplace_back(origin.x + point.x, origin.y + point.y);
                    }
                    if (timeline::size_cast(points) >= 2)
                    {
                        dc.SetPen(wxPen(colour, 2 * stroke_width));
                        dc.DrawLines(timeline::size_cast(points), points.data());
                    }
                }
                else
                {
                    dc.SetTextForeground(colour);
                    dc.DrawText(wxString::FromUTF8(value.value.c_str()), origin.x + value.x, origin.y + value.y);
                }
            },
            primitive);
    }
}
