// Copyright (c) 2026 Richard Thomson

#include <Viewer.h>

#include <timeline/size_cast.h>
#include <timeline/Snapshot.h>

#include <gtest/gtest.h>

#include <fstream>
#include <iterator>

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
        ImGui::DestroyContext();
    }
    Command frame(Viewer &viewer);
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
    Viewer viewer;

    const bool loaded = viewer.load_file(fixtures / "extreme-normalized-vectors.json", false);

    ASSERT_TRUE(loaded);
    EXPECT_EQ(25, viewer.control().document()->lane_count());
}

TEST_F(ImGuiViewer, drawsTheCompleteHost)
{
    Viewer viewer;
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json", false));

    const Command command = frame(viewer);

    EXPECT_EQ(Command::NONE, command);
    ASSERT_TRUE(viewer.control().layout());
    EXPECT_GT(ImGui::GetDrawData()->TotalVtxCount, 0);
}

TEST_F(ImGuiViewer, presentsParameterOutputInTheInspector)
{
    Viewer viewer;
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json", false));

    frame(viewer);
    const std::string text = viewer.inspector_text();

    EXPECT_NE(std::string::npos, text.find("Lanes: 25"));
    EXPECT_NE(std::string::npos, text.find("Parameter output:"));
    EXPECT_NE(std::string::npos, text.find("(exact)"));
}

TEST_F(ImGuiViewer, composesMusicWithTheDocument)
{
    Viewer viewer;
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json", false));

    const bool loaded = viewer.load_file(fixtures / "beat-keys/rms.beat-keys.json", true);

    ASSERT_TRUE(loaded);
    EXPECT_EQ(29, viewer.control().document()->lane_count());
}

TEST_F(ImGuiViewer, presentsMappingRecipesInTheInspector)
{
    Viewer viewer;
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json", false));

    const bool loaded = viewer.load_file(fixtures / "beat-keys/rms.beat-keys.json", true);
    const std::string text = viewer.inspector_text();

    ASSERT_TRUE(loaded);
    ASSERT_EQ(1, timeline::size_cast(viewer.mappings()));
    EXPECT_EQ(3, timeline::size_cast(viewer.mappings().front().recipes()));
    EXPECT_NE(std::string::npos, text.find("music.rms -> camera.zoom"));
}

TEST_F(ImGuiViewer, replacesTheDocumentAndRecipes)
{
    Viewer viewer;
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json", false));
    ASSERT_TRUE(viewer.load_file(fixtures / "beat-keys/rms.beat-keys.json", true));

    const bool loaded = viewer.load_file(fixtures / "extreme-normalized-vectors.json", false);

    ASSERT_TRUE(loaded);
    EXPECT_EQ(25, viewer.control().document()->lane_count());
    EXPECT_TRUE(viewer.mappings().empty());
}

TEST_F(ImGuiViewer, preservesOwnedStateWhenImportFails)
{
    Viewer viewer;
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json", false));
    frame(viewer);
    const std::string before = timeline::render_snapshot(viewer.control().layout()->display_list());

    const bool loaded = viewer.load_file(fixtures / "invalid-schema.json", false);

    EXPECT_FALSE(loaded);
    EXPECT_FALSE(viewer.diagnostics().empty());
    EXPECT_EQ(25, viewer.control().document()->lane_count());
    EXPECT_EQ(before, timeline::render_snapshot(viewer.control().layout()->display_list()));
    EXPECT_EQ(0, *viewer.control().interaction()->playhead_frame());
}

TEST_F(ImGuiViewer, rejectsSnapshotExportWithoutLayout)
{
    Viewer viewer;
    const std::filesystem::path path = std::filesystem::current_path() / "timeline-imgui-viewer-test.txt";

    const bool exported = viewer.export_snapshot(path);

    EXPECT_FALSE(exported);
}

