// Copyright (c) 2026 Richard Thomson

#include <imgui.h>

#include <gtest/gtest.h>

TEST(ImGuiDependency, creates_context_without_platform_or_renderer_backend)
{
    EXPECT_TRUE(IMGUI_CHECKVERSION());
    ImGuiContext *context = ImGui::CreateContext();
    EXPECT_NE(nullptr, context);
    EXPECT_EQ(context, ImGui::GetCurrentContext());
    ImGui::DestroyContext(context);
    EXPECT_EQ(nullptr, ImGui::GetCurrentContext());
}
