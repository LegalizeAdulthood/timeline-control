// Copyright (c) 2026 Richard Thomson

#include <wxTimeline/wxTimelineRenderer.h>

#include <timeline/size_cast.h>

#include <wx/brush.h>
#include <wx/colour.h>
#include <wx/pen.h>

#include <type_traits>
#include <variant>

namespace
{

wxColour style_colour(timeline::StyleRole style)
{
    switch (style)
    {
    case timeline::StyleRole::RULER:
        return wxColour(82, 88, 96);
    case timeline::StyleRole::RULER_LABEL:
        return wxColour(55, 60, 68);
    case timeline::StyleRole::LANE_BACKGROUND:
        return wxColour(235, 238, 242);
    case timeline::StyleRole::LANE_LABEL:
        return wxColour(35, 39, 45);
    case timeline::StyleRole::INSTANT_MARKER:
        return wxColour(196, 58, 48);
    case timeline::StyleRole::INTERVAL_SPAN:
        return wxColour(32, 126, 120);
    case timeline::StyleRole::ENVELOPE_ATTACK:
        return wxColour(210, 145, 35);
    case timeline::StyleRole::ENVELOPE_SUSTAIN:
        return wxColour(67, 132, 78);
    case timeline::StyleRole::ENVELOPE_DECAY:
        return wxColour(66, 100, 166);
    case timeline::StyleRole::CURVE:
        return wxColour(126, 72, 154);
    }
    return wxColour(0, 0, 0);
}

} // namespace

void draw_timeline_display_list(wxDC &dc, const timeline::DisplayList &display_list, wxPoint origin)
{
    for (const auto &primitive : display_list.primitives())
    {
        std::visit(
            [&](const auto &value)
            {
                using Value = std::decay_t<decltype(value)>;
                const auto colour = style_colour(value.style);
                if constexpr (std::is_same_v<Value, timeline::Line>)
                {
                    const auto width = value.style == timeline::StyleRole::INSTANT_MARKER ? 2 : 1;
                    dc.SetPen(wxPen(colour, width));
                    dc.DrawLine(origin.x + value.x1, origin.y + value.y1, origin.x + value.x2, origin.y + value.y2);
                }
                else if constexpr (std::is_same_v<Value, timeline::Rectangle>)
                {
                    dc.SetPen(wxPen(colour));
                    dc.SetBrush(wxBrush(colour));
                    dc.DrawRectangle(origin.x + value.x, origin.y + value.y, value.width, value.height);
                }
                else if constexpr (std::is_same_v<Value, timeline::Polyline>)
                {
                    auto points = std::vector<wxPoint>{};
                    points.reserve(value.points.size());
                    for (const auto &point : value.points)
                    {
                        points.emplace_back(origin.x + point.x, origin.y + point.y);
                    }
                    if (timeline::size_cast(points) >= 2)
                    {
                        dc.SetPen(wxPen(colour, 2));
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
