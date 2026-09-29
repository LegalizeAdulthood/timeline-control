#pragma once

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

    void set_document(timeline::Document document);

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

private:
    void notify_inspection_changed();
    void on_mouse_move(wxMouseEvent &event);
    void on_paint(wxPaintEvent &event);

    std::optional<timeline::Document> m_document;
    std::optional<timeline::FrameInspection> m_inspection;
    std::optional<timeline::LayoutMetrics> m_layout_metrics;
    std::optional<timeline::Viewport> m_viewport;
    int m_layout_top{0};
};
