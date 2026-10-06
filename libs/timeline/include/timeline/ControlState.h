// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/Interaction.h>
#include <timeline/Query.h>

#include <optional>
#include <utility>

namespace timeline
{

/// Owned document and toolkit-neutral display state for a timeline control.
///
/// Replacing the document creates fresh interaction and navigation state.
/// Layout rebuilding consumes host dimensions and toolkit-neutral metrics
/// without retaining native GUI objects.
///
class ControlState
{
public:
    ControlState() = default;
    explicit ControlState(Document document) :
        ControlState()
    {
        set_document(std::move(document));
    }

    void set_document(Document document);
    bool rebuild_layout(int width, int height, LayoutMetrics metrics);
    void clear_layout();
    void fit_view();
    void zoom_by(double factor);
    void zoom_at(double factor, int x);
    void scroll_by(Duration distance);
    void scroll_to_fraction(double fraction);
    void scroll_lanes(int lanes);
    bool begin_selection(Point point, int hit_tolerance, bool additive);
    void extend_range(Point point);
    void end_range();
    void clear_selection();
    void step_playhead(int frames, bool extend_selection);
    void hover_at(Point point, int hit_tolerance, bool inspect_frame);
    void clear_hover();

    const std::optional<Document> &document() const
    {
        return m_document;
    }
    const std::optional<Interaction> &interaction() const
    {
        return m_interaction;
    }
    const std::optional<Navigation> &navigation() const
    {
        return m_navigation;
    }
    const std::optional<FrameInspection> &inspection() const
    {
        return m_inspection;
    }
    const std::optional<HitResult> &hit_result() const
    {
        return m_hit_result;
    }
    const std::optional<Layout> &layout() const
    {
        return m_layout;
    }
    const std::optional<Viewport> &viewport() const
    {
        return m_viewport;
    }
    const std::optional<LayoutMetrics> &layout_metrics() const
    {
        return m_layout_metrics;
    }

private:
    void invalidate_layout()
    {
        m_layout.reset();
    }
    void rebuild_current_layout();
    void update_inspection();

    std::optional<Document> m_document;
    std::optional<Interaction> m_interaction;
    std::optional<Navigation> m_navigation;
    std::optional<FrameInspection> m_inspection;
    std::optional<HitResult> m_hit_result;
    std::optional<Layout> m_layout;
    std::optional<Viewport> m_viewport;
    std::optional<LayoutMetrics> m_layout_metrics;
};

} // namespace timeline
