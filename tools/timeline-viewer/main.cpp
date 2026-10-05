// Copyright (c) 2026 Richard Thomson

#include <wxTimeline/config.h>

#include <timeline/size_cast.h>

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

wxString document_summary(const timeline::Document &document)
{
    wxString text;
    const auto draw_line = [&text](const wxString &line)
    {
        text += line + "\n";
    };
    wxString title = document.metadata().title();
    if (title.empty())
    {
        title = "Untitled timeline";
    }

    draw_line("Title: " + title);
    draw_line(document.is_valid() ? "Valid: yes" : "Valid: no");
    draw_line(
        wxString::Format("Ticks per second: %lld", static_cast<long long>(document.timebase().ticks_per_second())));
    if (document.frame_grid())
    {
        const timeline::FrameGrid &frame_grid = *document.frame_grid();
        draw_line(wxString::Format("Frames: %lld", static_cast<long long>(frame_grid.frame_count())));
        draw_line(wxString::Format("Frame rate: %lld/%lld fps",
            static_cast<long long>(frame_grid.frames_per_second_numerator()),
            static_cast<long long>(frame_grid.frames_per_second_denominator())));
    }
    if (document.source_summary())
    {
        const timeline::SourceSummary &summary = *document.source_summary();
        draw_line("Schema: " + summary.schema() + wxString::Format(" v%d", summary.schema_version()));
        draw_line(wxString::Format("Features: %d", summary.feature_count()));
        draw_line(wxString::Format("Events: %d", summary.event_count()));
        if (summary.generation_summary())
        {
            const timeline::GenerationSummary &generation = *summary.generation_summary();
            draw_line("Generator: " + generation.generator_name() + " " + generation.generator_version());
            draw_line(wxString::Format("Inputs: %d", timeline::size_cast(generation.source_references())));
            if (!generation.target_counts().empty())
            {
                draw_line(
                    wxString::Format("Generated target groups: %d", timeline::size_cast(generation.target_counts())));
            }
            if (!generation.source_counts().empty())
            {
                draw_line(wxString::Format("Music source groups: %d", timeline::size_cast(generation.source_counts())));
            }
        }
        if (summary.first_frame())
        {
            draw_line(wxString::Format("Frame extent: %lld to %lld", static_cast<long long>(*summary.first_frame()),
                static_cast<long long>(*summary.last_frame())));
        }
        if (summary.first_time())
        {
            draw_line(wxString::Format("Time extent: %.6f to %.6f seconds",
                document.timebase().seconds(*summary.first_time()), document.timebase().seconds(*summary.last_time())));
        }
        if (summary.frame_offset())
        {
            draw_line(
                wxString::Format("Frame offset: %.6f seconds", document.timebase().seconds(*summary.frame_offset())));
        }
    }
    draw_line(wxString::Format("Tracks: %d", document.track_count()));
    draw_line(wxString::Format("Keyframes: %d", document.keyframe_count()));
    draw_line(wxString::Format("Lanes: %d", document.lane_count()));
    draw_line("Source: " + document.metadata().description());
    return text;
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
    const std::optional<timeline::Document> &document = m_timeline_control->document();
    const std::optional<timeline::HitResult> &hit = m_timeline_control->hit_result();
    wxString text("Hit: none\n");
    if (hit)
    {
        text = "Hit: " + to_wx_string(timeline::to_string(hit->style)) + "\n";
        if (!hit->id.lane_id.empty() && document)
        {
            text += "Lane: " + to_wx_string(document->strings().lookup(hit->id.lane_id)) + "\n";
        }
        if (!hit->id.item_id.empty())
        {
            text += "Item: " + to_wx_string(document->strings().lookup(hit->id.item_id)) + "\n";
        }
    }
    if (document)
    {
        text += "\n" + document_summary(*document);
    }
    for (const timeline_par_animator::BeatKeysMapping &mapping : m_mappings)
    {
        text += "\nMusic input: " + mapping.source_document().metadata().description() + "\n";
        text += "Output: " + mapping.output().mode + " / " + mapping.output().namespace_name + "\n";
        text += wxString::Format("Mapping recipes: %d\n", timeline::size_cast(mapping.recipes()));
        for (const timeline_par_animator::MappingRecipe &recipe : mapping.recipes())
        {
            text += recipe.source + " -> " + recipe.target + " (" + recipe.operation + ")\n";
            text += wxString::Format(
                "Scale: %g  Offset: %g  Decay: %g seconds\n", recipe.scale, recipe.offset, recipe.decay_seconds);
            if (recipe.clamp)
            {
                text += wxString::Format("Clamp: %g to %g\n", recipe.clamp->first, recipe.clamp->second);
            }
        }
    }
    const std::optional<timeline::Interaction> &interaction = m_timeline_control->interaction();
    if (interaction)
    {
        if (interaction->playhead())
        {
            text += wxString::Format("\nPlayhead: %.6f seconds\n",
                m_timeline_control->document()->timebase().seconds(*interaction->playhead()));
        }
        if (interaction->playhead_frame())
        {
            text += wxString::Format("Playhead frame: %lld\n", static_cast<long long>(*interaction->playhead_frame()));
        }
        if (interaction->selected_lane())
        {
            text += "Selected lane: " + to_wx_string(document->strings().lookup(*interaction->selected_lane())) + "\n";
        }
        for (const timeline::DisplayId &id : interaction->selected_items())
        {
            text += "Selected item: " + to_wx_string(document->strings().lookup(id.lane_id)) + "/" +
                to_wx_string(document->strings().lookup(id.item_id)) + "\n";
        }
        if (interaction->selected_range())
        {
            const timeline::TimeRange &range = *interaction->selected_range();
            const timeline::Timebase &timebase = m_timeline_control->document()->timebase();
            text += wxString::Format("Selected range: %.6f to %.6f seconds\n", timebase.seconds(range.start()),
                timebase.seconds(range.end()));
        }
        if (interaction->selected_frames())
        {
            const timeline::FrameRange frames = *interaction->selected_frames();
            text += wxString::Format("Selected frames: %lld to %lld\n", static_cast<long long>(frames.first()),
                static_cast<long long>(frames.last()));
        }
    }
    const std::optional<timeline::FrameInspection> &inspection = m_timeline_control->inspection();
    if (!inspection)
    {
        m_inspector->SetValue(text + "\nNo frame inspection.");
        return;
    }

    text += wxString::Format("\nFrame: %lld\nTime: %.6f seconds\n", static_cast<long long>(inspection->frame),
        m_timeline_control->document()->timebase().seconds(inspection->time));
    for (const timeline::LaneInspection &lane : inspection->lanes)
    {
        text += "\n" + lane.label + " [" + lane.kind + "]";
        text += wxString::Format("\n  Source items: %d", lane.item_count);
        if (lane.value)
        {
            text += wxString::Format("\n  Frame value: %.6f", *lane.value);
        }
        if (lane.output_value)
        {
            text += wxString::Format("\n  Parameter output: %.6f", *lane.output_value);
        }
        if (lane.items.empty())
        {
            text += "\n  No activity";
            continue;
        }
        for (const timeline::InspectionItem &item : lane.items)
        {
            text += "\n  " + to_wx_string(timeline::to_string(item.type)) + " " +
                to_wx_string(document->strings().lookup(item.id)) + " (" +
                to_wx_string(timeline::to_string(item.role)) + ")";
            if (item.value)
            {
                text += wxString::Format(": %.6f", *item.value);
            }
            for (const auto &[name, value] : item.attributes)
            {
                if (!value.empty())
                {
                    text += "\n    " + name + ": " + value;
                }
            }
            if (item.palette)
            {
                text += wxString::Format("\n    Palette: %d colors", timeline::size_cast(*item.palette));
                for (int index = 0; index < timeline::size_cast(*item.palette); ++index)
                {
                    const timeline::RgbColor &color = (*item.palette)[index];
                    text +=
                        wxString::Format("\n      [%d] RGB %d/%d/%d", index, color.red(), color.green(), color.blue());
                }
            }
        }
    }
    m_inspector->SetValue(text);
    SetStatusText(wxString::Format("Frame %lld", static_cast<long long>(inspection->frame)));
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
    SetTitle(wxString(VIEWER_TITLE) + " - " + m_timeline_control->document()->metadata().title());
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
