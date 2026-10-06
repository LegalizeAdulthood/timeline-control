// Copyright (c) 2026 Richard Thomson

#include <wxTimeline/wxTimelineControl.h>
#include <wxTimeline/wxTimelineRenderer.h>

#include <timeline/Layout.h>
#include <timeline/Snapshot.h>

#include <wx/dcbuffer.h>
#include <wx/dcclient.h>
#include <wx/renderer.h>
#include <wx/settings.h>

#include <algorithm>
#include <cmath>
#include <string_view>
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
    wxPanel::SetBackgroundStyle(wxBG_STYLE_PAINT);
    wxPanel::SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
    wxPanel::SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    Bind(wxEVT_MOTION, &wxTimelineControl::on_mouse_move, this);
    Bind(wxEVT_LEFT_DOWN, &wxTimelineControl::on_mouse_down, this);
    Bind(wxEVT_LEFT_UP, &wxTimelineControl::on_mouse_up, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &wxTimelineControl::on_capture_lost, this);
    Bind(wxEVT_CHAR_HOOK, &wxTimelineControl::on_key_down, this);
    Bind(wxEVT_LEAVE_WINDOW, &wxTimelineControl::on_mouse_leave, this);
    Bind(wxEVT_MOUSEWHEEL, &wxTimelineControl::on_mouse_wheel, this);
    Bind(wxEVT_PAINT, &wxTimelineControl::on_paint, this);
    Bind(wxEVT_SIZE, &wxTimelineControl::on_resize, this);
    Bind(wxEVT_SET_FOCUS, &wxTimelineControl::on_focus, this);
    Bind(wxEVT_KILL_FOCUS, &wxTimelineControl::on_focus, this);
    Bind(wxEVT_DPI_CHANGED, &wxTimelineControl::on_dpi_changed, this);
    Bind(wxEVT_SYS_COLOUR_CHANGED, &wxTimelineControl::on_system_colour_changed, this);
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
    m_state.set_document(std::move(document));
    m_hover_point.reset();
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
    if (!m_state.navigation())
    {
        return;
    }
    m_state.fit_view();
    Refresh(false);
}

std::string wxTimelineControl::snapshot()
{
    wxClientDC dc(this);
    dc.SetFont(GetFont());
    rebuild_layout(dc);
    return m_state.layout() ? timeline::render_snapshot(m_state.layout()->display_list())
                            : timeline::render_snapshot(timeline::DisplayList{});
}

void wxTimelineControl::zoom_by(double factor)
{
    if (!m_state.navigation() || !m_state.viewport())
    {
        return;
    }
    m_state.zoom_by(factor);
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
    if (!m_state.layout())
    {
        wxClientDC dc(this);
        dc.SetFont(GetFont());
        rebuild_layout(dc);
    }
    m_hover_point = timeline::Point{event.GetX(), event.GetY()};
    if (HasCapture() && event.LeftIsDown() && m_state.interaction() && m_state.viewport() && m_state.layout_metrics())
    {
        m_state.extend_range(*m_hover_point);
        update_control();
        return;
    }
    const std::optional<timeline::HitResult> old_hit = m_state.hit_result();
    const std::optional<timeline::Ticks> old_frame =
        m_state.inspection() ? std::optional<timeline::Ticks>(m_state.inspection()->frame) : std::nullopt;
    const bool inspect_frame = m_state.viewport() && 0 <= event.GetX() && event.GetX() < m_state.viewport()->width() &&
        0 <= event.GetY() && event.GetY() < m_state.viewport()->height();
    m_state.hover_at(*m_hover_point, FromDIP(3), inspect_frame);
    const std::optional<timeline::Ticks> frame =
        m_state.inspection() ? std::optional<timeline::Ticks>(m_state.inspection()->frame) : std::nullopt;
    const bool changed = m_state.hit_result() != old_hit || frame != old_frame;
    if (changed)
    {
        notify_inspection_changed();
    }
    event.Skip();
}

void wxTimelineControl::clear_hit()
{
    if (m_state.hit_result())
    {
        m_state.clear_hover();
        notify_inspection_changed();
    }
}

