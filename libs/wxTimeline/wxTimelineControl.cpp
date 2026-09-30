// Copyright (c) 2026 Richard Thomson

#include <wxTimeline/wxTimelineControl.h>
#include <wxTimeline/wxTimelineRenderer.h>

#include <timeline/Layout.h>
#include <timeline/size_cast.h>
#include <timeline/Snapshot.h>

#include <wx/dcbuffer.h>

#include <algorithm>
#include <cmath>
#include <utility>

wxDEFINE_EVENT(wxEVT_TIMELINE_INSPECTION_CHANGED, wxCommandEvent);

namespace
{

constexpr int scroll_range = 10000;
constexpr double zoom_step = 1.25;

int scroll_event_position(const wxScrollWinEvent &event, const wxWindow &window)
{
    const int orientation = event.GetOrientation();
    const int range = window.GetScrollRange(orientation);
    const int thumb = window.GetScrollThumb(orientation);
    const int maximum = std::max(0, range - thumb);
    const int position = window.GetScrollPos(orientation);
    const int line = orientation == wxHORIZONTAL ? std::max(1, thumb / 10) : 1;

    if (event.GetEventType() == wxEVT_SCROLLWIN_TOP)
    {
        return 0;
    }
    if (event.GetEventType() == wxEVT_SCROLLWIN_BOTTOM)
    {
        return maximum;
    }
    if (event.GetEventType() == wxEVT_SCROLLWIN_LINEUP)
    {
        return std::max(0, position - line);
    }
    if (event.GetEventType() == wxEVT_SCROLLWIN_LINEDOWN)
    {
        return std::min(maximum, position + line);
    }
    if (event.GetEventType() == wxEVT_SCROLLWIN_PAGEUP)
    {
        return std::max(0, position - thumb);
    }
    if (event.GetEventType() == wxEVT_SCROLLWIN_PAGEDOWN)
    {
        return std::min(maximum, position + thumb);
    }
    return std::clamp(event.GetPosition(), 0, maximum);
}

} // namespace

wxTimelineControl::wxTimelineControl(wxWindow *parent) :
    wxTimelineControl(parent, wxID_ANY)
{
}

wxTimelineControl::wxTimelineControl(wxWindow *parent, wxWindowID id) :
    wxPanel(parent, id, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxHSCROLL | wxVSCROLL)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_MOTION, &wxTimelineControl::on_mouse_move, this);
    Bind(wxEVT_LEFT_DOWN, &wxTimelineControl::on_mouse_down, this);
    Bind(wxEVT_LEFT_UP, &wxTimelineControl::on_mouse_up, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &wxTimelineControl::on_capture_lost, this);
    Bind(wxEVT_CHAR_HOOK, &wxTimelineControl::on_key_down, this);
    Bind(wxEVT_LEAVE_WINDOW, &wxTimelineControl::on_mouse_leave, this);
    Bind(wxEVT_MOUSEWHEEL, &wxTimelineControl::on_mouse_wheel, this);
    Bind(wxEVT_PAINT, &wxTimelineControl::on_paint, this);
    Bind(wxEVT_SIZE, &wxTimelineControl::on_resize, this);
    Bind(wxEVT_SCROLLWIN_TOP, &wxTimelineControl::on_scroll, this);
    Bind(wxEVT_SCROLLWIN_BOTTOM, &wxTimelineControl::on_scroll, this);
    Bind(wxEVT_SCROLLWIN_LINEUP, &wxTimelineControl::on_scroll, this);
    Bind(wxEVT_SCROLLWIN_LINEDOWN, &wxTimelineControl::on_scroll, this);
    Bind(wxEVT_SCROLLWIN_PAGEUP, &wxTimelineControl::on_scroll, this);
    Bind(wxEVT_SCROLLWIN_PAGEDOWN, &wxTimelineControl::on_scroll, this);
    Bind(wxEVT_SCROLLWIN_THUMBTRACK, &wxTimelineControl::on_scroll, this);
    Bind(wxEVT_SCROLLWIN_THUMBRELEASE, &wxTimelineControl::on_scroll, this);
}

