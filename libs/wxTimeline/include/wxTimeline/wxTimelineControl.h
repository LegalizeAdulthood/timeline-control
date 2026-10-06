// Copyright (c) 2026 Richard Thomson

#pragma once

#include <wxTimeline/wxTimelineRenderer.h>

#include <timeline/ControlState.h>

#include <wx/panel.h>

#include <optional>

wxDECLARE_EVENT(wxEVT_TIMELINE_INSPECTION_CHANGED, wxCommandEvent);

/// Native wxWidgets view of a toolkit-neutral timeline document.
///
/// The control owns the document it displays and delegates timeline semantics
/// to timeline-core.
///
class wxTimelineControl : public wxPanel
{
public:
    explicit wxTimelineControl(wxWindow *parent);
    wxTimelineControl(wxWindow *parent, wxWindowID id);

    bool AcceptsFocus() const override
    {
        return true;
    }

    void set_document(timeline::Document document);
    void zoom_in();
    void zoom_out();
    void fit_view();
    void clear_selection();
    void step_playhead(int frames, bool extend_selection);

    /// Repaints pending changes and snapshots the displayed timeline primitives.
    std::string snapshot();

    bool has_document() const
    {
        return m_state.document().has_value();
    }
    const std::optional<timeline::Document> &document() const
    {
        return m_state.document();
    }
    const std::optional<timeline::FrameInspection> &inspection() const
    {
        return m_state.inspection();
    }
    const std::optional<timeline::HitResult> &hit_result() const
    {
        return m_state.hit_result();
    }

    const std::optional<timeline::Interaction> &interaction() const
    {
        return m_state.interaction();
    }

protected:
    /// Presentation hook; layout and interaction remain owned by this control.
    virtual void draw_display_list(wxDC &dc, const timeline::DisplayList &display_list,
        const wxTimelinePalette &palette, int stroke_width, bool focused)
    {
        draw_timeline_display_list(dc, display_list, wxPoint(0, 0), palette, stroke_width, focused);
    }

private:
    void rebuild_layout(wxDC &dc);
    void update_control();
    void on_mouse_down(wxMouseEvent &event);
    void on_mouse_up(wxMouseEvent &event);
    void on_capture_lost(wxMouseCaptureLostEvent &event);
    void on_key_down(wxKeyEvent &event);
    void clear_hit();
    void notify_inspection_changed();
    void on_mouse_leave(wxMouseEvent &event);
    void on_mouse_move(wxMouseEvent &event);
    void on_mouse_wheel(wxMouseEvent &event);
    void on_paint(wxPaintEvent &event);
    void on_resize(wxSizeEvent &event);
    void on_dpi_changed(wxDPIChangedEvent &event);
    void on_system_colour_changed(wxSysColourChangedEvent &event);
    void on_focus(wxFocusEvent &event);
    void invalidate_layout();
    void on_scroll(wxScrollWinEvent &event);
    void update_scrollbars();
    void zoom_by(double factor);

    timeline::ControlState m_state;
    std::optional<timeline::Point> m_hover_point;
};
