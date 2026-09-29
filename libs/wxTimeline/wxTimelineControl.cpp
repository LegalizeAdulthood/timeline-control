#include <wxTimeline/wxTimelineControl.h>
#include <wxTimeline/wxTimelineRenderer.h>

#include <timeline/Layout.h>
#include <timeline/size_cast.h>

#include <wx/dcbuffer.h>

#include <algorithm>
#include <utility>

wxTimelineControl::wxTimelineControl(wxWindow *parent) :
    wxTimelineControl(parent, wxID_ANY)
{
}

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
            wxString::Format(" v%d", summary.schema_version()));
        draw_line(wxString::Format("Features: %d", summary.feature_count()));
        draw_line(wxString::Format("Events: %d", summary.event_count()));
        if (summary.generation_summary())
        {
            const auto &generation = *summary.generation_summary();
            draw_line("Generator: " + wxString::FromUTF8(generation.generator_name().c_str()) + " " +
                wxString::FromUTF8(generation.generator_version().c_str()));
            draw_line(wxString::Format("Inputs: %d", timeline::size_cast(generation.source_references())));
            draw_line(wxString::Format("Generated target groups: %d", timeline::size_cast(generation.target_counts())));
            draw_line(wxString::Format("Music source groups: %d", timeline::size_cast(generation.source_counts())));
        }
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
    draw_line(wxString::Format("Tracks: %d", m_document->track_count()));
    draw_line(wxString::Format("Keyframes: %d", m_document->keyframe_count()));
    draw_line(wxString::Format("Lanes: %d", m_document->lane_count()));

    const auto content_start = m_document->content_start();
    const auto content_end = m_document->content_end();
    if (!content_start || !content_end || *content_end <= *content_start)
    {
        return;
    }

    const auto client_size = GetClientSize();
    const auto layout_top = position.y + 8;
    const auto layout_height = client_size.GetHeight() - layout_top - 8;
    if (client_size.GetWidth() <= 160 || layout_height <= 40)
    {
        return;
    }

    auto label_width = 80;
    for (const auto &lane : m_document->lanes())
    {
        label_width = std::max(label_width, dc.GetTextExtent(wxString::FromUTF8(lane.label().c_str())).GetWidth() + 16);
    }
    label_width = std::min(label_width, client_size.GetWidth() / 2);
    const auto metrics = timeline::LayoutMetrics(label_width, dc.GetCharHeight() + 8, dc.GetCharHeight() + 16, 4);
    const auto viewport = timeline::Viewport(client_size.GetWidth(), layout_height, *content_start, *content_end);
    const auto layout = timeline::Layout(*m_document, viewport, metrics);
    draw_timeline_display_list(dc, layout.display_list(), wxPoint(0, layout_top));
}