void wxTimelineControl::set_document(timeline::Document document)
{
    if (HasCapture())
    {
        ReleaseMouse();
    }
    m_document = std::move(document);
    m_interaction.emplace(*m_document);
    m_inspection.reset();
    m_hit_result.reset();
    m_layout.reset();
    m_layout_metrics.reset();
    m_navigation.reset();
    m_viewport.reset();
    if (m_document->frame_grid() && m_document->frame_grid()->frame_count() > 0)
    {
        m_interaction->move_playhead_frame(0);
        m_inspection = timeline::inspect_frame(*m_document, 0);
    }
    const std::optional<timeline::Time> content_start = m_document->content_start();
    const std::optional<timeline::Time> content_end = m_document->content_end();
    if (content_start && content_end && *content_start < *content_end)
    {
        m_navigation.emplace(*content_start, *content_end, m_document->lane_count());
    }
    notify_inspection_changed();
    update_scrollbars();
    Refresh(false);
}

void wxTimelineControl::zoom_in()
{
    zoom_by(zoom_step);
}

void wxTimelineControl::zoom_out()
{
    zoom_by(1.0 / zoom_step);
}

void wxTimelineControl::fit_view()
{
    if (!m_navigation)
    {
        return;
    }
    m_navigation->fit();
    Refresh(false);
}

std::string wxTimelineControl::snapshot()
{
    Update();
    return m_layout ? timeline::render_snapshot(m_layout->display_list())
                    : timeline::render_snapshot(timeline::DisplayList{});
}

void wxTimelineControl::zoom_by(double factor)
{
    if (!m_navigation || !m_viewport)
    {
        return;
    }
    const timeline::Ticks middle_tick =
        m_viewport->start().ticks() + (m_viewport->end().ticks() - m_viewport->start().ticks()) / 2;
    m_navigation->zoom_by(factor, timeline::Time::from_ticks(middle_tick));
    Refresh(false);
}

void wxTimelineControl::notify_inspection_changed()
{
    wxCommandEvent event(wxEVT_TIMELINE_INSPECTION_CHANGED, GetId());
    event.SetEventObject(this);
    ProcessWindowEvent(event);
}

void wxTimelineControl::on_mouse_move(wxMouseEvent &event)
{
    if (HasCapture() && event.LeftIsDown() && m_interaction && m_viewport && m_layout_metrics)
    {
        m_interaction->extend_range(timeline::time_at_x(event.GetX(), *m_viewport, *m_layout_metrics));
        update_interaction();
        return;
    }
    const std::optional<timeline::HitResult> hit = m_layout
        ? m_layout->hit_test(timeline::Point{event.GetX(), event.GetY() - m_layout_top}, FromDIP(3))
        : std::nullopt;
    bool changed = hit != m_hit_result;
    m_hit_result = hit;
    if (m_document && m_document->frame_grid() && m_layout_metrics && m_viewport &&
        m_layout_metrics->lane_label_width() <= event.GetX() && event.GetX() < m_viewport->width() &&
        m_layout_top <= event.GetY() && event.GetY() < m_layout_top + m_viewport->height())
    {
        const timeline::Time time = timeline::time_at_x(event.GetX(), *m_viewport, *m_layout_metrics);
        const std::optional<timeline::Ticks> frame = m_document->frame_grid()->nearest_frame(time);
        if (frame && (!m_inspection || m_inspection->frame != *frame))
        {
            m_inspection = timeline::inspect_frame(*m_document, *frame);
            changed = true;
        }
    }
    if (changed)
    {
        notify_inspection_changed();
    }
    event.Skip();
}

void wxTimelineControl::clear_hit()
{
    if (m_hit_result)
    {
        m_hit_result.reset();
        notify_inspection_changed();
    }
}

void wxTimelineControl::update_interaction()
{
    if (m_document && m_interaction && m_interaction->playhead_frame())
    {
        m_inspection = timeline::inspect_frame(*m_document, *m_interaction->playhead_frame());
    }
    notify_inspection_changed();
    Refresh(false);
}

void wxTimelineControl::clear_selection()
{
    if (HasCapture())
    {
        ReleaseMouse();
    }
    if (m_interaction)
    {
        m_interaction->clear_selection();
        update_interaction();
    }
}

void wxTimelineControl::on_mouse_down(wxMouseEvent &event)
{
    if (!m_layout || !m_interaction || !m_viewport || !m_layout_metrics || event.GetY() < m_layout_top ||
        m_layout_top + m_viewport->height() <= event.GetY())
    {
        event.Skip();
        return;
    }
    SetFocus();
    const timeline::Point point{event.GetX(), event.GetY() - m_layout_top};
    m_hit_result = m_layout->hit_test(point, FromDIP(3));
    m_interaction->select_hit(m_hit_result, event.ControlDown());
    if (m_layout_metrics->lane_label_width() <= point.x)
    {
        m_interaction->begin_range(timeline::time_at_x(point.x, *m_viewport, *m_layout_metrics));
        if (!HasCapture())
        {
            CaptureMouse();
        }
    }
    update_interaction();
}

