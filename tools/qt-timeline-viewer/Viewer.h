// Copyright (c) 2026 Richard Thomson

#pragma once

#include <filesystem>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <qtTimeline/QTimelineWidget.h>
#include <string>
#include <timelineParAnimator/TimelineJson.h>
#include <vector>

namespace timeline_qt_viewer
{

/// Native read-only host composing JSON importers and the document-owning widget.
/// Import diagnostics belong to this host, not the timeline control.
///
class Viewer : public QMainWindow
{
public:
    Viewer();
    explicit Viewer(const std::filesystem::path &startup_path);
    bool load_file(const std::filesystem::path &path)
    {
        return load_file(path, false);
    }
    bool load_file(const std::filesystem::path &path, bool append);
    bool export_snapshot(const std::filesystem::path &path);
    const std::vector<timeline_par_animator::BeatKeysMapping> &mappings() const
    {
        return m_mappings;
    }
    std::string inspector_text() const;
    QTimelineWidget &control()
    {
        return m_control;
    }
    const QTimelineWidget &control() const
    {
        return m_control;
    }
    const std::vector<std::string> &diagnostics() const
    {
        return m_diagnostics;
    }

private:
    void choose_file(bool append);
    void choose_export();
    void show_diagnostics(const QString &title);
    void update_inspector();
    QTimelineWidget &m_control;
    QPlainTextEdit &m_inspector;
    QAction &m_add;
    QAction &m_export;
    QMenu &m_view;
    std::vector<std::string> m_diagnostics;
    std::vector<timeline_par_animator::BeatKeysMapping> m_mappings;
};

} // namespace timeline_qt_viewer
