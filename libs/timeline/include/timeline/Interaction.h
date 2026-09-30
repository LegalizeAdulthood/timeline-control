// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/Layout.h>

namespace timeline
{

/// Inclusive exact-time selection with ordered endpoints.
///
class TimeRange
{
public:
    TimeRange(Time start, Time end);

    Time start() const
    {
        return m_start;
    }
    Time end() const
    {
        return m_end;
    }

private:
    Time m_start;
    Time m_end;
};

/// View-only playhead and selection state independent of native GUI objects.
///
/// Positions are clamped to document bounds and snapped to a frame grid when
/// one exists. Item identities are lane-qualified and survive layout changes.
/// Creating new state for a replacement document clears all interaction.
///
class Interaction
{
public:
    explicit Interaction(const Document &document);

    const std::optional<Time> &playhead() const
    {
        return m_playhead;
    }
    std::optional<Ticks> playhead_frame() const
    {
        return m_playhead && m_frame_grid ? m_frame_grid->nearest_frame(*m_playhead) : std::nullopt;
    }
    const std::optional<std::string> &selected_lane() const
    {
        return m_selected_lane;
    }
    const std::vector<DisplayId> &selected_items() const
    {
        return m_selected_items;
    }
    const std::optional<TimeRange> &selected_range() const
    {
        return m_selected_range;
    }
    std::optional<FrameRange> selected_frames() const;
    bool is_selected(const DisplayId &id) const;

    void move_playhead(Time time)
    {
        m_playhead = snap(time);
    }
    void move_playhead_frame(Ticks frame);
    void step_playhead(int frames, bool extend_selection);
    void select_hit(const std::optional<HitResult> &hit, bool additive);
    void select_range(Time start, Time end);
    void clear_selection();
    void begin_range(Time time);
    void extend_range(Time time);
    void end_range()
    {
        m_range_anchor.reset();
    }

private:
    std::optional<Time> snap(Time time) const;

    std::optional<Time> m_start;
    std::optional<Time> m_end;
    std::optional<FrameGrid> m_frame_grid;
    std::optional<Time> m_playhead;
    std::optional<std::string> m_selected_lane;
    std::vector<DisplayId> m_selected_items;
    std::optional<TimeRange> m_selected_range;
    std::optional<Time> m_range_anchor;
};

} // namespace timeline
