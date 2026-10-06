// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/DisplayList.h>

#include <wx/dc.h>
#include <wx/gdicmn.h>

#include <map>

/// Native theme colors used to interpret toolkit-neutral semantic roles.
///
struct wxTimelinePalette
{
    wxColour background;
    wxColour foreground;
    wxColour highlight;
};

/// Application-selected native colors for semantic style roles.
using wxTimelineStyleColors = std::map<timeline::StyleRole, wxColour>;

/// Maps a semantic role to native control and system colors.
wxColour timeline_style_colour(timeline::StyleRole style, const wxTimelinePalette &palette, bool focused);

/// Returns an application override or the native default for a style role.
wxColour timeline_style_colour(timeline::StyleRole style, const wxTimelinePalette &palette,
    const wxTimelineStyleColors &style_colors, bool focused);

void draw_timeline_display_list(wxDC &dc, const timeline::DisplayList &display_list, wxPoint origin);

void draw_timeline_display_list(wxDC &dc, const timeline::DisplayList &display_list, wxPoint origin,
    const wxTimelinePalette &palette, int stroke_width, bool focused);

void draw_timeline_display_list(wxDC &dc, const timeline::DisplayList &display_list, wxPoint origin,
    const wxTimelinePalette &palette, const wxTimelineStyleColors &style_colors, int stroke_width, bool focused);
