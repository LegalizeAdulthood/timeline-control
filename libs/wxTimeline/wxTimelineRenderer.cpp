// Copyright (c) 2026 Richard Thomson

#include <wxTimeline/wxTimelineRenderer.h>

#include <timeline/DisplayListRenderer.h>
#include <timeline/size_cast.h>

#include <wx/brush.h>
#include <wx/colour.h>
#include <wx/pen.h>
#include <wx/settings.h>

#include <algorithm>
#include <string_view>
#include <vector>

namespace
{

wxColour mix(const wxColour &first, const wxColour &second, int weight)
{
    return wxColour((first.Red() * weight + second.Red() * (100 - weight)) / 100,
        (first.Green() * weight + second.Green() * (100 - weight)) / 100,
        (first.Blue() * weight + second.Blue() * (100 - weight)) / 100);
}

/// Adapts toolkit-neutral display-list operations to a native wxDC.
///
class WxDisplayListRenderer final : public timeline::DisplayListRenderer
{
public:
    WxDisplayListRenderer(wxDC &dc, wxPoint origin, const wxTimelinePalette &palette,
        const wxTimelineStyleColors &style_colors, int stroke_width, bool focused);

    void draw_line(const timeline::Line &line) override;
    void fill_rectangle(const timeline::Rectangle &rectangle) override;
    void draw_text(const timeline::Text &text, std::string_view value) override;
    void draw_marker(const timeline::Marker &marker) override;
    void draw_polyline(const timeline::Polyline &polyline) override;
    void draw_swatch(const timeline::Swatch &swatch) override;

private:
    wxColour colour(timeline::StyleRole style) const
    {
        return timeline_style_colour(style, m_palette, m_style_colors, m_focused);
    }

    wxDC &m_dc;
    wxPoint m_origin;
    const wxTimelinePalette &m_palette;
    const wxTimelineStyleColors &m_style_colors;
    int m_stroke_width;
    bool m_focused;
};

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

namespace
{

WxDisplayListRenderer::WxDisplayListRenderer(wxDC &dc, wxPoint origin, const wxTimelinePalette &palette,
    const wxTimelineStyleColors &style_colors, int stroke_width, bool focused) :
    m_dc(dc),
    m_origin(origin),
    m_palette(palette),
    m_style_colors(style_colors),
    m_stroke_width(std::max(1, stroke_width)),
    m_focused(focused)
{
}

void WxDisplayListRenderer::draw_line(const timeline::Line &line)
{
    m_dc.SetPen(wxPen(colour(line.style), m_stroke_width));
    m_dc.DrawLine(m_origin.x + line.x1, m_origin.y + line.y1, m_origin.x + line.x2, m_origin.y + line.y2);
}

void WxDisplayListRenderer::fill_rectangle(const timeline::Rectangle &rectangle)
{
    m_dc.SetPen(*wxTRANSPARENT_PEN);
    m_dc.SetBrush(wxBrush(colour(rectangle.style)));
    m_dc.DrawRectangle(m_origin.x + rectangle.x, m_origin.y + rectangle.y, rectangle.width, rectangle.height);
}

void WxDisplayListRenderer::draw_text(const timeline::Text &text, std::string_view value)
{
    m_dc.SetTextForeground(colour(text.style));
    m_dc.DrawText(wxString(value.data(), value.size()), m_origin.x + text.x, m_origin.y + text.y);
}

void WxDisplayListRenderer::draw_marker(const timeline::Marker &marker)
{
    m_dc.SetPen(*wxTRANSPARENT_PEN);
    m_dc.SetBrush(wxBrush(colour(marker.style)));
    m_dc.DrawRectangle(m_origin.x + marker.x, m_origin.y + marker.y, marker.width, marker.height);
}

void WxDisplayListRenderer::draw_polyline(const timeline::Polyline &polyline)
{
    std::vector<wxPoint> points;
    points.reserve(polyline.points.size());
    for (const timeline::Point &point : polyline.points)
    {
        points.emplace_back(m_origin.x + point.x, m_origin.y + point.y);
    }
    if (timeline::size_cast(points) >= 2)
    {
        m_dc.SetPen(wxPen(colour(polyline.style), 2 * m_stroke_width));
        m_dc.DrawLines(timeline::size_cast(points), points.data());
    }
}

void WxDisplayListRenderer::draw_swatch(const timeline::Swatch &swatch)
{
    m_dc.SetPen(*wxTRANSPARENT_PEN);
    wxColour fill = colour(swatch.style);
    if (m_style_colors.find(swatch.style) == m_style_colors.end())
    {
        fill = wxColour(static_cast<unsigned char>(swatch.color.red()),
            static_cast<unsigned char>(swatch.color.green()), static_cast<unsigned char>(swatch.color.blue()));
    }
    m_dc.SetBrush(wxBrush(fill));
    m_dc.DrawRectangle(m_origin.x + swatch.x, m_origin.y + swatch.y, swatch.width, swatch.height);
}

} // namespace

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
    WxDisplayListRenderer renderer(dc, origin, palette, style_colors, stroke_width, focused);
    timeline::render_display_list(renderer, display_list);
}
