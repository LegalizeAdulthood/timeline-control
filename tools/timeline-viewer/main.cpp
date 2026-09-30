// Copyright (c) 2026 Richard Thomson

#include <timelineParAnimator/TimelineJson.h>
#include <wxTimeline/wxTimelineControl.h>

#include <wx/filedlg.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <wx/wx.h>

#include <filesystem>
#include <system_error>
#include <utility>

namespace
{

const char *hit_role_name(timeline::StyleRole role)
{
    switch (role)
    {
    case timeline::StyleRole::RULER:
    case timeline::StyleRole::RULER_LABEL:
        return "ruler";
    case timeline::StyleRole::LANE_LABEL:
        return "lane header";
    case timeline::StyleRole::LANE_BACKGROUND:
        return "lane";
    case timeline::StyleRole::INSTANT_MARKER:
        return "instant event";
    case timeline::StyleRole::INTERVAL_SPAN:
        return "interval";
    case timeline::StyleRole::ENVELOPE_ATTACK:
        return "envelope attack";
    case timeline::StyleRole::ENVELOPE_SUSTAIN:
        return "envelope sustain";
    case timeline::StyleRole::ENVELOPE_DECAY:
        return "envelope decay";
    case timeline::StyleRole::CURVE:
        return "curve";
    case timeline::StyleRole::KEYFRAME_SEGMENT:
        return "keyframe segment";
    case timeline::StyleRole::KEYFRAME_MARKER:
        return "keyframe";
    case timeline::StyleRole::PLAYHEAD:
        return "playhead";
    case timeline::StyleRole::SELECTED_ITEM:
    case timeline::StyleRole::SELECTED_LANE:
    case timeline::StyleRole::SELECTED_RANGE:
        return "selection";
    }
    return "item";
}

const char *item_type_name(timeline::InspectionItemType type)
{
    switch (type)
    {
    case timeline::InspectionItemType::INSTANT:
        return "instant";
    case timeline::InspectionItemType::INTERVAL:
        return "interval";
    case timeline::InspectionItemType::ENVELOPE:
        return "envelope";
    case timeline::InspectionItemType::CURVE:
        return "curve";
    case timeline::InspectionItemType::KEYFRAME:
        return "keyframe";
    }
    return "item";
}

const char *item_role_name(timeline::InspectionItemRole role)
{
    switch (role)
    {
    case timeline::InspectionItemRole::ACTIVE:
        return "active";
    case timeline::InspectionItemRole::SAMPLED:
        return "sampled";
    case timeline::InspectionItemRole::BEFORE:
        return "before";
    case timeline::InspectionItemRole::AFTER:
        return "after";
    case timeline::InspectionItemRole::EXACT:
        return "exact";
    }
    return "item";
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
    void on_exit(wxCommandEvent &event);
    void on_zoom_in(wxCommandEvent &event);
    void on_zoom_out(wxCommandEvent &event);
    void on_fit_view(wxCommandEvent &event);
    void on_clear_selection(wxCommandEvent &event);
    void show_import_diagnostics(const std::vector<std::string> &diagnostics, const wxString &title, long dialog_style);

    wxTimelineControl *m_timeline_control;
    wxTextCtrl *m_inspector;
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
    m_timeline_control(new wxTimelineControl(this)),
    m_inspector(new wxTextCtrl(
        this, wxID_ANY, "No frame inspection.", wxDefaultPosition, wxSize(280, -1), wxTE_MULTILINE | wxTE_READONLY))
{
    auto *file_menu = new wxMenu;
    file_menu->Append(wxID_OPEN, "&Open...\tCtrl+O");
    file_menu->AppendSeparator();
    file_menu->Append(wxID_EXIT, "E&xit");

    auto *view_menu = new wxMenu;
    view_menu->Append(wxID_ZOOM_IN, "Zoom &In\tCtrl++");
    view_menu->Append(wxID_ZOOM_OUT, "Zoom &Out\tCtrl+-");
    view_menu->Append(wxID_ZOOM_100, "&Fit\tCtrl+0");
    view_menu->AppendSeparator();
    view_menu->Append(wxID_CLEAR, "&Clear Selection\tEsc");

    auto *menu_bar = new wxMenuBar;
    menu_bar->Append(file_menu, "&File");
    menu_bar->Append(view_menu, "&View");
    SetMenuBar(menu_bar);
    CreateStatusBar();
    SetStatusText("No timeline loaded");

    auto *content = new wxBoxSizer(wxHORIZONTAL);
    content->Add(m_timeline_control, 1, wxEXPAND);
    content->Add(m_inspector, 0, wxEXPAND | wxLEFT, 1);
    SetSizer(content);

    m_timeline_control->Bind(wxEVT_TIMELINE_INSPECTION_CHANGED, &TimelineViewerFrame::on_inspection_changed, this);
    Bind(wxEVT_MENU, &TimelineViewerFrame::on_open, this, wxID_OPEN);
    Bind(wxEVT_MENU, &TimelineViewerFrame::on_exit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &TimelineViewerFrame::on_zoom_in, this, wxID_ZOOM_IN);
    Bind(wxEVT_MENU, &TimelineViewerFrame::on_zoom_out, this, wxID_ZOOM_OUT);
    Bind(wxEVT_MENU, &TimelineViewerFrame::on_fit_view, this, wxID_ZOOM_100);
    Bind(wxEVT_MENU, &TimelineViewerFrame::on_clear_selection, this, wxID_CLEAR);
}

void TimelineViewerFrame::on_inspection_changed(wxCommandEvent &)
{
    const auto &hit = m_timeline_control->hit_result();
    auto text = wxString("Hit: none\n");
    if (hit)
    {
        text = "Hit: " + wxString::FromUTF8(hit_role_name(hit->style)) + "\n";
        if (!hit->id.lane_id.empty())
        {
            text += "Lane: " + wxString::FromUTF8(hit->id.lane_id.c_str()) + "\n";
        }
        if (!hit->id.item_id.empty())
        {
            text += "Item: " + wxString::FromUTF8(hit->id.item_id.c_str()) + "\n";
        }
    }
    const auto &document = m_timeline_control->document();
    if (document)
    {
        text += "\nTitle: " + wxString::FromUTF8(document->metadata().title().c_str()) + "\n";
        text += "Source: " + wxString::FromUTF8(document->metadata().description().c_str()) + "\n";
    }
    const auto &interaction = m_timeline_control->interaction();
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
            text += "Selected lane: " + wxString::FromUTF8(interaction->selected_lane()->c_str()) + "\n";
        }
        for (const auto &id : interaction->selected_items())
        {
            text += "Selected item: " + wxString::FromUTF8(id.lane_id.c_str()) + "/" +
                wxString::FromUTF8(id.item_id.c_str()) + "\n";
        }
        if (interaction->selected_range())
        {
            const auto &range = *interaction->selected_range();
            const auto &timebase = m_timeline_control->document()->timebase();
            text += wxString::Format("Selected range: %.6f to %.6f seconds\n", timebase.seconds(range.start()),
                timebase.seconds(range.end()));
        }
        if (interaction->selected_frames())
        {
            const auto frames = *interaction->selected_frames();
            text += wxString::Format("Selected frames: %lld to %lld\n", static_cast<long long>(frames.first()),
                static_cast<long long>(frames.last()));
        }
    }
    const auto &inspection = m_timeline_control->inspection();
    if (!inspection)
    {
        m_inspector->SetValue(text + "\nNo frame inspection.");
        return;
    }

    text += wxString::Format("\nFrame: %lld\nTime: %.6f seconds\n", static_cast<long long>(inspection->frame),
        m_timeline_control->document()->timebase().seconds(inspection->time));
    for (const auto &lane : inspection->lanes)
    {
        text += "\n" + wxString::FromUTF8(lane.label.c_str()) + " [" + wxString::FromUTF8(lane.kind.c_str()) + "]";
        text += wxString::Format("\n  Source items: %d", lane.item_count);
        if (lane.items.empty())
        {
            text += "\n  No activity";
            continue;
        }
        for (const auto &item : lane.items)
        {
            text += "\n  " + wxString::FromUTF8(item_type_name(item.type)) + " " + wxString::FromUTF8(item.id.c_str()) +
                " (" + wxString::FromUTF8(item_role_name(item.role)) + ")";
            if (item.value)
            {
                text += wxString::Format(": %.6f", *item.value);
            }
        }
    }
    m_inspector->SetValue(text);
    SetStatusText(wxString::Format("Frame %lld", static_cast<long long>(inspection->frame)));
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
    auto import_options = timeline_par_animator::JsonImportOptions{};
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
