// Copyright (c) 2026 Richard Thomson

#include <timelineParAnimator/TimelineJson.h>
#include <wxTimeline/wxTimelineControl.h>

#include <wx/filedlg.h>
#include <wx/wx.h>

#include <filesystem>
#include <system_error>
#include <utility>

/// Main window that composes JSON adapters with the wx timeline control.
///
class TimelineViewerFrame : public wxFrame
{
public:
    TimelineViewerFrame();

private:
    void on_open(wxCommandEvent &event);
    void on_exit(wxCommandEvent &event);
    void show_import_diagnostics(const std::vector<std::string> &diagnostics, const wxString &title, long dialog_style);

    wxTimelineControl *m_timeline_control;
};

/// wxWidgets application for manually exercising the timeline control.
///
class TimelineViewerApp : public wxApp
{
public:
    bool OnInit() override;
};

TimelineViewerFrame::TimelineViewerFrame() :
    wxFrame(nullptr, wxID_ANY, "Timeline Viewer", wxDefaultPosition, wxSize(800, 500)),
    m_timeline_control(new wxTimelineControl(this))
{
    auto *file_menu = new wxMenu;
    file_menu->Append(wxID_OPEN, "&Open...\tCtrl+O");
    file_menu->AppendSeparator();
    file_menu->Append(wxID_EXIT, "E&xit");

    auto *menu_bar = new wxMenuBar;
    menu_bar->Append(file_menu, "&File");
    SetMenuBar(menu_bar);
    CreateStatusBar();
    SetStatusText("No timeline loaded");

    Bind(wxEVT_MENU, &TimelineViewerFrame::on_open, this, wxID_OPEN);
    Bind(wxEVT_MENU, &TimelineViewerFrame::on_exit, this, wxID_EXIT);
}

void TimelineViewerFrame::on_open(wxCommandEvent &)
{
    auto dialog = wxFileDialog(this, "Open timeline JSON", wxEmptyString, wxEmptyString, "JSON files (*.json)|*.json",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK)
    {
        return;
    }

    const auto source_path = std::filesystem::path(dialog.GetPath().ToStdWstring());
    auto import_options = timeline_par_animator::TimelineJsonImportOptions{};
    const auto beat_keys_config_path = source_path.parent_path() / "adapter.beat-keys.json";
    auto filesystem_error = std::error_code{};
    if (std::filesystem::is_regular_file(beat_keys_config_path, filesystem_error))
    {
        import_options.beat_keys_config_path = beat_keys_config_path;
    }

    auto result = timeline_par_animator::import_timeline_json(source_path, import_options);
    if (!result.succeeded())
    {
        show_import_diagnostics(result.diagnostics, "Timeline import failed", wxOK | wxICON_ERROR);
        return;
    }

    m_timeline_control->set_document(std::move(*result.document));
    SetTitle("Timeline Viewer - " + dialog.GetFilename());
    SetStatusText("Loaded " + dialog.GetFilename());
    if (!result.diagnostics.empty())
    {
        show_import_diagnostics(result.diagnostics, "Timeline import diagnostics", wxOK | wxICON_INFORMATION);
    }
}

void TimelineViewerFrame::on_exit(wxCommandEvent &)
{
    Close(true);
}

void TimelineViewerFrame::show_import_diagnostics(
    const std::vector<std::string> &diagnostics, const wxString &title, long dialog_style)
{
    auto message = wxString{};
    for (const auto &diagnostic : diagnostics)
    {
        if (!message.empty())
        {
            message += "\n";
        }
        message += wxString::FromUTF8(diagnostic.c_str());
    }
    if (message.empty())
    {
        message = "The selected file could not be imported.";
    }

    wxMessageBox(message, title, dialog_style, this);
}

bool TimelineViewerApp::OnInit()
{
    auto *frame = new TimelineViewerFrame;
    frame->Show();
    return true;
}

wxIMPLEMENT_APP(TimelineViewerApp);
