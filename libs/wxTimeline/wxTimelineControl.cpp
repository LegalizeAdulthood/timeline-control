#include <wxTimeline/wxTimelineControl.h>

#include <wx/dcbuffer.h>

#include <utility>

wxTimelineControl::wxTimelineControl(wxWindow *parent, wxWindowID id) :
    wxPanel(parent, id)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT, &wxTimelineControl::on_paint, this);
}

void wxTimelineControl::set_document(timeline::TimelineDocument document)
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
    draw_line(wxString::Format("Tracks: %llu", static_cast<unsigned long long>(m_document->track_count())));
    draw_line(wxString::Format("Keyframes: %llu", static_cast<unsigned long long>(m_document->keyframe_count())));
    draw_line(wxString::Format("Lanes: %llu", static_cast<unsigned long long>(m_document->lane_count())));
}
