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
    m_state.set_document(std::move(document));
    m_dragging = false;
    m_last_frame = -1;
}

void Control::fit_view()
{
    if (m_state.navigation())
    {
        m_state.navigation()->fit();
        m_state.invalidate_layout();
    }
}

void Control::clear_selection()
{
    end_drag();
    if (m_state.interaction())
    {
        m_state.interaction()->clear_selection();
        update_inspection();
        m_state.invalidate_layout();
    }
}

void Control::zoom_by(double factor)
{
    if (m_state.navigation() && m_state.viewport())
    {
        const timeline::Ticks middle = m_state.viewport()->start().ticks() +
            (m_state.viewport()->end().ticks() - m_state.viewport()->start().ticks()) / 2;
        m_state.navigation()->zoom_by(factor, timeline::Time::from_ticks(middle));
        m_state.invalidate_layout();
    }
}

void Control::end_drag()
{
    if (m_state.interaction())
    {
        m_state.interaction()->end_range();
    }
    m_dragging = false;
}

void Control::update_inspection()
{
    if (m_state.document() && m_state.interaction() && m_state.interaction()->playhead_frame())
    {
        m_state.inspection() = timeline::inspect_frame(*m_state.document(), *m_state.interaction()->playhead_frame());
    }
}

void Control::update_layout(ImVec2 size)
{
    const int width = static_cast<int>(size.x);
    const int height = static_cast<int>(size.y);
    const int font_height = static_cast<int>(std::ceil(ImGui::GetFontSize()));
    const int padding = std::max(1, static_cast<int>(std::ceil(ImGui::GetStyle().FramePadding.y)));
    if (!m_state.navigation() || width < 2 || height <= font_height + 2 * padding)
    {
        m_state.clear_layout();
        m_state.hit_result().reset();
        end_drag();
        return;
    }
    int label_width = 6 * font_height;
    for (const timeline::Lane &lane : m_state.document()->lanes())
    {
        const std::string_view label = m_state.document()->strings().lookup(lane.label());
        label_width = std::max(label_width,
            static_cast<int>(std::ceil(ImGui::CalcTextSize(label.data(), label.data() + label.size()).x)) +
                4 * padding);
    }
    label_width = std::clamp(label_width, 1, width / 2);
    m_state.rebuild_layout(width, height,
        timeline::LayoutMetrics(label_width, font_height + 2 * padding, font_height + 4 * padding, padding));
}

void Control::update_input(timeline::Point point, bool hovered, bool active, bool focused)
{
    if (!m_state.layout())
    {
        return;
    }
    const ImGuiIO &io = ImGui::GetIO();
    if (io.AppFocusLost)
    {
        end_drag();
        m_state.hit_result().reset();
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
        m_state.interaction()->select_hit(m_state.layout()->hit_test(point, 3), io.KeyCtrl);
        if (point.x >= m_state.layout_metrics()->lane_label_width())
        {
            m_state.interaction()->begin_range(
                timeline::time_at_x(point.x, *m_state.viewport(), *m_state.layout_metrics()));
            m_dragging = true;
        }
        interaction_changed = true;
    }
    if (m_dragging)
    {
        if (active || ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            m_state.interaction()->extend_range(
                timeline::time_at_x(point.x, *m_state.viewport(), *m_state.layout_metrics()));
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
            m_state.navigation()->zoom_by(std::pow(m_zoom_step, io.MouseWheel),
                timeline::time_at_x(point.x, *m_state.viewport(), *m_state.layout_metrics()));
        }
        else if (io.KeyShift || io.MouseWheelH != 0.0F)
        {
            const float wheel = io.KeyShift ? io.MouseWheel : io.MouseWheelH;
            const timeline::Ticks delta = std::max<timeline::Ticks>(
                1, (m_state.viewport()->end().ticks() - m_state.viewport()->start().ticks()) / 10);
            m_state.navigation()->scroll_to(timeline::Time::from_ticks(m_state.viewport()->start().ticks() -
                static_cast<timeline::Ticks>(std::llround(static_cast<double>(wheel) * static_cast<double>(delta)))));
        }
        else
        {
            int steps = static_cast<int>(std::round(io.MouseWheel));
            if (steps == 0)
            {
                steps = io.MouseWheel < 0.0F ? -1 : 1;
            }
            m_state.navigation()->scroll_to_lane(m_state.viewport()->first_lane() - steps,
                std::max(1, timeline::visible_lane_count(*m_state.viewport(), *m_state.layout_metrics())));
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
            m_state.interaction()->step_playhead(ImGui::IsKeyPressed(ImGuiKey_LeftArrow) ? -1 : 1, io.KeyShift);
            if (m_state.interaction()->playhead())
            {
                m_state.navigation()->reveal(*m_state.interaction()->playhead());
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
        const ImVec2 size(
            static_cast<float>(m_state.viewport()->width()), static_cast<float>(m_state.viewport()->height()));
        update_layout(size);
    }
    m_state.hit_result() = hovered ? m_state.layout()->hit_test(point, 3) : std::nullopt;
    if (hovered && !interaction_changed && m_state.document()->frame_grid() &&
        point.x >= m_state.layout_metrics()->lane_label_width())
    {
        const std::optional<timeline::Ticks> frame = m_state.document()->frame_grid()->nearest_frame(
            timeline::time_at_x(point.x, *m_state.viewport(), *m_state.layout_metrics()));
        if (frame && (!m_state.inspection() || m_state.inspection()->frame != *frame))
        {
            m_state.inspection() = timeline::inspect_frame(*m_state.document(), *frame);
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
        control.m_state.hit_result().reset();
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
    if (control.m_state.layout())
    {
        draw_display_list(draw_list, control.m_state.layout()->display_list(), origin, ImGui::GetStyle(), focused,
            control.m_state.layout_metrics()->lane_label_width());
    }
    else if (!control.m_state.navigation())
    {
        draw_list.AddText(origin, ImGui::GetColorU32(ImGuiCol_Text),
            control.m_state.document() ? "No timeline content." : "No timeline loaded.");
    }
    ImGui::PopClipRect();
    ImGui::PopID();
}

} // namespace timeline_imgui