void wxTimelineControl::update_control()
{
    if (m_hover_point && m_state.layout())
    {
        m_state.hover_at(*m_hover_point, FromDIP(3), false);
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
    if (m_state.interaction())
    {
        m_state.clear_selection();
        update_control();
    }
}

void wxTimelineControl::step_playhead(int frames, bool extend_selection)
{
    if (!m_state.interaction())
    {
        return;
    }
    m_state.step_playhead(frames, extend_selection);
    update_control();
}

void wxTimelineControl::on_mouse_down(wxMouseEvent &event)
{
    SetFocus();
    Update();
    if (!m_state.layout() || !m_state.interaction() || !m_state.viewport() || !m_state.layout_metrics() ||
        event.GetY() < 0 || m_state.viewport()->height() <= event.GetY())
    {
        event.Skip();
        return;
    }
    const timeline::Point point{event.GetX(), event.GetY()};
    m_hover_point = point;
    if (m_state.begin_selection(point, FromDIP(3), event.ControlDown()))
    {
        if (!HasCapture())
        {
            CaptureMouse();
        }
    }
    update_control();
}

void wxTimelineControl::on_mouse_up(wxMouseEvent &event)
{
    if (HasCapture())
    {
        m_state.extend_range(timeline::Point{event.GetX(), event.GetY()});
        m_state.end_range();
        ReleaseMouse();
        update_control();
    }
    event.Skip();
}

void wxTimelineControl::on_capture_lost(wxMouseCaptureLostEvent &)
{
    m_state.end_range();
}

void wxTimelineControl::on_key_down(wxKeyEvent &event)
{
    if (!HasFocus() || event.ControlDown() || event.AltDown() || event.MetaDown())
    {
        event.Skip();
        return;
    }
    if (event.GetKeyCode() == WXK_TAB)
    {
        Navigate(event.ShiftDown() ? wxNavigationKeyEvent::IsBackward : wxNavigationKeyEvent::IsForward);
        return;
    }
    if (event.GetKeyCode() == WXK_ESCAPE)
    {
        clear_selection();
        return;
    }
    if (m_state.interaction() && (event.GetKeyCode() == WXK_LEFT || event.GetKeyCode() == WXK_RIGHT))
    {
        step_playhead(event.GetKeyCode() == WXK_LEFT ? -1 : 1, event.ShiftDown());
        return;
    }
    event.Skip();
}

void wxTimelineControl::on_mouse_leave(wxMouseEvent &event)
{
    m_hover_point.reset();
    clear_hit();
    event.Skip();
}

void wxTimelineControl::on_mouse_wheel(wxMouseEvent &event)
{
    if (!m_state.navigation() || !m_state.viewport() || !m_state.layout_metrics() || event.GetWheelDelta() == 0)
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
        m_state.zoom_at(std::pow(zoom_step, steps), event.GetX());
    }
    else if (event.ShiftDown())
    {
        const timeline::Ticks visible_ticks = m_state.viewport()->end().ticks() - m_state.viewport()->start().ticks();
        const timeline::Ticks delta = std::max<timeline::Ticks>(1, visible_ticks / 10);
        m_state.scroll_by(timeline::Duration::from_ticks(-static_cast<timeline::Ticks>(steps) * delta));
    }
    else
    {
        m_state.scroll_lanes(-steps);
    }
    Refresh(false);
}

void wxTimelineControl::invalidate_layout()
{
    clear_hit();
    m_state.clear_layout();
    Refresh(false);
}

void wxTimelineControl::on_resize(wxSizeEvent &event)
{
    invalidate_layout();
    event.Skip();
}

void wxTimelineControl::on_dpi_changed(wxDPIChangedEvent &event)
{
    invalidate_layout();
    event.Skip();
}

void wxTimelineControl::on_system_colour_changed(wxSysColourChangedEvent &event)
{
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
    SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    invalidate_layout();
    event.Skip();
}

void wxTimelineControl::on_focus(wxFocusEvent &event)
{
    if (event.GetEventType() == wxEVT_KILL_FOCUS)
    {
        m_state.end_range();
        if (HasCapture())
        {
            ReleaseMouse();
        }
    }
    Refresh(false);
    event.Skip();
}

