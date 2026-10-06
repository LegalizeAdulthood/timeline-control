// Copyright (c) 2026 Richard Thomson

#include <Viewer.h>

#include <timeline/size_cast.h>
#include <timeline/Snapshot.h>

#include <gtest/gtest.h>

#include <fstream>
#include <iterator>
#include <system_error>

using timeline_imgui_viewer::Command;
using timeline_imgui_viewer::Viewer;

namespace
{

const std::filesystem::path fixtures(TIMELINE_TEST_FIXTURE_DIR);

/// Backend-free host for the complete viewer's menus, timeline, and inspector.
///
class ImGuiViewer : public testing::Test
{
protected:
    void SetUp() override;
    void TearDown() override
    {
        std::error_code error;
        std::filesystem::remove(m_snapshot_path, error);
        ImGui::DestroyContext();
    }
    Command frame(Viewer &viewer);

    Viewer m_viewer;
    const std::filesystem::path m_snapshot_path{std::filesystem::current_path() / "timeline-imgui-viewer-test.txt"};
};

/// Viewer loaded with the shared animation and submitted through initial frames.
///
class LoadedImGuiViewer : public ImGuiViewer
{
protected:
    void SetUp() override
    {
        ImGuiViewer::SetUp();
        ASSERT_TRUE(m_viewer.load_file(fixtures / "extreme-normalized-vectors.json", false));
        frame(m_viewer);
        frame(m_viewer);
    }
};

void ImGuiViewer::SetUp()
{
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = ImVec2(1000, 640);
    io.DeltaTime = 1.0F / 60.0F;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigInputTrickleEventQueue = false;
    io.ConfigMacOSXBehaviors = false;
    unsigned char *pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID(ImTextureID{1});
    std::error_code error;
    std::filesystem::remove(m_snapshot_path, error);
}

Command ImGuiViewer::frame(Viewer &viewer)
{
    ImGui::NewFrame();
    const Command command = timeline_imgui_viewer::draw_viewer(viewer, false);
    ImGui::Render();
    return command;
}

TEST_F(ImGuiViewer, loadsSharedAnimation)
{
    const bool loaded = m_viewer.load_file(fixtures / "extreme-normalized-vectors.json", false);

    ASSERT_TRUE(loaded);
    EXPECT_EQ(25, m_viewer.control().document()->lane_count());
}

TEST_F(LoadedImGuiViewer, drawsTheCompleteHost)
{
    const Command command = frame(m_viewer);

    EXPECT_EQ(Command::NONE, command);
    ASSERT_TRUE(m_viewer.control().layout());
    EXPECT_GT(ImGui::GetDrawData()->TotalVtxCount, 0);
}

TEST_F(LoadedImGuiViewer, presentsParameterOutputInTheInspector)
{
    const std::string text = m_viewer.inspector_text();

    EXPECT_NE(std::string::npos, text.find("Lanes: 25"));
    EXPECT_NE(std::string::npos, text.find("Parameter output:"));
    EXPECT_NE(std::string::npos, text.find("(exact)"));
}

TEST_F(LoadedImGuiViewer, composesMusicWithTheDocument)
{
    const bool loaded = m_viewer.load_file(fixtures / "beat-keys/rms.beat-keys.json", true);

    ASSERT_TRUE(loaded);
    EXPECT_EQ(29, m_viewer.control().document()->lane_count());
}

TEST_F(LoadedImGuiViewer, presentsMappingRecipesInTheInspector)
{
    const bool loaded = m_viewer.load_file(fixtures / "beat-keys/rms.beat-keys.json", true);
    const std::string text = m_viewer.inspector_text();

    ASSERT_TRUE(loaded);
    ASSERT_EQ(1, timeline::size_cast(m_viewer.mappings()));
    EXPECT_EQ(3, timeline::size_cast(m_viewer.mappings().front().recipes()));
    EXPECT_NE(std::string::npos, text.find("music.rms -> camera.zoom"));
}

TEST_F(LoadedImGuiViewer, replacesTheDocumentAndRecipes)
{
    ASSERT_TRUE(m_viewer.load_file(fixtures / "beat-keys/rms.beat-keys.json", true));

    const bool loaded = m_viewer.load_file(fixtures / "extreme-normalized-vectors.json", false);

    ASSERT_TRUE(loaded);
    EXPECT_EQ(25, m_viewer.control().document()->lane_count());
    EXPECT_TRUE(m_viewer.mappings().empty());
}

TEST_F(LoadedImGuiViewer, preservesOwnedStateWhenImportFails)
{
    const std::string before = timeline::render_snapshot(m_viewer.control().layout()->display_list());

    const bool loaded = m_viewer.load_file(fixtures / "invalid-schema.json", false);

    EXPECT_FALSE(loaded);
    EXPECT_FALSE(m_viewer.diagnostics().empty());
    EXPECT_EQ(25, m_viewer.control().document()->lane_count());
    EXPECT_EQ(before, timeline::render_snapshot(m_viewer.control().layout()->display_list()));
    EXPECT_EQ(0, *m_viewer.control().interaction()->playhead_frame());
}

TEST_F(ImGuiViewer, rejectsSnapshotExportWithoutLayout)
{
    const bool exported = m_viewer.export_snapshot(m_snapshot_path);

    EXPECT_FALSE(exported);
}

TEST_F(LoadedImGuiViewer, exportsTheCoreSnapshot)
{
    const bool exported = m_viewer.export_snapshot(m_snapshot_path);

    ASSERT_TRUE(exported);
    std::ifstream input(m_snapshot_path);
    const std::string actual((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    EXPECT_EQ(timeline::render_snapshot(m_viewer.control().layout()->display_list()), actual);
}

TEST_F(LoadedImGuiViewer, reportsSnapshotWriteFailure)
{
    const std::filesystem::path path =
        std::filesystem::current_path() / "timeline-imgui-viewer-absent" / "snapshot.txt";

    const bool exported = m_viewer.export_snapshot(path);

    EXPECT_FALSE(exported);
    EXPECT_FALSE(m_viewer.diagnostics().empty());
}

TEST_F(ImGuiViewer, drawsWithoutADocument)
{
    const Command command = frame(m_viewer);

    EXPECT_EQ(Command::NONE, command);
    EXPECT_FALSE(m_viewer.control().document());
}

TEST_F(ImGuiViewer, displaysAnEmptyDocument)
{
    const bool loaded = m_viewer.load_file(fixtures / "empty-animation.json", false);
    frame(m_viewer);

    ASSERT_TRUE(loaded);
    EXPECT_EQ(0, m_viewer.control().document()->lane_count());
}

TEST_F(ImGuiViewer, displaysAMusicDocument)
{
    const bool loaded = m_viewer.load_file(fixtures / "par-beatdown/gold-write-windowed-features.json", false);
    frame(m_viewer);

    ASSERT_TRUE(loaded);
    EXPECT_GT(m_viewer.control().document()->lane_count(), 0);
    EXPECT_NE(std::string::npos, m_viewer.inspector_text().find("Schema:"));
}

TEST_F(LoadedImGuiViewer, keepsKeyboardFrameNavigationOnTheNestedTimeline)
{
    ImGuiIO &io = ImGui::GetIO();
    io.AddMousePosEvent(250, 80);
    frame(m_viewer);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    frame(m_viewer);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    frame(m_viewer);
    const timeline::Ticks first = *m_viewer.control().interaction()->playhead_frame();
    ASSERT_LT(first, 4);

    io.AddKeyEvent(ImGuiKey_RightArrow, true);
    frame(m_viewer);

    EXPECT_EQ(first + 1, *m_viewer.control().interaction()->playhead_frame());
}

TEST_F(ImGuiViewer, adaptsToANarrowHost)
{
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2(500, 600);
    ASSERT_TRUE(m_viewer.load_file(fixtures / "extreme-normalized-vectors.json", false));

    frame(m_viewer);
    frame(m_viewer);

    ASSERT_TRUE(m_viewer.control().viewport());
    EXPECT_LT(m_viewer.control().viewport()->height(), 400);
}

TEST_F(LoadedImGuiViewer, routesTheOpenShortcut)
{
    ImGuiIO &io = ImGui::GetIO();

    io.AddKeyEvent(ImGuiMod_Ctrl, true);
    io.AddKeyEvent(ImGuiKey_O, true);
    const Command command = frame(m_viewer);

    EXPECT_EQ(Command::OPEN, command);
}

TEST_F(LoadedImGuiViewer, routesTheAddShortcut)
{
    ImGuiIO &io = ImGui::GetIO();

    io.AddKeyEvent(ImGuiMod_Ctrl, true);
    io.AddKeyEvent(ImGuiMod_Shift, true);
    io.AddKeyEvent(ImGuiKey_O, true);
    const Command command = frame(m_viewer);

    EXPECT_EQ(Command::ADD, command);
}

} // namespace
