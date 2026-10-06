// Copyright (c) 2026 Richard Thomson

#include <timeline/ControlState.h>

#include <algorithm>
#include <utility>

namespace timeline
{

void ControlState::set_document(Document document)
{
    m_document = std::move(document);
    m_interaction.emplace(*m_document);
    m_navigation.reset();
    m_inspection.reset();
    m_hit_result.reset();
    m_layout.reset();
    m_viewport.reset();
    m_layout_metrics.reset();
    if (m_document->frame_grid() && m_document->frame_grid()->frame_count() > 0)
    {
        m_interaction->move_playhead_frame(0);
        m_inspection = inspect_frame(*m_document, 0);
    }
    const std::optional<Time> start = m_document->content_start();
    const std::optional<Time> end = m_document->content_end();
    if (start && end && *start < *end)
    {
        m_navigation.emplace(*start, *end, m_document->lane_count());
    }
}

bool ControlState::rebuild_layout(int width, int height, LayoutMetrics metrics)
{
    clear_layout();
    if (!m_document || !m_interaction || !m_navigation || width < 2 || height <= metrics.ruler_height())
    {
        return false;
    }
    m_layout_metrics = metrics;
    const Viewport viewport = m_navigation->viewport(width, height);
    m_navigation->scroll_to_lane(viewport.first_lane(), std::max(1, visible_lane_count(viewport, *m_layout_metrics)));
    m_viewport = m_navigation->viewport(width, height);
    m_layout.emplace(*m_document, *m_viewport, *m_layout_metrics, *m_interaction);
    return true;
}

void ControlState::clear_layout()
{
    m_layout.reset();
    m_viewport.reset();
    m_layout_metrics.reset();
    m_hit_result.reset();
}

void ControlState::fit_view()
{
    if (m_navigation)
    {
        m_navigation->fit();
        rebuild_current_layout();
    }
}

void ControlState::zoom_by(double factor)
{
    if (m_navigation && m_viewport)
    {
        const Ticks middle =
            m_viewport->start().ticks() + (m_viewport->end().ticks() - m_viewport->start().ticks()) / 2;
        m_navigation->zoom_by(factor, Time::from_ticks(middle));
        rebuild_current_layout();
    }
}

void ControlState::zoom_at(double factor, int x)
{
    if (m_navigation && m_viewport && m_layout_metrics)
    {
        m_navigation->zoom_by(factor, time_at_x(x, *m_viewport, *m_layout_metrics));
        rebuild_current_layout();
    }
}

void ControlState::scroll_by(Duration distance)
{
    if (m_navigation && m_viewport)
    {
        m_navigation->scroll_to(m_viewport->start() + distance);
        rebuild_current_layout();
    }
}

void ControlState::scroll_to_fraction(double fraction)
{
    if (m_navigation)
    {
        m_navigation->scroll_to_fraction(fraction);
        rebuild_current_layout();
    }
}

void ControlState::scroll_lanes(int lanes)
{
    if (m_navigation && m_viewport && m_layout_metrics)
    {
        const int visible_lanes = std::max(1, visible_lane_count(*m_viewport, *m_layout_metrics));
        m_navigation->scroll_to_lane(m_viewport->first_lane() + lanes, visible_lanes);
        rebuild_current_layout();
    }
}

bool ControlState::begin_selection(Point point, int hit_tolerance, bool additive)
{
    if (!m_interaction || !m_layout || !m_viewport || !m_layout_metrics)
    {
        return false;
    }
    m_hit_result = m_layout->hit_test(point, hit_tolerance);
    m_interaction->select_hit(m_hit_result, additive);
    const bool range_started = point.x >= m_layout_metrics->lane_label_width();
    if (range_started)
    {
        m_interaction->begin_range(time_at_x(point.x, *m_viewport, *m_layout_metrics));
    }
    update_inspection();
    rebuild_current_layout();
    return range_started;
}

void ControlState::extend_range(Point point)
{
    if (m_interaction && m_viewport && m_layout_metrics)
    {
        m_interaction->extend_range(time_at_x(point.x, *m_viewport, *m_layout_metrics));
        update_inspection();
        rebuild_current_layout();
    }
}

void ControlState::end_range()
{
    if (m_interaction)
    {
        m_interaction->end_range();
    }
}

void ControlState::clear_selection()
{
    if (m_interaction)
    {
        m_interaction->end_range();
        m_interaction->clear_selection();
        update_inspection();
        rebuild_current_layout();
    }
}

void ControlState::step_playhead(int frames, bool extend_selection)
{
    if (!m_interaction)
    {
        return;
    }
    m_interaction->step_playhead(frames, extend_selection);
    if (m_navigation && m_interaction->playhead())
    {
        m_navigation->reveal(*m_interaction->playhead());
    }
    update_inspection();
    rebuild_current_layout();
}

void ControlState::hover_at(Point point, int hit_tolerance, bool inspect_frame)
{
    if (!m_layout)
    {
        m_hit_result.reset();
        return;
    }
    m_hit_result = m_layout->hit_test(point, hit_tolerance);
    if (inspect_frame && m_document->frame_grid() && point.x >= m_layout_metrics->lane_label_width())
    {
        const std::optional<Ticks> frame =
            m_document->frame_grid()->nearest_frame(time_at_x(point.x, *m_viewport, *m_layout_metrics));
        if (frame && (!m_inspection || m_inspection->frame != *frame))
        {
            m_inspection = timeline::inspect_frame(*m_document, *frame);
        }
    }
}

void ControlState::clear_hover()
{
    m_hit_result.reset();
}

void ControlState::rebuild_current_layout()
{
    if (!m_viewport || !m_layout_metrics)
    {
        invalidate_layout();
        return;
    }
    const int width = m_viewport->width();
    const int height = m_viewport->height();
    const LayoutMetrics metrics = *m_layout_metrics;
    rebuild_layout(width, height, metrics);
}

void ControlState::update_inspection()
{
    if (m_document && m_interaction && m_interaction->playhead_frame())
    {
        m_inspection = inspect_frame(*m_document, *m_interaction->playhead_frame());
    }
    else
    {
        m_inspection.reset();
    }
}

} // namespace timeline
