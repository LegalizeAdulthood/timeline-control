// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/Interaction.h>
#include <timeline/Layout.h>
#include <timeline/Query.h>

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

    /// Repaints pending changes and snapshots the displayed timeline primitives.
    std::string snapshot();

    bool has_document() const
    {
        return m_document.has_value();
    }
    const std::optional<timeline::Document> &document() const
    {
        return m_document;
    }
    const std::optional<timeline::FrameInspection> &inspection() const
    {
        return m_inspection;
    }
    const std::optional<timeline::HitResult> &hit_result() const
    {
        return m_hit_result;
    }

    const std::optional<timeline::Interaction> &interaction() const
    {
        return m_interaction;
    }

private:
    void update_interaction();
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

    std::optional<timeline::Document> m_document;
    std::optional<timeline::Interaction> m_interaction;
    std::optional<timeline::FrameInspection> m_inspection;
    std::optional<timeline::HitResult> m_hit_result;
    std::optional<timeline::Point> m_hover_point;
    std::optional<timeline::Layout> m_layout;
    std::optional<timeline::LayoutMetrics> m_layout_metrics;
    std::optional<timeline::Navigation> m_navigation;
    std::optional<timeline::Viewport> m_viewport;
};
