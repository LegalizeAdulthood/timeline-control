// Copyright (c) 2026 Richard Thomson

#include <timeline/Interaction.h>

#include <algorithm>
#include <stdexcept>

namespace timeline
{

TimeRange::TimeRange(Time start, Time end) :
    m_start(start),
    m_end(end)
{
    if (m_end < m_start)
    {
        throw std::invalid_argument("timeline selection range is reversed");
    }
}

Interaction::Interaction(const Document &document) :
    m_start(document.content_start()),
    m_end(document.content_end()),
    m_frame_grid(document.frame_grid())
{
}

std::optional<Time> Interaction::snap(Time time) const
{
    if (!m_start || !m_end || (m_frame_grid && m_frame_grid->frame_count() == 0))
    {
        return std::nullopt;
    }
    if (m_frame_grid && m_frame_grid->frame_count() > 0)
    {
        const auto first = m_frame_grid->offset();
        const auto last = m_frame_grid->frame_start(m_frame_grid->frame_count() - 1);
        const auto bounded = Time::from_ticks(std::clamp(time.ticks(), first.ticks(), last.ticks()));
        return m_frame_grid->frame_start(*m_frame_grid->nearest_frame(bounded));
    }
    return Time::from_ticks(std::clamp(time.ticks(), m_start->ticks(), m_end->ticks()));
}

void Interaction::move_playhead_frame(Ticks frame)
{
    if (m_frame_grid && m_frame_grid->frame_count() > 0)
    {
        m_playhead = m_frame_grid->frame_start(std::clamp(frame, Ticks{0}, m_frame_grid->frame_count() - 1));
    }
}

void Interaction::step_playhead(int frames, bool extend_selection)
{
    if (!m_frame_grid || m_frame_grid->frame_count() == 0)
    {
        return;
    }
    const auto frame = playhead_frame().value_or(0);
    if (extend_selection && !m_range_anchor)
    {
        m_range_anchor = m_frame_grid->frame_start(frame);
    }
    const auto delta = std::clamp<Ticks>(frames, -frame, m_frame_grid->frame_count() - 1 - frame);
    move_playhead_frame(frame + delta);
    if (extend_selection)
    {
        select_range(*m_range_anchor, *m_playhead);
    }
    else
    {
        m_selected_range.reset();
        end_range();
    }
}

bool Interaction::is_selected(const DisplayId &id) const
{
    return std::any_of(m_selected_items.begin(), m_selected_items.end(),
        [&id](const DisplayId &item) { return item.lane_id == id.lane_id && item.item_id == id.item_id; });
}

void Interaction::select_hit(const std::optional<HitResult> &hit, bool additive)
{
    if (!hit)
    {
        if (!additive)
        {
            clear_selection();
        }
        return;
    }
    if (hit->id.lane_id.empty())
    {
        return;
    }
    m_selected_lane = hit->id.lane_id;
    m_selected_range.reset();
    m_range_anchor.reset();
    if (!additive || hit->id.item_id.empty())
    {
        m_selected_items.clear();
    }
    if (!hit->id.item_id.empty())
    {
        const auto found = std::find_if(m_selected_items.begin(), m_selected_items.end(), [&hit](const DisplayId &item)
            { return item.lane_id == hit->id.lane_id && item.item_id == hit->id.item_id; });
        if (found == m_selected_items.end())
        {
            m_selected_items.push_back(hit->id);
        }
        else
        {
            m_selected_items.erase(found);
        }
    }
}

void Interaction::select_range(Time start, Time end)
{
    const auto first = snap(start);
    const auto last = snap(end);
    if (!first || !last)
    {
        return;
    }
    m_selected_items.clear();
    m_selected_range.emplace(std::min(*first, *last), std::max(*first, *last));
}

std::optional<FrameRange> Interaction::selected_frames() const
{
    if (!m_selected_range || !m_frame_grid || m_frame_grid->frame_count() == 0)
    {
        return std::nullopt;
    }
    return FrameRange(
        *m_frame_grid->nearest_frame(m_selected_range->start()), *m_frame_grid->nearest_frame(m_selected_range->end()));
}

void Interaction::clear_selection()
{
    m_selected_items.clear();
    m_selected_lane.reset();
    m_selected_range.reset();
    m_range_anchor.reset();
}

void Interaction::begin_range(Time time)
{
    move_playhead(time);
    m_selected_range.reset();
    m_range_anchor = m_playhead;
}

void Interaction::extend_range(Time time)
{
    if (!m_range_anchor)
    {
        return;
    }
    move_playhead(time);
    if (m_playhead && *m_range_anchor != *m_playhead)
    {
        select_range(*m_range_anchor, *m_playhead);
    }
    else if (m_selected_range)
    {
        select_range(*m_range_anchor, *m_range_anchor);
    }
}

} // namespace timeline
