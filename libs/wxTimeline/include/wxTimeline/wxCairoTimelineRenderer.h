// Copyright (c) 2026 Richard Thomson

#pragma once

#include <wx/font.h>
#include <wx/image.h>
#include <wxTimeline/wxTimelineRenderer.h>

/// Renders a numeric polyline to a transparent, device-scaled Cairo image.
/// Invalid dimensions or rendering failures return an invalid image.
wxImage render_cairo_curve(const timeline::Polyline &curve, wxSize size, wxPoint origin, wxRect clip,
    const wxColour &colour, int stroke_width, double device_scale);

/// Renders ordered primitives with Cairo using native wx font coverage.
/// Invalid dimensions or rendering failures return an invalid image.
wxImage render_cairo_display_list(const timeline::DisplayList &display_list, wxSize size, wxPoint origin, wxRect clip,
    const wxTimelinePalette &palette, int stroke_width, bool focused, const wxFont &font, double device_scale);

/// Presents a complete Cairo display list, falling back to wx on failure.
void draw_cairo_timeline_display_list(wxDC &dc, const timeline::DisplayList &display_list, wxPoint origin,
    const wxTimelinePalette &palette, int stroke_width, bool focused, wxSize size, double device_scale);