void wxTimelineControl::on_mouse_up(wxMouseEvent &event)
{
    if (HasCapture())
    {
        if (m_interaction && m_viewport && m_layout_metrics)
        {
            m_interaction->extend_range(timeline::time_at_x(event.GetX(), *m_viewport, *m_layout_metrics));
            m_interaction->end_range();
        }
        ReleaseMouse();
        update_interaction();
    }
    event.Skip();
}

void wxTimelineControl::on_capture_lost(wxMouseCaptureLostEvent &)
{
    if (m_interaction)
    {
        m_interaction->end_range();
    }
}

void wxTimelineControl::on_key_down(wxKeyEvent &event)
{
    if (event.GetKeyCode() == WXK_ESCAPE)
    {
        clear_selection();
        return;
    }
    if (m_interaction && (event.GetKeyCode() == WXK_LEFT || event.GetKeyCode() == WXK_RIGHT))
    {
        m_interaction->step_playhead(event.GetKeyCode() == WXK_LEFT ? -1 : 1, event.ShiftDown());
        update_interaction();
        return;
    }
    event.Skip();
}

void wxTimelineControl::on_mouse_leave(wxMouseEvent &event)
{
    clear_hit();
    event.Skip();
}

void wxTimelineControl::on_mouse_wheel(wxMouseEvent &event)
{
    if (!m_navigation || !m_viewport || !m_layout_metrics || event.GetWheelDelta() == 0)
    {
        event.Skip();
        return;
    }

    int steps = event.GetWheelRotation() / event.GetWheelDelta();
    if (steps == 0)
    {
        steps = event.GetWheelRotation() < 0 ? -1 : 1;
    }

    if (event.ControlDown())
    {
        const timeline::Time anchor = timeline::time_at_x(event.GetX(), *m_viewport, *m_layout_metrics);
        m_navigation->zoom_by(std::pow(zoom_step, steps), anchor);
    }
    else if (event.ShiftDown())
    {
        const timeline::Ticks visible_ticks = m_viewport->end().ticks() - m_viewport->start().ticks();
        const auto delta = std::max<timeline::Ticks>(1, visible_ticks / 10);
        m_navigation->scroll_to(
            timeline::Time::from_ticks(m_viewport->start().ticks() - static_cast<timeline::Ticks>(steps) * delta));
    }
    else
    {
        const int visible_lanes = std::max(1, timeline::visible_lane_count(*m_viewport, *m_layout_metrics));
        m_navigation->scroll_to_lane(m_viewport->first_lane() - steps, visible_lanes);
    }
    Refresh(false);
}

void wxTimelineControl::on_resize(wxSizeEvent &event)
{
    m_layout.reset();
    clear_hit();
    m_layout_metrics.reset();
    m_viewport.reset();
    Refresh(false);
    event.Skip();
}

void wxTimelineControl::on_scroll(wxScrollWinEvent &event)
{
    if (!m_navigation || !m_viewport || !m_layout_metrics)
    {
        return;
    }

    const int position = scroll_event_position(event, *this);
    if (event.GetOrientation() == wxHORIZONTAL)
    {
        const int maximum = GetScrollRange(wxHORIZONTAL) - GetScrollThumb(wxHORIZONTAL);
        const double fraction = maximum > 0 ? static_cast<double>(position) / maximum : 0.0;
        m_navigation->scroll_to_fraction(fraction);
    }
    else
    {
        const int visible_lanes = std::max(1, timeline::visible_lane_count(*m_viewport, *m_layout_metrics));
        m_navigation->scroll_to_lane(position, visible_lanes);
    }
    Refresh(false);
}

