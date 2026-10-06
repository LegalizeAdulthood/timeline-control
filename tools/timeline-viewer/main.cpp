// Copyright (c) 2026 Richard Thomson

#include <wxTimeline/config.h>

#include <timelineViewer/format_inspector.h>
#include <timelineViewer/load_timeline.h>
#include <timelineViewer/write_snapshot.h>

#include <timelineParAnimator/BeatKeysMapping.h>

#include <timeline/size_cast.h>

#ifdef TIMELINE_CONTROL_WITH_CAIRO
#include <wxTimeline/wxCairoTimeline.h>
#endif
#include <wxTimeline/wxTimelineControl.h>

#include <wx/filedlg.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <wx/wx.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace
{

#ifdef TIMELINE_CONTROL_WITH_CAIRO
constexpr const char VIEWER_TITLE[] = "Cairo Timeline Viewer";
#else
constexpr const char VIEWER_TITLE[] = "Timeline Viewer";
#endif

wxString to_wx_string(std::string_view value)
{
    return wxString(value.data(), value.size());
}

std::filesystem::path native_path(const wxString &path)
{
    return std::filesystem::path(path.ToStdWstring());
}

wxString filename(const std::filesystem::path &path)
{
    return wxString(path.filename().wstring());
}

bool smoke_failure(std::string_view message)
{
    std::cerr << message << '\n';
    return false;
}

} // namespace

/// Main window that composes JSON adapters with the wx timeline control.
///
class TimelineViewerFrame : public wxFrame
{
public:
    TimelineViewerFrame();
    bool load_file(const std::filesystem::path &path, bool append);
    bool export_snapshot(const std::filesystem::path &path);
    const std::optional<timeline::Document> &document() const
    {
        return m_timeline_control->document();
    }
    const std::vector<timeline_par_animator::BeatKeysMapping> &mappings() const
    {
        return m_mappings;
    }
    const std::vector<std::string> &diagnostics() const
    {
        return m_diagnostics;
    }

private:
    void on_inspection_changed(wxCommandEvent &event);
    void on_open(wxCommandEvent &event);
    void on_add(wxCommandEvent &event);
    void choose_file(bool append);
    void on_export_snapshot(wxCommandEvent &event);
    void on_exit(wxCommandEvent &event);
    void on_zoom_in(wxCommandEvent &event);
    void on_zoom_out(wxCommandEvent &event);
    void on_fit_view(wxCommandEvent &event);
    void on_clear_selection(wxCommandEvent &event);
    void show_diagnostics(const std::vector<std::string> &diagnostics, const wxString &title, long dialog_style);

    wxPanel *m_content;
    wxTimelineControl *m_timeline_control;
    wxTextCtrl *m_inspector;
    std::vector<timeline_par_animator::BeatKeysMapping> m_mappings;
    std::vector<std::string> m_diagnostics;
    timeline_viewer::LoadOutcome m_load_outcome{timeline_viewer::LoadOutcome::IMPORT_FAILED};
};

/// wxWidgets application for manually exercising the timeline control.
///
class TimelineViewerApp : public wxApp
{
public:
    bool OnInit() override;
    int OnRun() override;

private:
    bool run_smoke_test();

    TimelineViewerFrame *m_frame{};
    bool m_smoke_test{false};
    std::filesystem::path m_replacement_path;
    std::filesystem::path m_comparison_path;
    std::filesystem::path m_invalid_path;
    std::filesystem::path m_snapshot_path;
};

TimelineViewerFrame::TimelineViewerFrame() :
    wxFrame(nullptr, wxID_ANY, VIEWER_TITLE, wxDefaultPosition, wxSize(800, 500)),
    m_content(new wxPanel(this)),
#ifdef TIMELINE_CONTROL_WITH_CAIRO
    m_timeline_control(new wxCairoTimeline(m_content)),
#else
    m_timeline_control(new wxTimelineControl(m_content)),
