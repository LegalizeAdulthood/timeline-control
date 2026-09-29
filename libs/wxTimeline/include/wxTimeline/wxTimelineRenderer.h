// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/DisplayList.h>

#include <wx/dc.h>
#include <wx/gdicmn.h>

void draw_timeline_display_list(wxDC &dc, const timeline::DisplayList &display_list, wxPoint origin);
