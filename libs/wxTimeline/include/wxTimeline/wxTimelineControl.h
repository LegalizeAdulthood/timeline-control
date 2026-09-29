#pragma once

#include <timeline/Document.h>

#include <wx/panel.h>

#include <optional>

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

private:
    void on_paint(wxPaintEvent &event);

    std::optional<timeline::Document> m_document;
};
