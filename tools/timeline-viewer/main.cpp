// Copyright (c) 2026 Richard Thomson

#include <wxTimeline/config.h>

#include <timelineViewer/format_inspector.h>

#include <timelineParAnimator/TimelineJson.h>

#ifdef TIMELINE_CONTROL_WITH_CAIRO
#include <wxTimeline/wxCairoTimeline.h>
#endif
#include <wxTimeline/wxTimelineControl.h>

#include <wx/filedlg.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <wx/wx.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

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

} // namespace

/// Main window that composes JSON adapters with the wx timeline control.
///
class TimelineViewerFrame : public wxFrame
{
public:
    TimelineViewerFrame();

private:
    void on_inspection_changed(wxCommandEvent &event);
    void on_open(wxCommandEvent &event);
    void on_add(wxCommandEvent &event);
    void load_file(bool append);
    void on_export_snapshot(wxCommandEvent &event);
    void on_exit(wxCommandEvent &event);
    void on_zoom_in(wxCommandEvent &event);
    void on_zoom_out(wxCommandEvent &event);
    void on_fit_view(wxCommandEvent &event);
    void on_clear_selection(wxCommandEvent &event);
    void show_import_diagnostics(const std::vector<std::string> &diagnostics, const wxString &title, long dialog_style);

    wxPanel *m_content;
    wxTimelineControl *m_timeline_control;
    wxTextCtrl *m_inspector;
    std::vector<timeline_par_animator::BeatKeysMapping> m_mappings;
};

/// wxWidgets application for manually exercising the timeline control.
///
class TimelineViewerApp : public wxApp
{
public:
    bool OnInit() override;
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
    auto *file_menu = new wxMenu;
    file_menu->Append(wxID_OPEN, "&Open...\tCtrl+O");
    file_menu->Append(wxID_ADD, "&Add...\tCtrl+Shift+O");
    file_menu->Append(wxID_SAVEAS, "Export &Snapshot...");
    file_menu->AppendSeparator();
    file_menu->Append(wxID_EXIT, "E&xit");

    auto *view_menu = new wxMenu;
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

    auto *menu_bar = new wxMenuBar;
    menu_bar->Append(file_menu, "&File");
    menu_bar->Append(view_menu, "&View");
    wxFrame::SetMenuBar(menu_bar);
    wxFrame::CreateStatusBar();
    wxFrame::SetStatusText("No timeline loaded");

    auto *content = new wxBoxSizer(wxHORIZONTAL);
    content->Add(m_timeline_control, 1, wxEXPAND);
    content->Add(m_inspector, 0, wxEXPAND | wxLEFT, 1);
    m_content->SetSizer(content);
    auto *frame_content = new wxBoxSizer(wxVERTICAL);
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
    load_file(false);
}

void TimelineViewerFrame::on_add(wxCommandEvent &)
{
    load_file(true);
}

void TimelineViewerFrame::load_file(bool append)
{
    wxFileDialog dialog(this, append ? "Add timeline JSON" : "Open timeline JSON", wxEmptyString, wxEmptyString,
        "JSON files (*.json)|*.json", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK)
    {
        return;
    }

    const std::filesystem::path source_path(dialog.GetPath().ToStdWstring());
    timeline_par_animator::JsonImportOptions import_options{};
    if (append && m_timeline_control->document() && m_timeline_control->document()->frame_grid())
    {
        const timeline::FrameGrid &grid = *m_timeline_control->document()->frame_grid();
        import_options.ticks_per_second = grid.timebase().ticks_per_second();
        import_options.frames_per_second_numerator = grid.frames_per_second_numerator();
        import_options.frames_per_second_denominator = grid.frames_per_second_denominator();
    }
    const std::filesystem::path beat_keys_config_path = source_path.parent_path() / "adapter.beat-keys.json";
    std::error_code filesystem_error{};
    if (std::filesystem::is_regular_file(beat_keys_config_path, filesystem_error))
    {
        import_options.beat_keys_config_path = beat_keys_config_path;
    }

    timeline_par_animator::JsonImportResult result =
        timeline_par_animator::import_timeline_json(source_path, import_options);
    if (!result.succeeded())
    {
        show_import_diagnostics(result.diagnostics, "Timeline import failed", wxOK | wxICON_ERROR);
        return;
    }

    if (append && m_timeline_control->document())
    {
        try
        {
            result.document = timeline::combine_documents(*m_timeline_control->document(), *result.document);
        }
        catch (const std::exception &error)
        {
            show_import_diagnostics({error.what()}, "Unable to add timeline", wxOK | wxICON_ERROR);
            return;
        }
    }
    else
    {
        m_mappings.clear();
    }
    if (result.mapping)
    {
        m_mappings.push_back(std::move(*result.mapping));
    }
    m_timeline_control->set_document(std::move(*result.document));
    const timeline::Document &document = *m_timeline_control->document();
    SetTitle(wxString(VIEWER_TITLE) + " - " + to_wx_string(document.strings().lookup(document.metadata().title())));
    SetStatusText("Loaded " + dialog.GetFilename());
    if (!result.diagnostics.empty())
    {
        show_import_diagnostics(result.diagnostics, "Timeline import diagnostics", wxOK | wxICON_INFORMATION);
    }
}

void TimelineViewerFrame::on_export_snapshot(wxCommandEvent &)
{
    wxFileDialog dialog(this, "Export timeline snapshot", wxEmptyString, "timeline.txt", "Text snapshots (*.txt)|*.txt",
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK)
    {
        return;
    }

    const std::string text = m_timeline_control->snapshot();
    std::ofstream output(std::filesystem::path(dialog.GetPath().ToStdWstring()), std::ios::binary);
    output << text;
    output.close();
    if (!output)
    {
        wxMessageBox("Unable to write the timeline snapshot.", "Snapshot export failed", wxOK | wxICON_ERROR, this);
        return;
    }
    SetStatusText("Exported " + dialog.GetFilename());
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

void TimelineViewerFrame::show_import_diagnostics(
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
    auto *frame = new TimelineViewerFrame;
    frame->Show();
    return true;
}

wxIMPLEMENT_APP(TimelineViewerApp);