TEST_F(ImGuiViewer, exportsTheCoreSnapshot)
{
    Viewer viewer;
    const std::filesystem::path path = std::filesystem::current_path() / "timeline-imgui-viewer-test.txt";
    std::filesystem::remove(path);
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json", false));
    frame(viewer);

    const bool exported = viewer.export_snapshot(path);

    ASSERT_TRUE(exported);
    std::ifstream input(path);
    const std::string actual((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    EXPECT_EQ(timeline::render_snapshot(viewer.control().layout()->display_list()), actual);

    input.close();
    std::filesystem::remove(path);
}

TEST_F(ImGuiViewer, reportsSnapshotWriteFailure)
{
    Viewer viewer;
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json", false));
    frame(viewer);
    const std::filesystem::path path =
        std::filesystem::current_path() / "timeline-imgui-viewer-absent" / "snapshot.txt";

    const bool exported = viewer.export_snapshot(path);

    EXPECT_FALSE(exported);
    EXPECT_FALSE(viewer.diagnostics().empty());
}

TEST_F(ImGuiViewer, drawsWithoutADocument)
{
    Viewer viewer;

    const Command command = frame(viewer);

    EXPECT_EQ(Command::NONE, command);
    EXPECT_FALSE(viewer.control().document());
}

TEST_F(ImGuiViewer, displaysAnEmptyDocument)
{
    Viewer viewer;

    const bool loaded = viewer.load_file(fixtures / "empty-animation.json", false);
    frame(viewer);

    ASSERT_TRUE(loaded);
    EXPECT_EQ(0, viewer.control().document()->lane_count());
}

TEST_F(ImGuiViewer, displaysAMusicDocument)
{
    Viewer viewer;

    const bool loaded = viewer.load_file(fixtures / "par-beatdown/gold-write-windowed-features.json", false);
    frame(viewer);

    ASSERT_TRUE(loaded);
    EXPECT_GT(viewer.control().document()->lane_count(), 0);
    EXPECT_NE(std::string::npos, viewer.inspector_text().find("Schema:"));
}

TEST_F(ImGuiViewer, keepsKeyboardFrameNavigationOnTheNestedTimeline)
{
    Viewer viewer;
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json", false));
    frame(viewer);
    frame(viewer);
    ImGuiIO &io = ImGui::GetIO();
    io.AddMousePosEvent(250, 80);
    frame(viewer);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    frame(viewer);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    frame(viewer);
    const timeline::Ticks first = *viewer.control().interaction()->playhead_frame();
    ASSERT_LT(first, 4);

    io.AddKeyEvent(ImGuiKey_RightArrow, true);
    frame(viewer);

    EXPECT_EQ(first + 1, *viewer.control().interaction()->playhead_frame());
}

TEST_F(ImGuiViewer, adaptsToANarrowHost)
{
    Viewer viewer;
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2(500, 600);
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json", false));

    frame(viewer);
    frame(viewer);

    ASSERT_TRUE(viewer.control().viewport());
    EXPECT_LT(viewer.control().viewport()->height(), 400);
}

TEST_F(ImGuiViewer, routesTheOpenShortcut)
{
    Viewer viewer;
    ImGuiIO &io = ImGui::GetIO();
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json", false));
    frame(viewer);
    frame(viewer);

    io.AddKeyEvent(ImGuiMod_Ctrl, true);
    io.AddKeyEvent(ImGuiKey_O, true);
    const Command command = frame(viewer);

    EXPECT_EQ(Command::OPEN, command);
}

TEST_F(ImGuiViewer, routesTheAddShortcut)
{
    Viewer viewer;
    ImGuiIO &io = ImGui::GetIO();
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json", false));
    frame(viewer);
    frame(viewer);

    io.AddKeyEvent(ImGuiMod_Ctrl, true);
    io.AddKeyEvent(ImGuiMod_Shift, true);
    io.AddKeyEvent(ImGuiKey_O, true);
    const Command command = frame(viewer);

    EXPECT_EQ(Command::ADD, command);
}

} // namespace
