// Copyright (c) 2026 Richard Thomson

#include <Viewer.h>

#include <timelineViewer/format_inspector.h>
#include <timelineViewer/load_timeline.h>
#include <timelineViewer/write_snapshot.h>

#include <timeline/size_cast.h>

#include <QAction>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QScrollBar>
#include <QSplitter>
#include <QStatusBar>

#include <string_view>
#include <utility>

namespace timeline_qt_viewer
{

namespace
{

std::filesystem::path native_path(const QString &path)
{
#ifdef _WIN32
    return std::filesystem::path(path.toStdWString());
#else
    return std::filesystem::u8path(path.toStdString());
#endif
}

QString to_qt_string(std::string_view value)
{
    return QString::fromLatin1(value.data(), timeline::size_cast(value));
}

} // namespace

Viewer::Viewer() :
    Viewer(std::filesystem::path{})
{
}

Viewer::Viewer(const std::filesystem::path &startup_path) :
    m_control(*new QTimelineWidget(*this)),
    m_inspector(*new QPlainTextEdit(this)),
    m_add(*new QAction(QStringLiteral("&Add comparison..."), this)),
    m_export(*new QAction(QStringLiteral("Export &Snapshot..."), this)),
    m_view(*new QMenu(QStringLiteral("&View"), this))
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
    connect(&open, &QAction::triggered, this, [this] { choose_file(false); });
    m_add.setObjectName(QStringLiteral("add_comparison"));
    m_add.setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+O")));
    connect(&m_add, &QAction::triggered, this, [this] { choose_file(true); });
    file.addAction(&m_add);
    m_export.setObjectName(QStringLiteral("export_snapshot"));
    connect(&m_export, &QAction::triggered, this, [this] { choose_export(); });
    file.addAction(&m_export);
    connect(&file, &QMenu::aboutToShow, this, [this] { update_inspector(); });
    file.addSeparator();
    QAction &exit = *file.addAction(QStringLiteral("E&xit"));
    connect(&exit, &QAction::triggered, this, &QWidget::close);
    menuBar()->addMenu(&m_view);
    connect(&m_view, &QMenu::aboutToShow, this, [this] { update_inspector(); });
    QAction &zoom_in = *m_view.addAction(QStringLiteral("Zoom &In"));
    zoom_in.setObjectName(QStringLiteral("zoom_in"));
    zoom_in.setShortcut(QKeySequence::ZoomIn);
    connect(&zoom_in, &QAction::triggered, &m_control, &QTimelineWidget::zoom_in);
    QAction &zoom_out = *m_view.addAction(QStringLiteral("Zoom &Out"));
    zoom_out.setObjectName(QStringLiteral("zoom_out"));
    zoom_out.setShortcut(QKeySequence::ZoomOut);
    connect(&zoom_out, &QAction::triggered, &m_control, &QTimelineWidget::zoom_out);
    QAction &fit = *m_view.addAction(QStringLiteral("&Fit"));
    fit.setObjectName(QStringLiteral("fit_view"));
    fit.setShortcut(QKeySequence(QStringLiteral("Ctrl+0")));
    connect(&fit, &QAction::triggered, &m_control, &QTimelineWidget::fit_view);
    m_view.addSeparator();
    QAction &clear = *m_view.addAction(QStringLiteral("&Clear Selection"));
    clear.setObjectName(QStringLiteral("clear_selection"));
    clear.setShortcut(QKeySequence(Qt::Key_Escape));
    connect(&clear, &QAction::triggered, &m_control, &QTimelineWidget::clear_selection);
    connect(&m_control, &QTimelineWidget::inspection_changed, this, [this] { update_inspector(); });
    statusBar()->showMessage(QStringLiteral("No timeline loaded"));
    update_inspector();
    if (!startup_path.empty())
    {
        load_file(startup_path);
    }
}

bool Viewer::load_file(const std::filesystem::path &path, bool append)
{
    timeline_viewer::LoadResult result = timeline_viewer::load_timeline(path, append, m_control.document(), m_mappings);
    m_diagnostics = std::move(result.diagnostics);
    if (!result.succeeded())
    {
        statusBar()->showMessage(result.outcome == timeline_viewer::LoadOutcome::COMPOSITION_FAILED
                ? QStringLiteral("Unable to add timeline")
                : QStringLiteral("Timeline import failed"));
        return false;
    }
    m_mappings = std::move(result.mappings);
    m_control.set_document(std::move(*result.document));
    const QString name = QString::fromStdString(path.filename().u8string());
    const timeline::Document &document = *m_control.document();
    setWindowTitle(
        QStringLiteral("Qt Timeline Viewer - ") + to_qt_string(document.strings().lookup(document.metadata().title())));
    statusBar()->showMessage(QStringLiteral("Loaded ") + name);
    return true;
}

bool Viewer::export_snapshot(const std::filesystem::path &path)
{
    m_diagnostics.clear();
    if (!m_control.layout())
    {
        m_diagnostics.emplace_back("No timeline layout is available to export.");
        statusBar()->showMessage(QStringLiteral("Snapshot export failed"));
        return false;
    }
    timeline_viewer::SnapshotWriteResult result = timeline_viewer::write_snapshot(path, m_control.snapshot());
    m_diagnostics = std::move(result.diagnostics);
    if (!result.succeeded())
    {
        statusBar()->showMessage(QStringLiteral("Snapshot export failed"));
        return false;
    }
    statusBar()->showMessage(QStringLiteral("Exported ") + QString::fromStdString(path.filename().u8string()));
    return true;
}

std::string Viewer::inspector_text() const
{
    return timeline_viewer::format_inspector(
        m_control.document(), m_control.hit_result(), m_control.interaction(), m_control.inspection(), m_mappings);
}

void Viewer::choose_file(bool append)
{
    const QString path = QFileDialog::getOpenFileName(this,
        append ? QStringLiteral("Add comparison JSON") : QStringLiteral("Open timeline JSON"), {},
        QStringLiteral("JSON files (*.json)"));
    if (path.isEmpty())
    {
        return;
    }
    load_file(native_path(path), append);
    show_diagnostics(QStringLiteral("Import diagnostics"));
}

void Viewer::choose_export()
{
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export timeline snapshot"),
        QStringLiteral("timeline.txt"), QStringLiteral("Text snapshots (*.txt)"));
    if (!path.isEmpty())
    {
        export_snapshot(native_path(path));
        show_diagnostics(QStringLiteral("Snapshot diagnostics"));
    }
}

void Viewer::show_diagnostics(const QString &title)
{
    if (!m_diagnostics.empty())
    {
        QStringList messages;
        for (const std::string &message : m_diagnostics)
        {
            messages.append(QString::fromStdString(message));
        }
        QMessageBox::warning(this, title, messages.join('\n'));
    }
}

void Viewer::update_inspector()
{
    m_add.setEnabled(m_control.document().has_value());
    m_export.setEnabled(m_control.layout().has_value());
    m_view.setEnabled(m_control.document().has_value());
    for (QAction *action : m_view.actions())
    {
        action->setEnabled(m_control.layout().has_value());
    }
    const int scroll = m_inspector.verticalScrollBar()->value();
    m_inspector.setPlainText(QString::fromStdString(inspector_text()));
    m_inspector.verticalScrollBar()->setValue(scroll);
}

} // namespace timeline_qt_viewer