#endif
    m_inspector(new wxTextCtrl(m_content, wxID_ANY, "No frame inspection.", wxDefaultPosition, wxSize(280, -1),
        wxTE_MULTILINE | wxTE_READONLY))
{
    wxMenu *file_menu = new wxMenu;
    file_menu->Append(wxID_OPEN, "&Open...\tCtrl+O");
    file_menu->Append(wxID_ADD, "&Add...\tCtrl+Shift+O");
    file_menu->Append(wxID_SAVEAS, "Export &Snapshot...");
    file_menu->AppendSeparator();
    file_menu->Append(wxID_EXIT, "E&xit");

    wxMenu *view_menu = new wxMenu;
    view_menu->Append(wxID_ZOOM_IN, "Zoom &In\tCtrl++");
    view_menu->Append(wxID_ZOOM_OUT, "Zoom &Out\tCtrl+-");
    view_menu->Append(wxID_ZOOM_100, "&Fit\tCtrl+0");
    view_menu->AppendSeparator();
    view_menu->Append(wxID_CLEAR, "&Clear Selection\tEsc");
#ifdef TIMELINE_CONTROL_WITH_CAIRO
    wxMenu &renderer_menu = *new wxMenu;
    const int native_renderer = wxWindow::NewControlId();
    const int cairo_renderer = wxWindow::NewControlId();
    renderer_menu.AppendRadioItem(native_renderer, "&Native wx");
    renderer_menu.AppendRadioItem(cairo_renderer, "&Cairo");
    renderer_menu.Check(cairo_renderer, true);
    view_menu->AppendSubMenu(&renderer_menu, "&Renderer");
    Bind(
        wxEVT_MENU, [this](wxCommandEvent &)
        { static_cast<wxCairoTimeline &>(*m_timeline_control).set_cairo_enabled(false); }, native_renderer);
    Bind(
        wxEVT_MENU, [this](wxCommandEvent &)
        { static_cast<wxCairoTimeline &>(*m_timeline_control).set_cairo_enabled(true); }, cairo_renderer);
#endif

    wxMenuBar *menu_bar = new wxMenuBar;
    menu_bar->Append(file_menu, "&File");
    menu_bar->Append(view_menu, "&View");
    wxFrame::SetMenuBar(menu_bar);
    wxFrame::CreateStatusBar();
    wxFrame::SetStatusText("No timeline loaded");

    wxBoxSizer *content = new wxBoxSizer(wxHORIZONTAL);
    content->Add(m_timeline_control, 1, wxEXPAND);
    content->Add(m_inspector, 0, wxEXPAND | wxLEFT, 1);
    m_content->SetSizer(content);
    wxBoxSizer *frame_content = new wxBoxSizer(wxVERTICAL);
    frame_content->Add(m_content, 1, wxEXPAND);
    SetSizer(frame_content);

    m_timeline_control->Bind(wxEVT_TIMELINE_INSPECTION_CHANGED, &TimelineViewerFrame::on_inspection_changed, this);
    Bind(wxEVT_MENU, &TimelineViewerFrame::on_open, this, wxID_OPEN);
    Bind(wxEVT_MENU, &TimelineViewerFrame::on_add, this, wxID_ADD);
    Bind(
        wxEVT_UPDATE_UI, [this](wxUpdateUIEvent &event) { event.Enable(m_timeline_control->has_document()); },
        wxID_ADD);
    Bind(wxEVT_MENU, &TimelineViewerFrame::on_export_snapshot, this, wxID_SAVEAS);
    Bind(
        wxEVT_UPDATE_UI, [this](wxUpdateUIEvent &event) { event.Enable(m_timeline_control->has_document()); },
        wxID_SAVEAS);
    Bind(wxEVT_MENU, &TimelineViewerFrame::on_exit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &TimelineViewerFrame::on_zoom_in, this, wxID_ZOOM_IN);
    Bind(wxEVT_MENU, &TimelineViewerFrame::on_zoom_out, this, wxID_ZOOM_OUT);
    Bind(wxEVT_MENU, &TimelineViewerFrame::on_fit_view, this, wxID_ZOOM_100);
    Bind(wxEVT_MENU, &TimelineViewerFrame::on_clear_selection, this, wxID_CLEAR);
}

void TimelineViewerFrame::on_inspection_changed(wxCommandEvent &)
{
    const std::string text =
        timeline_viewer::format_inspector(m_timeline_control->document(), m_timeline_control->hit_result(),
            m_timeline_control->interaction(), m_timeline_control->inspection(), m_mappings);
    m_inspector->SetValue(wxString(text.data(), text.size()));
    if (m_timeline_control->inspection())
    {
        SetStatusText(wxString::Format("Frame %lld", static_cast<long long>(m_timeline_control->inspection()->frame)));
    }
}

void TimelineViewerFrame::on_open(wxCommandEvent &)
{
    choose_file(false);
}

void TimelineViewerFrame::on_add(wxCommandEvent &)
{
    choose_file(true);
}

void TimelineViewerFrame::choose_file(bool append)
{
    wxFileDialog dialog(this, append ? "Add timeline JSON" : "Open timeline JSON", wxEmptyString, wxEmptyString,
        "JSON files (*.json)|*.json", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK)
    {
        return;
    }

    if (!load_file(native_path(dialog.GetPath()), append))
    {
        const wxString title = m_load_outcome == timeline_viewer::LoadOutcome::COMPOSITION_FAILED
            ? "Unable to add timeline"
            : "Timeline import failed";
        show_diagnostics(m_diagnostics, title, wxOK | wxICON_ERROR);
        return;
    }
    if (!m_diagnostics.empty())
    {
        show_diagnostics(m_diagnostics, "Timeline import diagnostics", wxOK | wxICON_INFORMATION);
    }
}

