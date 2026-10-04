// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/Interaction.h>
#include <timeline/Query.h>

#include <imgui.h>

#include <string_view>
#include <utility>

namespace timeline_imgui
{

/// Owned document and view-only state for one immediate-mode timeline item.
///
/// A host keeps this state between ImGui frames. Document replacement resets
/// navigation, hover, selection, dragging, and inspection; all timeline
/// semantics remain delegated to timeline-core.
///
class Control
{
public:
    Control() = default;
    explicit Control(timeline::Document document) :
        Control()
    {
        set_document(std::move(document));
    }

    void set_document(timeline::Document document);
    void zoom_in()
    {
        zoom_by(m_zoom_step);
    }
    void zoom_out()
    {
        zoom_by(1.0 / m_zoom_step);
    }
    void fit_view();
    void clear_selection();

    const std::optional<timeline::Document> &document() const
    {
        return m_document;
    }
    const std::optional<timeline::Interaction> &interaction() const
    {
        return m_interaction;
    }
    const std::optional<timeline::FrameInspection> &inspection() const
    {
        return m_inspection;
    }
    const std::optional<timeline::HitResult> &hit_result() const
    {
        return m_hit_result;
    }
    const std::optional<timeline::Layout> &layout() const
    {
        return m_layout;
    }
    const std::optional<timeline::Viewport> &viewport() const
    {
        return m_viewport;
    }
    const std::optional<timeline::LayoutMetrics> &layout_metrics() const
    {
        return m_layout_metrics;
    }

private:
    friend void draw_timeline(std::string_view id, Control &control, ImVec2 size);

    static const double m_zoom_step;

    void update_layout(ImVec2 size);
    void update_inspection();
    void update_input(timeline::Point point, bool hovered, bool active, bool focused);
    void zoom_by(double factor);
    void end_drag();

    std::optional<timeline::Document> m_document;
    std::optional<timeline::Interaction> m_interaction;
    std::optional<timeline::Navigation> m_navigation;
    std::optional<timeline::FrameInspection> m_inspection;
    std::optional<timeline::HitResult> m_hit_result;
    std::optional<timeline::Layout> m_layout;
    std::optional<timeline::Viewport> m_viewport;
    std::optional<timeline::LayoutMetrics> m_layout_metrics;
    bool m_dragging{false};
    int m_last_frame{-1};
};

/// Submits one timeline item with a positive size in ImGui screen units.
/// Call within Begin/End, using a stable unique ID for each control.
void draw_timeline(std::string_view id, Control &control, ImVec2 size);

/// Submits one timeline item using the remaining content region.
inline void draw_timeline(std::string_view id, Control &control)
{
    draw_timeline(id, control, ImGui::GetContentRegionAvail());
}

} // namespace timeline_imgui
