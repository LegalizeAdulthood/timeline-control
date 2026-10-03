// Copyright (c) 2026 Richard Thomson

#include "Viewer.h"

#include <locale>
#include <QFileDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QScrollBar>
#include <QSplitter>
#include <QStatusBar>
#include <sstream>
#include <system_error>
#include <timelineParAnimator/TimelineJson.h>

namespace timeline_qt_viewer
{

Viewer::Viewer() :
    m_control(*new QTimelineWidget(*this)),
    m_inspector(*new QPlainTextEdit(this))
{
    setWindowTitle(QStringLiteral("Qt Timeline Viewer"));
    QSplitter &splitter = *new QSplitter(this);
    splitter.addWidget(&m_control);
    splitter.addWidget(&m_inspector);
    splitter.setStretchFactor(0, 2);
    splitter.setStretchFactor(1, 1);
    setCentralWidget(&splitter);
    m_inspector.setReadOnly(true);
    m_inspector.setLineWrapMode(QPlainTextEdit::WidgetWidth);
    QMenu &file = *menuBar()->addMenu(QStringLiteral("&File"));
    QAction &open = *file.addAction(QStringLiteral("&Open..."));
    open.setShortcut(QKeySequence::Open);
    connect(&open, &QAction::triggered, this, [this] { choose_file(); });
    file.addSeparator();
    QAction &exit = *file.addAction(QStringLiteral("E&xit"));
    connect(&exit, &QAction::triggered, this, &QWidget::close);
    connect(&m_control, &QTimelineWidget::inspection_changed, this, [this] { update_inspector(); });
    statusBar()->showMessage(QStringLiteral("No timeline loaded"));
    update_inspector();
}
bool Viewer::load_file(const std::filesystem::path &path)
{
    timeline_par_animator::JsonImportOptions options;
    const std::filesystem::path companion = path.parent_path() / "adapter.beat-keys.json";
    std::error_code error;
    if (std::filesystem::is_regular_file(companion, error))
    {
        options.beat_keys_config_path = companion;
    }
    timeline_par_animator::JsonImportResult imported = timeline_par_animator::import_timeline_json(path, options);
    m_diagnostics = std::move(imported.diagnostics);
    if (!imported.succeeded())
    {
        statusBar()->showMessage(QStringLiteral("Timeline import failed"));
        return false;
    }
    m_control.set_document(std::move(*imported.document));
    const QString name = QString::fromUtf8(path.filename().u8string().c_str());
    setWindowTitle(QStringLiteral("Qt Timeline Viewer - ") + name);
    statusBar()->showMessage(QStringLiteral("Loaded ") + name);
    return true;
}
std::string Viewer::inspector_text() const
{
    if (!m_control.document())
    {
        return "No frame inspection.";
    }
    const timeline::Document &document = *m_control.document();
    std::ostringstream text;
    text.imbue(std::locale::classic());
    text << "Title: " << document.metadata().title() << "\nLanes: " << document.lane_count();
    if (m_control.hit_result())
    {
        const timeline::DisplayId &id = m_control.hit_result()->id;
        text << "\nHit lane: " << id.lane_id << "\nHit item: " << id.item_id;
    }
    if (m_control.interaction())
    {
        const timeline::Interaction &interaction = *m_control.interaction();
        if (interaction.playhead_frame())
        {
            text << "\nPlayhead frame: " << *interaction.playhead_frame();
        }
        if (interaction.selected_lane())
        {
            text << "\nSelected lane: " << *interaction.selected_lane();
        }
        for (const timeline::DisplayId &id : interaction.selected_items())
        {
            text << "\nSelected item: " << id.lane_id << '/' << id.item_id;
        }
        if (interaction.selected_frames())
        {
            text << "\nSelected frames: " << interaction.selected_frames()->first() << " to "
                 << interaction.selected_frames()->last();
        }
    }
    if (!m_control.inspection())
    {
        text << "\nNo frame inspection.";
        return text.str();
    }
    const timeline::FrameInspection &inspection = *m_control.inspection();
    text << "\n\nFrame: " << inspection.frame << "\nTime: " << document.timebase().seconds(inspection.time)
         << " seconds";
    for (const timeline::LaneInspection &lane : inspection.lanes)
    {
        text << "\n\n" << lane.label << " [" << lane.kind << "]\nSource items: " << lane.item_count;
        if (lane.value)
        {
            text << "\nFrame value: " << *lane.value;
        }
        for (const timeline::InspectionItem &item : lane.items)
        {
            text << "\nItem: " << item.id << " [" << item.kind << ']';
            if (item.value)
            {
                text << " Value: " << *item.value;
            }
        }
    }
    return text.str();
}
void Viewer::choose_file()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open timeline JSON"), {}, QStringLiteral("JSON files (*.json)"));
    if (path.isEmpty())
    {
        return;
    }
#ifdef _WIN32
    load_file(std::filesystem::path(path.toStdWString()));
#else
    load_file(std::filesystem::u8path(path.toStdString()));
#endif
    if (!m_diagnostics.empty())
    {
        QStringList messages;
        for (const std::string &message : m_diagnostics)
        {
            messages.append(QString::fromUtf8(message.c_str()));
        }
        QMessageBox::warning(this, QStringLiteral("Import diagnostics"), messages.join('\n'));
    }
}
void Viewer::update_inspector()
{
    const int scroll = m_inspector.verticalScrollBar()->value();
    m_inspector.setPlainText(QString::fromUtf8(inspector_text().c_str()));
    m_inspector.verticalScrollBar()->setValue(scroll);
}

} // namespace timeline_qt_viewer
