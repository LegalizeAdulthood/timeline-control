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
}

} // namespace timeline
