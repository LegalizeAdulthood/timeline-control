// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/DisplayList.h>

#include <wx/dc.h>
#include <wx/gdicmn.h>

/// Native theme colors used to interpret toolkit-neutral semantic roles.
struct wxTimelinePalette
{
    wxColour background;
    wxColour foreground;
    wxColour highlight;
};

/// Maps a semantic role to a readable native color for the supplied theme.
wxColour timeline_style_colour(timeline::StyleRole style, const wxTimelinePalette &palette, bool focused);

void draw_timeline_display_list(wxDC &dc, const timeline::DisplayList &display_list, wxPoint origin);
void draw_timeline_display_list(wxDC &dc, const timeline::DisplayList &display_list, wxPoint origin,
    const wxTimelinePalette &palette, int stroke_width, bool focused);

/// Draws one native primitive, preserving the display list's original ordering.
void draw_timeline_primitive(wxDC &dc, const timeline::Primitive &primitive, wxPoint origin,
    const wxTimelinePalette &palette, int stroke_width, bool focused);