void wxTimelineControl::update_scrollbars()
{
    if (!m_document || !m_navigation || !m_viewport || !m_layout_metrics)
    {
        SetScrollbar(wxHORIZONTAL, 0, 1, 1, true);
        SetScrollbar(wxVERTICAL, 0, 1, 1, true);
        return;
    }

    const int horizontal_thumb =
        std::clamp(static_cast<int>(std::lround(scroll_range / m_navigation->zoom_scale())), 1, scroll_range);
    const int horizontal_maximum = scroll_range - horizontal_thumb;
    const int horizontal_position =
        static_cast<int>(std::lround(m_navigation->horizontal_fraction() * horizontal_maximum));
    const int vertical_range = std::max(1, m_document->lane_count());
    const int vertical_thumb =
        std::clamp(timeline::visible_lane_count(*m_viewport, *m_layout_metrics), 1, vertical_range);
    const int vertical_position = m_viewport->first_lane();

    SetScrollbar(wxHORIZONTAL, horizontal_position, horizontal_thumb, scroll_range, true);
    SetScrollbar(wxVERTICAL, vertical_position, vertical_thumb, vertical_range, true);
}

void wxTimelineControl::on_paint(wxPaintEvent &)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(GetBackgroundColour()));
    dc.Clear();
    dc.SetTextForeground(GetForegroundColour());
    m_layout.reset();
    clear_hit();
    m_layout_metrics.reset();
    m_viewport.reset();
    m_layout_top = 0;

    wxPoint position(12, 12);
    const auto draw_line = [&dc, &position](const wxString &text)
    {
        dc.DrawText(text, position);
        position.y += dc.GetCharHeight() + 4;
    };

    if (!m_document)
    {
        draw_line("No timeline loaded.");
        update_scrollbars();
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
        const timeline::FrameGrid &frame_grid = *m_document->frame_grid();
        draw_line(wxString::Format("Frames: %lld", static_cast<long long>(frame_grid.frame_count())));
        draw_line(wxString::Format("Frame rate: %lld/%lld fps",
            static_cast<long long>(frame_grid.frames_per_second_numerator()),
            static_cast<long long>(frame_grid.frames_per_second_denominator())));
    }
    if (m_document->source_summary())
    {
        const timeline::SourceSummary &summary = *m_document->source_summary();
        draw_line("Schema: " + wxString::FromUTF8(summary.schema().c_str()) +
            wxString::Format(" v%d", summary.schema_version()));
        draw_line(wxString::Format("Features: %d", summary.feature_count()));
        draw_line(wxString::Format("Events: %d", summary.event_count()));
        if (summary.generation_summary())
        {
            const timeline::GenerationSummary &generation = *summary.generation_summary();
            draw_line("Generator: " + wxString::FromUTF8(generation.generator_name().c_str()) + " " +
                wxString::FromUTF8(generation.generator_version().c_str()));
            draw_line(wxString::Format("Inputs: %d", timeline::size_cast(generation.source_references())));
            if (!generation.target_counts().empty())
            {
                draw_line(
                    wxString::Format("Generated target groups: %d", timeline::size_cast(generation.target_counts())));
            }
            if (!generation.source_counts().empty())
            {
                draw_line(wxString::Format("Music source groups: %d", timeline::size_cast(generation.source_counts())));
            }
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

    if (!m_navigation)
    {
        update_scrollbars();
        return;
    }

    const wxSize client_size = GetClientSize();
    m_layout_top = position.y + 8;
    const int layout_height = client_size.GetHeight() - m_layout_top - 8;
    if (client_size.GetWidth() <= 160 || layout_height <= 40)
    {
        return;
    }

    int label_width = 80;
    for (const timeline::Lane &lane : m_document->lanes())
    {
        label_width = std::max(label_width, dc.GetTextExtent(wxString::FromUTF8(lane.label().c_str())).GetWidth() + 16);
    }
    label_width = std::min(label_width, client_size.GetWidth() / 2);
    const timeline::LayoutMetrics layout_metrics(label_width, dc.GetCharHeight() + 8, dc.GetCharHeight() + 16, 4);
    timeline::Viewport viewport = m_navigation->viewport(client_size.GetWidth(), layout_height);
    const int visible_lanes = std::max(1, timeline::visible_lane_count(viewport, layout_metrics));
    m_navigation->scroll_to_lane(viewport.first_lane(), visible_lanes);
    viewport = m_navigation->viewport(client_size.GetWidth(), layout_height);
    m_layout.emplace(*m_document, viewport, layout_metrics, *m_interaction);
    m_layout_metrics = layout_metrics;
    m_viewport = viewport;
    draw_timeline_display_list(dc, m_layout->display_list(), wxPoint(0, m_layout_top));
    update_scrollbars();
}
