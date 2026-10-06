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
    void invalidate_layout()
    {
        m_layout.reset();
    }

    const std::optional<Document> &document() const
    {
        return m_document;
    }
    std::optional<Interaction> &interaction()
    {
        return m_interaction;
    }
    const std::optional<Interaction> &interaction() const
    {
        return m_interaction;
    }
    std::optional<Navigation> &navigation()
    {
        return m_navigation;
    }
    const std::optional<Navigation> &navigation() const
    {
        return m_navigation;
    }
    std::optional<FrameInspection> &inspection()
    {
        return m_inspection;
    }
    const std::optional<FrameInspection> &inspection() const
    {
        return m_inspection;
    }
    std::optional<HitResult> &hit_result()
    {
        return m_hit_result;
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