void wxTimelineControl::on_scroll(wxScrollWinEvent &event)
{
    if (!m_state.navigation() || !m_state.viewport() || !m_state.layout_metrics())
    {
        return;
    }

    const int position = scroll_event_position(event, *this);
    if (event.GetOrientation() == wxHORIZONTAL)
    {
        const int maximum = GetScrollRange(wxHORIZONTAL) - GetScrollThumb(wxHORIZONTAL);
        const double fraction = maximum > 0 ? static_cast<double>(position) / maximum : 0.0;
        m_state.scroll_to_fraction(fraction);
    }
    else
    {
        m_state.scroll_lanes(position - m_state.viewport()->first_lane());
    }
    Refresh(false);
}

void wxTimelineControl::update_scrollbars()
{
    if (!m_state.document() || !m_state.navigation() || !m_state.viewport() || !m_state.layout_metrics())
    {
        SetScrollbar(wxHORIZONTAL, 0, 1, 1, true);
        SetScrollbar(wxVERTICAL, 0, 1, 1, true);
        return;
    }

    const int horizontal_thumb =
        std::clamp(static_cast<int>(std::lround(scroll_range / m_state.navigation()->zoom_scale())), 1, scroll_range);
    const int horizontal_maximum = scroll_range - horizontal_thumb;
    const int horizontal_position =
        static_cast<int>(std::lround(m_state.navigation()->horizontal_fraction() * horizontal_maximum));
    const int vertical_range = std::max(1, m_state.document()->lane_count());
    const int vertical_thumb =
        std::clamp(timeline::visible_lane_count(*m_state.viewport(), *m_state.layout_metrics()), 1, vertical_range);
    const int vertical_position = m_state.viewport()->first_lane();

    SetScrollbar(wxHORIZONTAL, horizontal_position, horizontal_thumb, scroll_range, true);
    SetScrollbar(wxVERTICAL, vertical_position, vertical_thumb, vertical_range, true);
}

void wxTimelineControl::rebuild_layout(wxDC &dc)
{
    const std::optional<timeline::HitResult> old_hit = m_state.hit_result();
    m_state.clear_layout();

    const wxSize client_size = GetClientSize();
    if (!m_state.document() || !m_state.navigation())
    {
        update_scrollbars();
        return;
    }
    if (client_size.GetWidth() <= FromDIP(160) || client_size.GetHeight() <= FromDIP(40))
    {
        if (old_hit)
        {
            notify_inspection_changed();
        }
        update_scrollbars();
        return;
    }

    int label_width = FromDIP(80);
    for (const timeline::Lane &lane : m_state.document()->lanes())
    {
        const std::string_view label = m_state.document()->strings().lookup(lane.label());
        label_width =
            std::max(label_width, dc.GetTextExtent(wxString(label.data(), label.size())).GetWidth() + FromDIP(16));
    }
    label_width = std::min(label_width, client_size.GetWidth() / 2);
    const timeline::LayoutMetrics layout_metrics(
        label_width, dc.GetCharHeight() + FromDIP(8), dc.GetCharHeight() + FromDIP(16), FromDIP(4));
    if (!m_state.rebuild_layout(client_size.GetWidth(), client_size.GetHeight(), layout_metrics))
    {
        update_scrollbars();
        return;
    }
    if (m_hover_point)
    {
        m_state.hover_at(*m_hover_point, FromDIP(3), false);
    }
    if (m_state.hit_result() != old_hit)
    {
        notify_inspection_changed();
    }
    update_scrollbars();
}

void wxTimelineControl::on_paint(wxPaintEvent &)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(GetBackgroundColour()));
    dc.Clear();
    dc.SetFont(GetFont());
    dc.SetTextForeground(GetForegroundColour());
    rebuild_layout(dc);

    if (!m_state.document() || !m_state.navigation())
    {
        dc.DrawText(m_state.document() ? "No timeline content." : "No timeline loaded.", FromDIP(wxPoint(12, 12)));
        return;
    }
    if (m_state.layout())
    {
        const wxTimelinePalette palette{
            GetBackgroundColour(), GetForegroundColour(), wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)};
        const wxDCClipper clip(dc, GetClientRect());
        draw_display_list(dc, m_state.layout()->display_list(), palette, FromDIP(1), HasFocus());
        if (HasFocus())
        {
            wxRendererNative::Get().DrawFocusRect(this, dc, GetClientRect(), wxCONTROL_FOCUSED);
        }
    }
}
