// Copyright (c) 2026 Richard Thomson

#include <wxTimeline/wxTimelineRenderer.h>

#include <timeline/size_cast.h>

#include <wx/brush.h>
#include <wx/colour.h>
#include <wx/pen.h>
#include <wx/settings.h>

#include <algorithm>
#include <string_view>
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
    return timeline_style_colour(style, palette, wxTimelineStyleColors{}, focused);
}

wxColour timeline_style_colour(timeline::StyleRole style, const wxTimelinePalette &palette,
    const wxTimelineStyleColors &style_colors, bool focused)
{
    const wxTimelineStyleColors::const_iterator found = style_colors.find(style);
    if (found != style_colors.end())
    {
        return found->second;
    }
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
        return palette.highlight;
    case timeline::StyleRole::INTERVAL_SPAN:
        return mix(palette.highlight, palette.background, 55);
    case timeline::StyleRole::ENVELOPE_ATTACK:
        return mix(palette.highlight, palette.foreground, 70);
    case timeline::StyleRole::ENVELOPE_SUSTAIN:
        return mix(palette.highlight, palette.background, 75);
    case timeline::StyleRole::ENVELOPE_DECAY:
        return mix(palette.foreground, palette.background, 75);
    case timeline::StyleRole::CURVE:
        return palette.foreground;
    case timeline::StyleRole::PALETTE:
        return palette.foreground;
    case timeline::StyleRole::KEYFRAME_SEGMENT:
        return mix(palette.foreground, palette.background, 75);
    case timeline::StyleRole::KEYFRAME_MARKER:
        return palette.highlight;
    case timeline::StyleRole::SELECTED_LANE:
        return mix(focused ? palette.highlight : palette.foreground, palette.background, 25);
    case timeline::StyleRole::SELECTED_ITEM:
        return focused ? palette.highlight : mix(palette.foreground, palette.background, 65);
    case timeline::StyleRole::SELECTED_RANGE:
        return mix(focused ? palette.highlight : palette.foreground, palette.background, 18);
    case timeline::StyleRole::PLAYHEAD:
        return mix(palette.highlight, palette.foreground, 80);
    }
    return palette.foreground;
}

void draw_timeline_display_list(wxDC &dc, const timeline::DisplayList &display_list, wxPoint origin)
{
    const wxTimelinePalette palette{
        dc.GetBackground().GetColour(), dc.GetTextForeground(), wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)};
    draw_timeline_display_list(dc, display_list, origin, palette, wxTimelineStyleColors{}, 1, true);
}

void draw_timeline_display_list(wxDC &dc, const timeline::DisplayList &display_list, wxPoint origin,
    const wxTimelinePalette &palette, int stroke_width, bool focused)
{
    draw_timeline_display_list(dc, display_list, origin, palette, wxTimelineStyleColors{}, stroke_width, focused);
}

void draw_timeline_display_list(wxDC &dc, const timeline::DisplayList &display_list, wxPoint origin,
    const wxTimelinePalette &palette, const wxTimelineStyleColors &style_colors, int stroke_width, bool focused)
{
    stroke_width = std::max(1, stroke_width);
    for (const timeline::Primitive &primitive : display_list.primitives())
    {
        draw_timeline_primitive(
            dc, primitive, origin, display_list.strings(), palette, style_colors, stroke_width, focused);
    }
}

void draw_timeline_primitive(wxDC &dc, const timeline::Primitive &primitive, wxPoint origin,
    const timeline::StringTable &strings, const wxTimelinePalette &palette, int stroke_width, bool focused)
{
    draw_timeline_primitive(dc, primitive, origin, strings, palette, wxTimelineStyleColors{}, stroke_width, focused);
}

void draw_timeline_primitive(wxDC &dc, const timeline::Primitive &primitive, wxPoint origin,
    const timeline::StringTable &strings, const wxTimelinePalette &palette, const wxTimelineStyleColors &style_colors,
    int stroke_width, bool focused)
{
    stroke_width = std::max(1, stroke_width);
    std::visit(
        [&](const auto &value)
        {
            using Value = std::decay_t<decltype(value)>;
            const wxColour colour = timeline_style_colour(value.style, palette, style_colors, focused);
            const bool overridden = style_colors.find(value.style) != style_colors.end();
            if constexpr (std::is_same_v<Value, timeline::Line>)
            {
                dc.SetPen(wxPen(colour, stroke_width));
                dc.DrawLine(origin.x + value.x1, origin.y + value.y1, origin.x + value.x2, origin.y + value.y2);
            }
            else if constexpr (std::is_same_v<Value, timeline::Rectangle> || std::is_same_v<Value, timeline::Marker>)
            {
                dc.SetPen(*wxTRANSPARENT_PEN);
                dc.SetBrush(wxBrush(colour));
                dc.DrawRectangle(origin.x + value.x, origin.y + value.y, value.width, value.height);
            }
            else if constexpr (std::is_same_v<Value, timeline::Swatch>)
            {
                dc.SetPen(*wxTRANSPARENT_PEN);
                const wxColour fill = overridden ? colour
                                                 : wxColour(static_cast<unsigned char>(value.color.red()),
                                                       static_cast<unsigned char>(value.color.green()),
                                                       static_cast<unsigned char>(value.color.blue()));
                dc.SetBrush(wxBrush(fill));
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
                const std::string_view text = strings.lookup(value.value);
                dc.DrawText(wxString(text.data(), text.size()), origin.x + value.x, origin.y + value.y);
            }
        },
        primitive);
}
