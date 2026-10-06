// Copyright (c) 2026 Richard Thomson

#include <wxTimeline/wxCairoTimeline.h>
#include <wxTimeline/wxCairoTimelineRenderer.h>

wxCairoTimeline::wxCairoTimeline(wxWindow *parent) :
    wxCairoTimeline(parent, wxID_ANY)
{
}

wxCairoTimeline::wxCairoTimeline(wxWindow *parent, wxWindowID id) :
    wxTimelineControl(parent, id)
{
}

void wxCairoTimeline::set_cairo_enabled(bool enabled)
{
    if (m_cairo_enabled != enabled)
    {
        m_cairo_enabled = enabled;
        Refresh(false);
    }
}

void wxCairoTimeline::draw_display_list(wxDC &dc, const timeline::DisplayList &display_list,
    const wxTimelinePalette &palette, const wxTimelineStyleColors &style_colors, int stroke_width, bool focused)
{
    if (m_cairo_enabled)
    {
        draw_cairo_timeline_display_list(dc, display_list, wxPoint(0, 0), palette, style_colors, stroke_width, focused,
            GetClientSize(), GetContentScaleFactor());
    }
    else
    {
        wxTimelineControl::draw_display_list(dc, display_list, palette, style_colors, stroke_width, focused);
    }
}
