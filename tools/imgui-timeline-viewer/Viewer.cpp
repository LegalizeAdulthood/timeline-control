// Copyright (c) 2026 Richard Thomson

#include <Viewer.h>

#include <timelineViewer/format_inspector.h>
#include <timelineViewer/load_timeline.h>
#include <timelineViewer/write_snapshot.h>

#include <timeline/Snapshot.h>

#include <algorithm>
#include <utility>

namespace timeline_imgui_viewer
{

bool Viewer::load_file(const std::filesystem::path &path, bool append)
{
    timeline_viewer::LoadResult result = timeline_viewer::load_timeline(path, append, m_control.document(), m_mappings);
    m_diagnostics = std::move(result.diagnostics);
    if (!result.succeeded())
    {
        m_status = result.outcome == timeline_viewer::LoadOutcome::COMPOSITION_FAILED ? "Unable to add timeline"
                                                                                      : "Timeline import failed";
        return false;
    }
    m_mappings = std::move(result.mappings);
    m_control.set_document(std::move(*result.document));
    m_status = "Loaded " + path.filename().u8string();
    return true;
}

bool Viewer::export_snapshot(const std::filesystem::path &path)
{
    m_diagnostics.clear();
    if (!m_control.layout())
    {
        m_diagnostics.emplace_back("No timeline layout is available to export.");
        return false;
    }
    const std::string snapshot = timeline::render_snapshot(m_control.layout()->display_list());
    timeline_viewer::SnapshotWriteResult result = timeline_viewer::write_snapshot(path, snapshot);
    m_diagnostics = std::move(result.diagnostics);
    if (!result.succeeded())
    {
        m_status = "Snapshot export failed";
        return false;
    }
    m_status = "Exported " + path.filename().u8string();
    return true;
}

std::string Viewer::inspector_text() const
{
    return timeline_viewer::format_inspector(
        m_control.document(), m_control.hit_result(), m_control.interaction(), m_control.inspection(), m_mappings);
}
Command draw_viewer(Viewer &viewer, bool dialog_pending)
{
    Command command = Command::NONE;
    const ImGuiViewport &viewport = *ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport.WorkPos);
    ImGui::SetNextWindowSize(viewport.WorkSize);
    ImGui::Begin("Timeline Viewer", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollWithMouse);
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File", !dialog_pending))
        {
            if (ImGui::MenuItem("Open...", "Ctrl+O"))
            {
                command = Command::OPEN;
            }
            if (ImGui::MenuItem("Add comparison...", "Ctrl+Shift+O", false, viewer.control().document().has_value()))
            {
                command = Command::ADD;
            }
            if (ImGui::MenuItem("Export Snapshot...", nullptr, false, viewer.control().layout().has_value()))
            {
                command = Command::EXPORT;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit"))
            {
                command = Command::EXIT;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View", viewer.control().document().has_value() && !dialog_pending))
        {
            if (ImGui::MenuItem("Zoom In"))
            {
                viewer.control().zoom_in();
            }
            if (ImGui::MenuItem("Zoom Out"))
            {
                viewer.control().zoom_out();
            }
            if (ImGui::MenuItem("Fit"))
            {
                viewer.control().fit_view();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Clear Selection"))
            {
                viewer.control().clear_selection();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    if (!dialog_pending && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_RouteGlobal))
    {
        command = Command::OPEN;
    }
    if (!dialog_pending && viewer.control().document() &&
        ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_O, ImGuiInputFlags_RouteGlobal))
    {
        command = Command::ADD;
    }
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float status_height = ImGui::GetFrameHeightWithSpacing();
    const bool narrow = available.x < 720.0F;
    const float content_height = std::max(1.0F, available.y - status_height);
    const ImVec2 timeline_size(
        narrow ? available.x : std::max(1.0F, available.x - 340.0F), narrow ? content_height * 0.55F : content_height);
    ImGui::BeginDisabled(dialog_pending);
    if (ImGui::BeginChild("timeline", timeline_size, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollWithMouse))
    {
        timeline_imgui::draw_timeline("control", viewer.control());
    }
    ImGui::EndChild();
    ImGui::EndDisabled();
    if (!narrow)
    {
        ImGui::SameLine();
    }
    const ImVec2 inspector_size(narrow ? available.x : 0.0F,
        narrow ? std::max(1.0F, content_height - timeline_size.y - ImGui::GetStyle().ItemSpacing.y) : content_height);
    if (ImGui::BeginChild("inspector", inspector_size))
    {
        ImGui::PushTextWrapPos(0.0F);
        const std::string text = viewer.inspector_text();
        ImGui::TextUnformatted(text.c_str());
        ImGui::PopTextWrapPos();
    }
    ImGui::EndChild();
    ImGui::TextUnformatted(viewer.status().c_str());
    ImGui::End();
    return command;
}

} // namespace timeline_imgui_viewer
