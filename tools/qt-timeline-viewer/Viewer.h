// Copyright (c) 2026 Richard Thomson

#pragma once

#include <filesystem>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <qtTimeline/QTimelineWidget.h>
#include <string>
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
    bool load_file(const std::filesystem::path &path);
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
    void choose_file();
    void update_inspector();
    QTimelineWidget &m_control;
    QPlainTextEdit &m_inspector;
    std::vector<std::string> m_diagnostics;
};

} // namespace timeline_qt_viewer
