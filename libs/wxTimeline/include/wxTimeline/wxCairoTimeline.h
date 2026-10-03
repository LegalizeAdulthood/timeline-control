// Copyright (c) 2026 Richard Thomson

#pragma once

#include <wxTimeline/wxTimelineControl.h>

/// Native wx timeline with optional antialiased Cairo display-list rendering.
///
/// Changing renderer preserves the base control's owned document and all
/// navigation, interaction, and inspection state.
///
class wxCairoTimeline : public wxTimelineControl
{
public:
    explicit wxCairoTimeline(wxWindow *parent);
    wxCairoTimeline(wxWindow *parent, wxWindowID id);
    void set_cairo_enabled(bool enabled);
    bool cairo_enabled() const
    {
        return m_cairo_enabled;
    }

private:
    void draw_display_list(wxDC &dc, const timeline::DisplayList &display_list, const wxTimelinePalette &palette,
        int stroke_width, bool focused) override;
    bool m_cairo_enabled{true};
};
