// Copyright (c) 2026 Richard Thomson

#include "Viewer.h"
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <timeline/Snapshot.h>

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

TEST_F(ImGuiViewer, loadsSharedAnimationAndDrawsTheCompleteHost)
{
    Viewer viewer;
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json", false));
    EXPECT_EQ(25, viewer.control().document()->lane_count());
    EXPECT_EQ(Command::NONE, frame(viewer));
    ASSERT_TRUE(viewer.control().layout());
    EXPECT_GT(ImGui::GetDrawData()->TotalVtxCount, 0);
    EXPECT_NE(std::string::npos, viewer.inspector_text().find("Lanes: 25"));
    EXPECT_NE(std::string::npos, viewer.inspector_text().find("Parameter output:"));
    EXPECT_NE(std::string::npos, viewer.inspector_text().find("(exact)"));
}

TEST_F(ImGuiViewer, composesMusicAndRecipesThenReplacesTheDocument)
{
    Viewer viewer;
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json", false));
    ASSERT_TRUE(viewer.load_file(fixtures / "beat-keys/rms.beat-keys.json", true));
    EXPECT_EQ(29, viewer.control().document()->lane_count());
    ASSERT_EQ(1, timeline::size_cast(viewer.mappings()));
    EXPECT_EQ(3, timeline::size_cast(viewer.mappings().front().recipes()));
    EXPECT_NE(std::string::npos, viewer.inspector_text().find("music.rms -> camera.zoom"));
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json", false));
    EXPECT_EQ(25, viewer.control().document()->lane_count());
    EXPECT_TRUE(viewer.mappings().empty());
}

TEST_F(ImGuiViewer, preservesOwnedStateWhenImportFails)
{
    Viewer viewer;
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json", false));
    frame(viewer);
    const std::string before = timeline::render_snapshot(viewer.control().layout()->display_list());
    EXPECT_FALSE(viewer.load_file(fixtures / "invalid-schema.json", false));
    EXPECT_FALSE(viewer.diagnostics().empty());
    EXPECT_EQ(25, viewer.control().document()->lane_count());
    EXPECT_EQ(before, timeline::render_snapshot(viewer.control().layout()->display_list()));
    EXPECT_EQ(0, *viewer.control().interaction()->playhead_frame());
}

TEST_F(ImGuiViewer, exportsTheCoreSnapshotAndReportsWriteFailure)
{
    Viewer viewer;
    const std::filesystem::path path = std::filesystem::current_path() / "timeline-imgui-viewer-test.txt";
    EXPECT_FALSE(viewer.export_snapshot(path));
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json", false));
    frame(viewer);
    ASSERT_TRUE(viewer.export_snapshot(path));
    std::ifstream input(path);
    const std::string actual((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    EXPECT_EQ(timeline::render_snapshot(viewer.control().layout()->display_list()), actual);
    input.close();
    std::filesystem::remove(path);
    EXPECT_FALSE(viewer.export_snapshot(path / "absent" / "snapshot.txt"));
    EXPECT_FALSE(viewer.diagnostics().empty());
}

TEST_F(ImGuiViewer, supportsEmptyAndMusicDocumentsWithoutASecondSemanticModel)
{
    Viewer viewer;
    frame(viewer);
    EXPECT_FALSE(viewer.control().document());
    ASSERT_TRUE(viewer.load_file(fixtures / "empty-animation.json", false));
    frame(viewer);
    EXPECT_EQ(0, viewer.control().document()->lane_count());
    ASSERT_TRUE(viewer.load_file(fixtures / "par-beatdown/gold-write-windowed-features.json", false));
    frame(viewer);
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

TEST_F(ImGuiViewer, routesFileShortcutsAndAdaptsToANarrowHost)
{
    Viewer viewer;
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2(500, 600);
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json", false));
    frame(viewer);
    frame(viewer);
    ASSERT_TRUE(viewer.control().viewport());
    EXPECT_LT(viewer.control().viewport()->height(), 400);
    io.AddKeyEvent(ImGuiMod_Ctrl, true);
    io.AddKeyEvent(ImGuiKey_O, true);
    EXPECT_EQ(Command::OPEN, frame(viewer));
    io.AddKeyEvent(ImGuiKey_O, false);
    frame(viewer);
    io.AddKeyEvent(ImGuiMod_Shift, true);
    io.AddKeyEvent(ImGuiKey_O, true);
    EXPECT_EQ(Command::ADD, frame(viewer));
}

} // namespace