bool TimelineViewerFrame::load_file(const std::filesystem::path &path, bool append)
{
    timeline_viewer::LoadResult result =
        timeline_viewer::load_timeline(path, append, m_timeline_control->document(), m_mappings);
    m_diagnostics = std::move(result.diagnostics);
    m_load_outcome = result.outcome;
    if (!result.succeeded())
    {
        return false;
    }
    m_mappings = std::move(result.mappings);
    m_timeline_control->set_document(std::move(*result.document));
    const timeline::Document &document = *m_timeline_control->document();
    SetTitle(wxString(VIEWER_TITLE) + " - " + to_wx_string(document.strings().lookup(document.metadata().title())));
    SetStatusText("Loaded " + filename(path));
    return true;
}

void TimelineViewerFrame::on_export_snapshot(wxCommandEvent &)
{
    wxFileDialog dialog(this, "Export timeline snapshot", wxEmptyString, "timeline.txt", "Text snapshots (*.txt)|*.txt",
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK)
    {
        return;
    }

    if (!export_snapshot(native_path(dialog.GetPath())))
    {
        show_diagnostics(m_diagnostics, "Snapshot export failed", wxOK | wxICON_ERROR);
    }
}

bool TimelineViewerFrame::export_snapshot(const std::filesystem::path &path)
{
    timeline_viewer::SnapshotWriteResult result = timeline_viewer::write_snapshot(path, m_timeline_control->snapshot());
    m_diagnostics = std::move(result.diagnostics);
    if (!result.succeeded())
    {
        return false;
    }
    SetStatusText("Exported " + filename(path));
    return true;
}

void TimelineViewerFrame::on_exit(wxCommandEvent &)
{
    Close(true);
}

void TimelineViewerFrame::on_zoom_in(wxCommandEvent &)
{
    m_timeline_control->zoom_in();
}

void TimelineViewerFrame::on_zoom_out(wxCommandEvent &)
{
    m_timeline_control->zoom_out();
}

void TimelineViewerFrame::on_fit_view(wxCommandEvent &)
{
    m_timeline_control->fit_view();
}

void TimelineViewerFrame::on_clear_selection(wxCommandEvent &)
{
    m_timeline_control->clear_selection();
}

void TimelineViewerFrame::show_diagnostics(
    const std::vector<std::string> &diagnostics, const wxString &title, long dialog_style)
{
    wxString message{};
    for (const std::string &diagnostic : diagnostics)
    {
        if (!message.empty())
        {
            message += "\n";
        }
        message += diagnostic;
    }
    if (message.empty())
    {
        message = "The selected file could not be imported.";
    }

    wxMessageBox(message, title, dialog_style, this);
}

bool TimelineViewerApp::OnInit()
{
    m_frame = new TimelineViewerFrame;
    if (argc == 6 && wxString(argv[1]) == "--smoke-test")
    {
        m_smoke_test = true;
        m_replacement_path = native_path(wxString(argv[2]));
        m_comparison_path = native_path(wxString(argv[3]));
        m_invalid_path = native_path(wxString(argv[4]));
        m_snapshot_path = native_path(wxString(argv[5]));
        return true;
    }
    m_frame->Show();
    return true;
}

int TimelineViewerApp::OnRun()
{
    return m_smoke_test ? (run_smoke_test() ? EXIT_SUCCESS : EXIT_FAILURE) : wxApp::OnRun();
}

bool TimelineViewerApp::run_smoke_test()
{
    m_frame->Show();
    wxYield();
    std::error_code error;
    std::filesystem::remove(m_snapshot_path, error);
    if (!m_frame->load_file(m_replacement_path, false) || !m_frame->document() ||
        m_frame->document()->lane_count() != 25 || !m_frame->mappings().empty())
    {
        return smoke_failure("replacement load failed");
    }
    if (!m_frame->load_file(m_comparison_path, true) || !m_frame->document() ||
        m_frame->document()->lane_count() != 29 || timeline::size_cast(m_frame->mappings()) != 1)
    {
        return smoke_failure("comparison load failed");
    }
    if (m_frame->load_file(m_invalid_path, false) || m_frame->diagnostics().empty() || !m_frame->document() ||
        m_frame->document()->lane_count() != 29 || timeline::size_cast(m_frame->mappings()) != 1)
    {
        return smoke_failure("failed import changed viewer state");
    }
    if (!m_frame->load_file(m_replacement_path, false) || !m_frame->document() ||
        m_frame->document()->lane_count() != 25 || !m_frame->mappings().empty())
    {
        return smoke_failure("replacement reset failed");
    }
    if (!m_frame->export_snapshot(m_snapshot_path))
    {
        return smoke_failure("snapshot export failed");
    }
    error.clear();
    const bool wrote_snapshot = std::filesystem::is_regular_file(m_snapshot_path, error) && !error;
    std::filesystem::remove(m_snapshot_path, error);
    return wrote_snapshot || smoke_failure("snapshot output is missing");
}

wxIMPLEMENT_APP(TimelineViewerApp);
