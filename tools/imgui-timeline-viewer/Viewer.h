// Copyright (c) 2026 Richard Thomson

#pragma once

#include <imguiTimeline/TimelineControl.h>
#include <timelineParAnimator/TimelineJson.h>

namespace timeline_imgui_viewer
{

/// Viewer-owned import recipes, diagnostics, and a document-owning control.
/// Failed imports preserve the previously displayed document and interaction.
///
class Viewer
{
public:
    bool load_file(const std::filesystem::path &path, bool append);
    bool export_snapshot(const std::filesystem::path &path);
    std::string inspector_text() const;
    timeline_imgui::Control &control()
    {
        return m_control;
    }
    const timeline_imgui::Control &control() const
    {
        return m_control;
    }
    const std::vector<timeline_par_animator::BeatKeysMapping> &mappings() const
    {
        return m_mappings;
    }
    const std::vector<std::string> &diagnostics() const
    {
        return m_diagnostics;
    }
    const std::string &status() const
    {
        return m_status;
    }

private:
    timeline_imgui::Control m_control;
    std::vector<timeline_par_animator::BeatKeysMapping> m_mappings;
    std::vector<std::string> m_diagnostics;
    std::string m_status{"No timeline loaded"};
};

/// File and application commands requested by the immediate-mode menu.
enum class Command
{
    NONE,
    OPEN,
    ADD,
    EXPORT,
    EXIT
};

/// Draws the viewer and returns a host command without invoking native dialogs.
Command draw_viewer(Viewer &viewer, bool dialog_pending);

} // namespace timeline_imgui_viewer
