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
    explicit wxTimelineControl(wxWindow *parent, wxWindowID id = wxID_ANY);

    void set_document(timeline::TimelineDocument document);

    bool has_document() const
    {
        return m_document.has_value();
    }
    const timeline::TimelineDocument *document() const
    {
        return m_document ? &*m_document : nullptr;
    }

private:
    void on_paint(wxPaintEvent &event);

    std::optional<timeline::TimelineDocument> m_document;
};
