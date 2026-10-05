// Copyright (c) 2026 Richard Thomson

#include <imguiTimeline/TimelineControl.h>
#include <imguiTimeline/TimelineRenderer.h>

#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <string_view>
#include <utility>

namespace timeline_imgui
{

const double Control::m_zoom_step{1.25};

void Control::set_document(timeline::Document document)
{
    m_document = std::move(document);
    m_interaction.emplace(*m_document);
    m_navigation.reset();
    m_inspection.reset();
    m_hit_result.reset();
    m_layout.reset();
    m_viewport.reset();
    m_layout_metrics.reset();
    m_dragging = false;
    m_last_frame = -1;
    if (m_document->frame_grid() && m_document->frame_grid()->frame_count() > 0)
    {
        m_interaction->move_playhead_frame(0);
        update_inspection();
    }
    const std::optional<timeline::Time> start = m_document->content_start();
    const std::optional<timeline::Time> end = m_document->content_end();
    if (start && end && *start < *end)
    {
        m_navigation.emplace(*start, *end, m_document->lane_count());
    }
}

void Control::fit_view()
{
    if (m_navigation)
    {
        m_navigation->fit();
        m_layout.reset();
    }
}

void Control::clear_selection()
{
    end_drag();
    if (m_interaction)
    {
        m_interaction->clear_selection();
        update_inspection();
        m_layout.reset();
    }
}

void Control::zoom_by(double factor)
{
    if (m_navigation && m_viewport)
    {
        const timeline::Ticks middle =
            m_viewport->start().ticks() + (m_viewport->end().ticks() - m_viewport->start().ticks()) / 2;
        m_navigation->zoom_by(factor, timeline::Time::from_ticks(middle));
        m_layout.reset();
    }
}

void Control::end_drag()
{
    if (m_interaction)
    {
        m_interaction->end_range();
    }
    m_dragging = false;
}

void Control::update_inspection()
{
    if (m_document && m_interaction && m_interaction->playhead_frame())
    {
        m_inspection = timeline::inspect_frame(*m_document, *m_interaction->playhead_frame());
    }
}

void Control::update_layout(ImVec2 size)
{
    m_layout.reset();
    m_viewport.reset();
    m_layout_metrics.reset();
    const int width = static_cast<int>(size.x);
    const int height = static_cast<int>(size.y);
    const int font_height = static_cast<int>(std::ceil(ImGui::GetFontSize()));
    const int padding = std::max(1, static_cast<int>(std::ceil(ImGui::GetStyle().FramePadding.y)));
    if (!m_navigation || width < 2 || height <= font_height + 2 * padding)
    {
        m_hit_result.reset();
        end_drag();
        return;
    }
    int label_width = 6 * font_height;
    for (const timeline::Lane &lane : m_document->lanes())
    {
        const std::string_view label = m_document->strings().lookup(lane.label());
        label_width = std::max(label_width,
            static_cast<int>(std::ceil(ImGui::CalcTextSize(label.data(), label.data() + label.size()).x)) +
                4 * padding);
    }
    label_width = std::clamp(label_width, 1, width / 2);
    m_layout_metrics.emplace(label_width, font_height + 2 * padding, font_height + 4 * padding, padding);
    timeline::Viewport viewport = m_navigation->viewport(width, height);
    m_navigation->scroll_to_lane(
        viewport.first_lane(), std::max(1, timeline::visible_lane_count(viewport, *m_layout_metrics)));
    m_viewport = m_navigation->viewport(width, height);
    m_layout.emplace(*m_document, *m_viewport, *m_layout_metrics, *m_interaction);
}

void Control::update_input(timeline::Point point, bool hovered, bool active, bool focused)
{
    if (!m_layout)
    {
        return;
    }
    const ImGuiIO &io = ImGui::GetIO();
    if (io.AppFocusLost)
    {
        end_drag();
        m_hit_result.reset();
        return;
    }
    bool interaction_changed = false;
    bool view_changed = false;
    if (hovered)
    {
        ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelX);
        ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
    }
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        m_interaction->select_hit(m_layout->hit_test(point, 3), io.KeyCtrl);
        if (point.x >= m_layout_metrics->lane_label_width())
        {
            m_interaction->begin_range(timeline::time_at_x(point.x, *m_viewport, *m_layout_metrics));
            m_dragging = true;
        }
        interaction_changed = true;
    }
    if (m_dragging)
    {
        if (active || ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            m_interaction->extend_range(timeline::time_at_x(point.x, *m_viewport, *m_layout_metrics));
            interaction_changed = true;
        }
        if (!active || !io.MouseDown[ImGuiMouseButton_Left])
        {
            end_drag();
        }
    }
    if (hovered && (io.MouseWheel != 0.0F || io.MouseWheelH != 0.0F))
    {
        if (io.KeyCtrl)
        {
            m_navigation->zoom_by(
                std::pow(m_zoom_step, io.MouseWheel), timeline::time_at_x(point.x, *m_viewport, *m_layout_metrics));
        }
        else if (io.KeyShift || io.MouseWheelH != 0.0F)
        {
            const float wheel = io.KeyShift ? io.MouseWheel : io.MouseWheelH;
            const timeline::Ticks delta =
                std::max<timeline::Ticks>(1, (m_viewport->end().ticks() - m_viewport->start().ticks()) / 10);
            m_navigation->scroll_to(timeline::Time::from_ticks(m_viewport->start().ticks() -
                static_cast<timeline::Ticks>(std::llround(static_cast<double>(wheel) * static_cast<double>(delta)))));
        }
        else
        {
            int steps = static_cast<int>(std::round(io.MouseWheel));
            if (steps == 0)
            {
                steps = io.MouseWheel < 0.0F ? -1 : 1;
            }
            m_navigation->scroll_to_lane(m_viewport->first_lane() - steps,
                std::max(1, timeline::visible_lane_count(*m_viewport, *m_layout_metrics)));
        }
        view_changed = true;
    }
    if (focused && !io.KeyCtrl && !io.KeyAlt && !io.KeySuper)
    {
        // Keep directional navigation on this item rather than moving ImGui focus.
        const ImGuiID id = ImGui::GetItemID();
        for (const ImGuiKey key : {ImGuiKey_LeftArrow, ImGuiKey_RightArrow, ImGuiKey_Escape})
        {
            ImGui::SetKeyOwner(key, id);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            clear_selection();
            interaction_changed = true;
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_RightArrow))
        {
            ImGui::SetNavCursorVisible(true);
            m_interaction->step_playhead(ImGui::IsKeyPressed(ImGuiKey_LeftArrow) ? -1 : 1, io.KeyShift);
            if (m_interaction->playhead())
            {
                m_navigation->reveal(*m_interaction->playhead());
            }
            interaction_changed = true;
        }
    }
    if (interaction_changed)
    {
        update_inspection();
    }
    if (interaction_changed || view_changed)
    {
        const ImVec2 size(static_cast<float>(m_viewport->width()), static_cast<float>(m_viewport->height()));
        update_layout(size);
    }
    m_hit_result = hovered ? m_layout->hit_test(point, 3) : std::nullopt;
    if (hovered && !interaction_changed && m_document->frame_grid() && point.x >= m_layout_metrics->lane_label_width())
    {
        const std::optional<timeline::Ticks> frame =
            m_document->frame_grid()->nearest_frame(timeline::time_at_x(point.x, *m_viewport, *m_layout_metrics));
        if (frame && (!m_inspection || m_inspection->frame != *frame))
        {
            m_inspection = timeline::inspect_frame(*m_document, *frame);
        }
    }
}

