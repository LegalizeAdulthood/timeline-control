#include <wxTimeline/wxTimelineControl.h>

#include <wx/dcbuffer.h>

#include <utility>

wxTimelineControl::wxTimelineControl(wxWindow *parent, wxWindowID id) :
    wxPanel(parent, id)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT, &wxTimelineControl::on_paint, this);
}

void wxTimelineControl::set_document(timeline::Document document)
{
    m_document = std::move(document);
    Refresh(false);
}

void wxTimelineControl::on_paint(wxPaintEvent &)
{
    auto dc = wxAutoBufferedPaintDC(this);
    dc.SetBackground(wxBrush(GetBackgroundColour()));
    dc.Clear();
    dc.SetTextForeground(GetForegroundColour());

    auto position = wxPoint(12, 12);
    const auto draw_line = [&dc, &position](const wxString &text)
    {
        dc.DrawText(text, position);
        position.y += dc.GetCharHeight() + 4;
    };

    if (!m_document)
    {
        draw_line("No timeline loaded.");
        return;
    }

    auto title = wxString::FromUTF8(m_document->metadata().title().c_str());
    if (title.empty())
    {
        title = "Untitled timeline";
    }

    draw_line("Title: " + title);
    draw_line(m_document->is_valid() ? "Valid: yes" : "Valid: no");
    draw_line(
        wxString::Format("Ticks per second: %lld", static_cast<long long>(m_document->timebase().ticks_per_second())));
    if (m_document->frame_grid())
    {
        const auto &frame_grid = *m_document->frame_grid();
        draw_line(wxString::Format("Frames: %lld", static_cast<long long>(frame_grid.frame_count())));
        draw_line(wxString::Format("Frame rate: %lld/%lld fps",
            static_cast<long long>(frame_grid.frames_per_second_numerator()),
            static_cast<long long>(frame_grid.frames_per_second_denominator())));
    }
    if (m_document->source_summary())
    {
        const auto &summary = *m_document->source_summary();
        draw_line("Schema: " + wxString::FromUTF8(summary.schema().c_str()) +
            wxString::Format(" v%llu", static_cast<unsigned long long>(summary.schema_version())));
        draw_line(wxString::Format("Features: %llu", static_cast<unsigned long long>(summary.feature_count())));
        draw_line(wxString::Format("Events: %llu", static_cast<unsigned long long>(summary.event_count())));
        if (summary.first_frame())
        {
            draw_line(wxString::Format("Frame extent: %lld to %lld", static_cast<long long>(*summary.first_frame()),
                static_cast<long long>(*summary.last_frame())));
        }
        if (summary.first_time())
        {
            draw_line(wxString::Format("Time extent: %.6f to %.6f seconds",
                m_document->timebase().seconds(*summary.first_time()),
                m_document->timebase().seconds(*summary.last_time())));
        }
        if (summary.frame_offset())
        {
            draw_line(wxString::Format(
                "Frame offset: %.6f seconds", m_document->timebase().seconds(*summary.frame_offset())));
        }
    }
    draw_line(wxString::Format("Tracks: %llu", static_cast<unsigned long long>(m_document->track_count())));
    draw_line(wxString::Format("Keyframes: %llu", static_cast<unsigned long long>(m_document->keyframe_count())));
    draw_line(wxString::Format("Lanes: %llu", static_cast<unsigned long long>(m_document->lane_count())));
}