void draw_timeline(std::string_view id, Control &control, ImVec2 size)
{
    size.x = std::max(1.0F, size.x);
    size.y = std::max(1.0F, size.y);
    const char *begin = id.empty() ? "" : id.data();
    ImGui::PushID(begin, begin + id.size());
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("timeline", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_EnableNav);
    const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_NoNavOverride);
    const bool active = ImGui::IsItemActive();
    const bool focused = ImGui::IsItemFocused();
    if (control.m_last_frame != ImGui::GetFrameCount() - 1 || !ImGui::IsItemVisible())
    {
        control.end_drag();
    }
    control.m_last_frame = ImGui::GetFrameCount();
    control.update_layout(size);
    if (!ImGui::IsItemVisible())
    {
        control.m_hit_result.reset();
        ImGui::PopID();
        return;
    }
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const timeline::Point point = hovered || active || control.m_dragging
        ? timeline::Point{static_cast<int>(std::clamp(mouse.x - origin.x, -size.x, size.x)),
              static_cast<int>(std::clamp(mouse.y - origin.y, -size.y, size.y))}
        : timeline::Point{-1, -1};
    control.update_input(point, hovered, active, focused);
    ImGui::PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);
    ImDrawList &draw_list = *ImGui::GetWindowDrawList();
    if (control.m_layout)
    {
        draw_display_list(draw_list, control.m_layout->display_list(), origin, ImGui::GetStyle(), focused,
            control.m_layout_metrics->lane_label_width());
    }
    else if (!control.m_navigation)
    {
        draw_list.AddText(origin, ImGui::GetColorU32(ImGuiCol_Text),
            control.m_document ? "No timeline content." : "No timeline loaded.");
    }
    ImGui::PopClipRect();
    ImGui::PopID();
}

} // namespace timeline_imgui
