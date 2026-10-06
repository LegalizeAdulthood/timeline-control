// Copyright (c) 2026 Richard Thomson

#include <imguiTimeline/TimelineControl.h>
#include <imguiTimeline/TimelineRenderer.h>

#include <timelineParAnimator/TimelineJson.h>

#include <timeline/size_cast.h>
#include <timeline/Snapshot.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

using namespace timeline;
using timeline_imgui::Control;

namespace
{

Time at(Ticks ticks)
{
    return Time::from_ticks(ticks);
}

Document framed_document()
{
    DocumentBuilder builder(Document(FrameGrid(Timebase(100), 10, 10, 1), 0, 0));
    for (int lane_index = 0; lane_index < 12; ++lane_index)
    {
        Lane lane(builder.intern("lane-" + std::to_string(lane_index)),
            builder.intern("Lane " + std::to_string(lane_index)), builder.intern("events"), at(0), at(100));
        lane.add(Instant(builder.intern("pulse"), builder.intern("beat"), at(lane_index == 1 ? 50 : 30)));
        builder.add_lane(std::move(lane));
    }
    return std::move(builder).build();
}

struct ImportCase
{
    const char *name;
    const char *path;
};

void PrintTo(const ImportCase &value, std::ostream *stream)
{
    *stream << value.name;
}

std::string import_case_name(const testing::TestParamInfo<ImportCase> &info)
{
    return info.param.name;
}

/// Backend-free ImGui frame host that drives the adapter through real IO.
///
class ImGuiControl : public testing::Test
{
protected:
    void SetUp() override;
    void TearDown() override;
    void begin_frame();
    void frame(Control &control)
    {
        frame(control, ImVec2(400.0F, 130.0F));
    }
    void frame(Control &control, ImVec2 size);
    void prime(Control &control);
    ImVec2 frame_point(const Control &control, Ticks frame, int lane) const;
    void move_mouse(Control &control, ImVec2 point);
    void mouse_button(Control &control, bool down);

    ImGuiContext *m_context{nullptr};
    ImVec2 m_origin;
    bool m_allow_window_scroll{false};
    float m_host_scroll{0.0F};
};

class ImGuiImportedFixture : public ImGuiControl, public testing::WithParamInterface<ImportCase>
{
};

/// Standard framed timeline control with fresh adapter state.
///
class FramedImGuiControlTest : public ImGuiControl
{
protected:
    Control m_control{framed_document()};
};

/// Standard framed timeline control submitted through its initial frames.
///
class PrimedImGuiControlTest : public FramedImGuiControlTest
{
protected:
    void SetUp() override
    {
        FramedImGuiControlTest::SetUp();
        prime(m_control);
    }
};

void ImGuiControl::SetUp()
{
    m_context = ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = ImVec2(800.0F, 600.0F);
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

void ImGuiControl::TearDown()
{
    ImGui::DestroyContext(m_context);
}

void ImGuiControl::begin_frame()
{
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20.0F, 20.0F));
    ImGui::SetNextWindowSize(ImVec2(720.0F, 520.0F));
    ImGui::Begin("host", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoScrollbar | (m_allow_window_scroll ? 0 : ImGuiWindowFlags_NoScrollWithMouse));
    m_origin = ImGui::GetCursorScreenPos();
}

void ImGuiControl::frame(Control &control, ImVec2 size)
{
    begin_frame();
    timeline_imgui::draw_timeline("timeline", control, size);
    ImGui::Button("Other item");
    if (m_allow_window_scroll)
    {
        ImGui::Dummy(ImVec2(100.0F, 1000.0F));
    }
    m_host_scroll = ImGui::GetScrollY();
    ImGui::End();
    ImGui::Render();
}

void ImGuiControl::prime(Control &control)
{
    frame(control);
    frame(control);
}

ImVec2 ImGuiControl::frame_point(const Control &control, Ticks frame, int lane) const
{
    const Viewport &viewport = control.viewport().value();
    const LayoutMetrics &metrics = control.layout_metrics().value();
    return ImVec2(m_origin.x + static_cast<float>(frame_x(frame, *control.document()->frame_grid(), viewport, metrics)),
        m_origin.y + static_cast<float>(lane_y(lane, viewport, metrics).value() + metrics.lane_height() / 2));
}

void ImGuiControl::move_mouse(Control &control, ImVec2 point)
{
    ImGui::GetIO().AddMousePosEvent(point.x, point.y);
    frame(control);
}

void ImGuiControl::mouse_button(Control &control, bool down)
{
    ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, down);
    frame(control);
}

TEST_F(ImGuiControl, leavesMissingDocumentUnlaidOut)
{
    Control control;

    frame(control);

    EXPECT_FALSE(control.document());
    EXPECT_FALSE(control.layout());
}

TEST_F(ImGuiControl, leavesEmptyDocumentUnlaidOut)
{
    Control control(Document(100));

    frame(control);

    EXPECT_FALSE(control.layout());
}

TEST_F(ImGuiControl, leavesTinyItemUnlaidOut)
{
    Control control(framed_document());

    frame(control, ImVec2(1.0F, 1.0F));

    EXPECT_FALSE(control.layout());
}

TEST_F(ImGuiControl, supportsFramelessDocumentsWithoutFrameInspection)
{
    DocumentBuilder builder(Document(100));
    builder.add_lane(
        Lane(builder.intern("continuous"), builder.intern("Continuous"), builder.intern("signal"), at(0), at(100)));
    Control control(std::move(builder).build());
    prime(control);

    move_mouse(control, ImVec2(m_origin.x + 250.0F, m_origin.y + 10.0F));
    mouse_button(control, true);
    mouse_button(control, false);

    ASSERT_TRUE(control.layout());
    ASSERT_TRUE(control.interaction()->playhead());
    EXPECT_FALSE(control.interaction()->playhead_frame());
    EXPECT_FALSE(control.inspection());
}

TEST_F(PrimedImGuiControlTest, delegatesLayoutToCoreGeometry)
{
    ASSERT_TRUE(m_control.layout());
    const Layout expected(
        *m_control.document(), *m_control.viewport(), *m_control.layout_metrics(), *m_control.interaction());

    EXPECT_EQ(render_snapshot(expected.display_list()), render_snapshot(m_control.layout()->display_list()));
    EXPECT_GT(ImGui::GetDrawData()->TotalVtxCount, 0);
}

TEST_F(PrimedImGuiControlTest, delegatesHoverAndInspectionToCoreGeometry)
{
    move_mouse(m_control, frame_point(m_control, 3, 0));

    ASSERT_TRUE(m_control.hit_result());
    EXPECT_EQ("lane-0", m_control.document()->strings().lookup(m_control.hit_result()->id.lane_id));
    EXPECT_EQ(*m_control.document()->strings().find("pulse"), m_control.hit_result()->id.item_id);
    ASSERT_TRUE(m_control.inspection());
    EXPECT_EQ(3, m_control.inspection()->frame);
    EXPECT_EQ(0, *m_control.interaction()->playhead_frame());
}

TEST_F(PrimedImGuiControlTest, clearsHoverOutsideTheItem)
{
    move_mouse(m_control, frame_point(m_control, 3, 0));

    move_mouse(m_control, ImVec2(790.0F, 590.0F));

    EXPECT_FALSE(m_control.hit_result());
}

TEST_F(PrimedImGuiControlTest, selectsLaneQualifiedItems)
{
    move_mouse(m_control, frame_point(m_control, 3, 0));
    mouse_button(m_control, true);
    mouse_button(m_control, false);

    ASSERT_TRUE(m_control.interaction());
    EXPECT_EQ(3, *m_control.interaction()->playhead_frame());
    EXPECT_TRUE(m_control.interaction()->is_selected(
        DisplayId{m_control.document()->lanes().front().id(), *m_control.document()->strings().find("pulse")}));
}

TEST_F(PrimedImGuiControlTest, addsSelectedItemsWithCtrl)
{
    move_mouse(m_control, frame_point(m_control, 3, 0));
    mouse_button(m_control, true);
    mouse_button(m_control, false);

    ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, true);
    move_mouse(m_control, frame_point(m_control, 5, 1));
    mouse_button(m_control, true);
    mouse_button(m_control, false);

    EXPECT_EQ(2, size_cast(m_control.interaction()->selected_items()));
}

TEST_F(PrimedImGuiControlTest, snapsDraggedRangesToFrames)
{
    move_mouse(m_control, frame_point(m_control, 3, 0));
    mouse_button(m_control, true);
    move_mouse(m_control, frame_point(m_control, 6, 0));
    mouse_button(m_control, false);

    ASSERT_TRUE(m_control.interaction()->selected_frames());
    EXPECT_EQ(3, m_control.interaction()->selected_frames()->first());
    EXPECT_EQ(6, m_control.interaction()->selected_frames()->last());
    EXPECT_TRUE(m_control.interaction()->selected_items().empty());
    EXPECT_EQ(6, m_control.inspection()->frame);
}

TEST_F(PrimedImGuiControlTest, keepsDraggingOutsideTheItem)
{
    move_mouse(m_control, frame_point(m_control, 3, 0));
    mouse_button(m_control, true);
    move_mouse(m_control, ImVec2(0.0F, 0.0F));
    mouse_button(m_control, false);

    ASSERT_TRUE(m_control.interaction()->selected_frames());
    EXPECT_EQ(0, m_control.interaction()->selected_frames()->first());
    EXPECT_EQ(3, m_control.interaction()->selected_frames()->last());
}

TEST_F(PrimedImGuiControlTest, cancelsDraggingOnFocusLoss)
{
    move_mouse(m_control, frame_point(m_control, 3, 0));
    mouse_button(m_control, true);

    ImGui::GetIO().AddFocusEvent(false);
    frame(m_control);
    ImGui::GetIO().AddFocusEvent(true);
    move_mouse(m_control, frame_point(m_control, 7, 0));

    EXPECT_FALSE(m_control.interaction()->selected_frames());
    EXPECT_EQ(3, *m_control.interaction()->playhead_frame());
}

TEST_F(PrimedImGuiControlTest, releasesARangeBeyondTheRightEdgeWithoutJumpingLeft)
{
    move_mouse(m_control, frame_point(m_control, 3, 0));
    mouse_button(m_control, true);
    move_mouse(m_control, ImVec2(790.0F, 590.0F));
    mouse_button(m_control, false);

    ASSERT_TRUE(m_control.interaction()->selected_frames());
    EXPECT_EQ(3, m_control.interaction()->selected_frames()->first());
    EXPECT_EQ(9, m_control.interaction()->selected_frames()->last());
    EXPECT_EQ(9, *m_control.interaction()->playhead_frame());
}

TEST_F(PrimedImGuiControlTest, ownsKeyboardNavigation)
{
    move_mouse(m_control, frame_point(m_control, 3, 0));
    mouse_button(m_control, true);
    mouse_button(m_control, false);
    move_mouse(m_control, ImVec2(790.0F, 590.0F));

    ImGui::GetIO().AddKeyEvent(ImGuiKey_RightArrow, true);
    frame(m_control);

    EXPECT_EQ(4, *m_control.interaction()->playhead_frame());
}

TEST_F(PrimedImGuiControlTest, extendsKeyboardRangeWithShift)
{
    move_mouse(m_control, frame_point(m_control, 4, 0));
    mouse_button(m_control, true);
    mouse_button(m_control, false);

    ImGui::GetIO().AddKeyEvent(ImGuiMod_Shift, true);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_RightArrow, true);
    frame(m_control);

    ASSERT_TRUE(m_control.interaction()->selected_frames());
    EXPECT_EQ(4, m_control.interaction()->selected_frames()->first());
    EXPECT_EQ(5, m_control.interaction()->selected_frames()->last());
}

TEST_F(PrimedImGuiControlTest, clearsKeyboardSelectionWithEscape)
{
    move_mouse(m_control, frame_point(m_control, 4, 0));
    mouse_button(m_control, true);
    mouse_button(m_control, false);
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Shift, true);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_RightArrow, true);
    frame(m_control);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_RightArrow, false);
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Shift, false);
    frame(m_control);

    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
    frame(m_control);

    EXPECT_FALSE(m_control.interaction()->selected_lane());
    EXPECT_FALSE(m_control.interaction()->selected_frames());
    EXPECT_TRUE(m_control.interaction()->selected_items().empty());
}

TEST_F(PrimedImGuiControlTest, doesNotInterceptKeyboardNavigationForOtherItems)
{
    move_mouse(m_control, frame_point(m_control, 3, 0));
    mouse_button(m_control, true);
    mouse_button(m_control, false);
    move_mouse(m_control, ImVec2(790.0F, 590.0F));
    ImGui::GetIO().AddKeyEvent(ImGuiKey_RightArrow, true);
    frame(m_control);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_RightArrow, false);
    frame(m_control);
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Shift, true);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_RightArrow, true);
    frame(m_control);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_RightArrow, false);
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Shift, false);
    frame(m_control);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
    frame(m_control);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
    frame(m_control);

    ImGui::GetIO().AddKeyEvent(ImGuiKey_Tab, true);
    begin_frame();
    timeline_imgui::draw_timeline("timeline", m_control, ImVec2(400.0F, 130.0F));
    ImGui::Button("Other item");
    ImGui::End();
    ImGui::Render();
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Tab, false);
    begin_frame();
    timeline_imgui::draw_timeline("timeline", m_control, ImVec2(400.0F, 130.0F));
    ImGui::Button("Other item");
    const bool other_item_focused = ImGui::IsItemFocused();
    ImGui::End();
    ImGui::Render();
    ImGui::GetIO().AddKeyEvent(ImGuiKey_LeftArrow, true);
    frame(m_control);

    EXPECT_TRUE(other_item_focused);
    EXPECT_EQ(5, *m_control.interaction()->playhead_frame());
}

TEST_F(PrimedImGuiControlTest, translatesCtrlWheelToZoom)
{
    move_mouse(m_control, frame_point(m_control, 5, 0));

    ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, true);
    ImGui::GetIO().AddMouseWheelEvent(0.0F, 1.0F);
    frame(m_control);

    ASSERT_TRUE(m_control.viewport());
    EXPECT_EQ(80, m_control.viewport()->end().ticks() - m_control.viewport()->start().ticks());
}

TEST_F(PrimedImGuiControlTest, translatesShiftWheelToHorizontalPan)
{
    move_mouse(m_control, frame_point(m_control, 5, 0));
    m_control.zoom_in();
    frame(m_control);
    const Ticks start = m_control.viewport()->start().ticks();

    ImGui::GetIO().AddKeyEvent(ImGuiMod_Shift, true);
    ImGui::GetIO().AddMouseWheelEvent(0.0F, -1.0F);
    frame(m_control);

    EXPECT_GT(m_control.viewport()->start().ticks(), start);
}

TEST_F(PrimedImGuiControlTest, translatesWheelToLaneScroll)
{
    move_mouse(m_control, frame_point(m_control, 5, 0));

    ImGui::GetIO().AddMouseWheelEvent(0.0F, -2.0F);
    frame(m_control);

    EXPECT_EQ(2, m_control.viewport()->first_lane());
}

TEST_F(PrimedImGuiControlTest, fitViewRestoresTheCompleteTimeline)
{
    move_mouse(m_control, frame_point(m_control, 5, 0));
    m_control.zoom_in();
    ImGui::GetIO().AddMouseWheelEvent(0.0F, -2.0F);
    frame(m_control);

    m_control.fit_view();
    frame(m_control);

    EXPECT_EQ(at(0), m_control.viewport()->start());
    EXPECT_EQ(at(100), m_control.viewport()->end());
    EXPECT_EQ(0, m_control.viewport()->first_lane());
}

TEST_F(ImGuiControl, ownsItsDocument)
{
    Document source = framed_document();
    Control control(source);
    DocumentBuilder builder(std::move(source));

    builder.add_lane(Lane(builder.intern("extra"), builder.intern("Extra"), builder.intern("events"), at(0), at(100)));
    source = std::move(builder).build();

    EXPECT_EQ(12, control.document()->lane_count());
    EXPECT_EQ(13, source.lane_count());
}

TEST_F(ImGuiControl, resetsStateOnDocumentReplacement)
{
    Control control(framed_document());
    prime(control);
    move_mouse(control, frame_point(control, 3, 0));
    mouse_button(control, true);
    mouse_button(control, false);
    control.zoom_in();
    frame(control);

    control.set_document(framed_document());

    EXPECT_FALSE(control.layout());
    EXPECT_FALSE(control.hit_result());
    ASSERT_TRUE(control.interaction());
    EXPECT_TRUE(control.interaction()->selected_items().empty());
    EXPECT_EQ(0, *control.interaction()->playhead_frame());
    EXPECT_EQ(0, control.inspection()->frame);
}

TEST_F(ImGuiControl, cancelsDraggingOnDocumentReplacement)
{
    Control control(framed_document());
    prime(control);
    move_mouse(control, frame_point(control, 3, 0));
    mouse_button(control, true);

    control.set_document(framed_document());
    move_mouse(control, ImVec2(790.0F, 590.0F));
    mouse_button(control, false);

    EXPECT_EQ(at(0), control.viewport()->start());
    EXPECT_EQ(at(100), control.viewport()->end());
    EXPECT_EQ(0, *control.interaction()->playhead_frame());
    EXPECT_FALSE(control.interaction()->selected_frames());
}

TEST_F(FramedImGuiControlTest, ownsWheelInputWithoutScrollingTheHostWindow)
{
    m_allow_window_scroll = true;
    prime(m_control);

    move_mouse(m_control, frame_point(m_control, 5, 0));
    ImGui::GetIO().AddMouseWheelEvent(0.0F, -1.0F);
    frame(m_control);
    frame(m_control);

    EXPECT_FLOAT_EQ(0.0F, m_host_scroll);
    EXPECT_EQ(1, m_control.viewport()->first_lane());
}

TEST_F(PrimedImGuiControlTest, updatesGeometryAfterResize)
{
    frame(m_control, ImVec2(600.0F, 480.0F));

    EXPECT_EQ(600, m_control.viewport()->width());
    EXPECT_EQ(480, m_control.viewport()->height());
}

TEST_F(PrimedImGuiControlTest, clampsLaneScrollAfterResize)
{
    move_mouse(m_control, frame_point(m_control, 5, 0));
    ImGui::GetIO().AddMouseWheelEvent(0.0F, -8.0F);
    frame(m_control);
    ASSERT_GT(m_control.viewport()->first_lane(), 0);

    frame(m_control, ImVec2(600.0F, 480.0F));

    EXPECT_EQ(0, m_control.viewport()->first_lane());
}

TEST_F(PrimedImGuiControlTest, zoomInNarrowsTheViewport)
{
    m_control.zoom_in();
    frame(m_control);

    EXPECT_EQ(80, m_control.viewport()->end().ticks() - m_control.viewport()->start().ticks());
}

TEST_F(PrimedImGuiControlTest, zoomOutRestoresTheViewport)
{
    m_control.zoom_in();
    frame(m_control);

    m_control.zoom_out();
    frame(m_control);

    EXPECT_EQ(100, m_control.viewport()->end().ticks() - m_control.viewport()->start().ticks());
}

TEST_F(ImGuiControl, submitsMultipleIndependentItems)
{
    Control first(framed_document());
    Control second(framed_document());

    begin_frame();
    timeline_imgui::draw_timeline("first", first, ImVec2(400.0F, 130.0F));
    const ImGuiID first_id = ImGui::GetItemID();
    timeline_imgui::draw_timeline("second", second, ImVec2(400.0F, 130.0F));
    const ImGuiID second_id = ImGui::GetItemID();
    ImGui::End();
    ImGui::Render();
    first.set_document(Document(100));

    EXPECT_NE(first_id, second_id);
    EXPECT_FALSE(first.layout());
    EXPECT_TRUE(second.layout());
}

TEST_F(ImGuiControl, usesTheAvailableSizeOverload)
{
    Control first(framed_document());
    Control second(framed_document());

    begin_frame();
    timeline_imgui::draw_timeline("first", first, ImVec2(400.0F, 130.0F));
    timeline_imgui::draw_timeline("second", second);
    ImGui::End();
    ImGui::Render();

    ASSERT_TRUE(first.layout());
    ASSERT_TRUE(second.layout());
    EXPECT_GT(second.viewport()->height(), first.viewport()->height());
}

TEST_F(ImGuiControl, delegatesEveryPrimitiveToMatchingImguiMeshOperations)
{
    begin_frame();
    ImDrawList &draw_list = *ImGui::GetWindowDrawList();
    const ImGuiStyle &style = ImGui::GetStyle();
    StringTableBuilder strings;
    const StringId text = strings.intern("A");
    DisplayList list(std::move(strings).build());
    list.add(Line{0, 0, 20, 0, StyleRole::RULER, {}});
    list.add(Rectangle{0, 10, 20, 5, StyleRole::LANE_BACKGROUND, {}});
    list.add(Marker{25, 10, 2, 5, StyleRole::INSTANT_MARKER, {}});
    list.add(Polyline{{{0, 20}, {10, 25}, {20, 20}}, StyleRole::CURVE, {}});
    list.add(Swatch{0, 30, 20, 5, RgbColor(7, 11, 19), StyleRole::SELECTED_ITEM, {}});
    list.add(Text{0, 40, text, StyleRole::RULER_LABEL, {}});

    const int first_vertex = draw_list.VtxBuffer.Size;
    const int first_index = draw_list.IdxBuffer.Size;
    timeline_imgui::draw_display_list(draw_list, list, m_origin, style, true, 80);
    const std::vector<ImDrawVert> actual(
        draw_list.VtxBuffer.Data + first_vertex, draw_list.VtxBuffer.Data + draw_list.VtxBuffer.Size);
    const int actual_indices = draw_list.IdxBuffer.Size - first_index;
    const int reference_vertex = draw_list.VtxBuffer.Size;
    const int reference_index = draw_list.IdxBuffer.Size;
    draw_list.AddLine(m_origin, ImVec2(m_origin.x + 20.0F, m_origin.y),
        timeline_imgui::style_colour(StyleRole::RULER, style, true), 1.0F);
    draw_list.AddRectFilled(ImVec2(m_origin.x, m_origin.y + 10.0F), ImVec2(m_origin.x + 20.0F, m_origin.y + 15.0F),
        timeline_imgui::style_colour(StyleRole::LANE_BACKGROUND, style, true));
    draw_list.AddRectFilled(ImVec2(m_origin.x + 25.0F, m_origin.y + 10.0F),
        ImVec2(m_origin.x + 27.0F, m_origin.y + 15.0F),
        timeline_imgui::style_colour(StyleRole::INSTANT_MARKER, style, true));
    const ImVec2 points[]{ImVec2(m_origin.x, m_origin.y + 20.0F), ImVec2(m_origin.x + 10.0F, m_origin.y + 25.0F),
        ImVec2(m_origin.x + 20.0F, m_origin.y + 20.0F)};
    draw_list.AddPolyline(points, 3, timeline_imgui::style_colour(StyleRole::CURVE, style, true), 0, 2.0F);
    draw_list.AddRectFilled(ImVec2(m_origin.x, m_origin.y + 30.0F), ImVec2(m_origin.x + 20.0F, m_origin.y + 35.0F),
        IM_COL32(7, 11, 19, 255));
    draw_list.AddText(
        ImVec2(m_origin.x, m_origin.y + 40.0F), timeline_imgui::style_colour(StyleRole::RULER_LABEL, style, true), "A");
    const std::vector<ImDrawVert> expected(
        draw_list.VtxBuffer.Data + reference_vertex, draw_list.VtxBuffer.Data + draw_list.VtxBuffer.Size);
    const int reference_indices = draw_list.IdxBuffer.Size - reference_index;
    ImGui::End();
    ImGui::Render();

    ASSERT_EQ(expected.size(), actual.size());
    EXPECT_EQ(reference_indices, actual_indices);
    for (int index = 0; index < size_cast(actual); ++index)
    {
        EXPECT_EQ(expected[index].col, actual[index].col);
        EXPECT_FLOAT_EQ(expected[index].pos.x, actual[index].pos.x);
        EXPECT_FLOAT_EQ(expected[index].pos.y, actual[index].pos.y);
        EXPECT_FLOAT_EQ(expected[index].uv.x, actual[index].uv.x);
        EXPECT_FLOAT_EQ(expected[index].uv.y, actual[index].uv.y);
    }
}

TEST_F(ImGuiControl, clipsLongLaneLabelsWithoutChangingTheDrawListClipStack)
{
    begin_frame();
    ImDrawList &draw_list = *ImGui::GetWindowDrawList();
    const ImVec2 clip = draw_list.GetClipRectMax();
    const int first_vertex = draw_list.VtxBuffer.Size;
    StringTableBuilder strings;
    const StringId text = strings.intern("A very long lane label which cannot fit");
    DisplayList list(std::move(strings).build());
    list.add(Text{0, 0, text, StyleRole::LANE_LABEL, {}});

    timeline_imgui::draw_display_list(draw_list, list, m_origin, ImGui::GetStyle(), false, 40);
    const std::vector<ImDrawVert> vertices(
        draw_list.VtxBuffer.Data + first_vertex, draw_list.VtxBuffer.Data + draw_list.VtxBuffer.Size);
    const ImVec2 after = draw_list.GetClipRectMax();
    ImGui::End();
    ImGui::Render();

    EXPECT_FALSE(vertices.empty());
    for (const ImDrawVert &vertex : vertices)
    {
        EXPECT_LE(vertex.pos.x, m_origin.x + 40.0F);
    }
    EXPECT_FLOAT_EQ(clip.x, after.x);
    EXPECT_FLOAT_EQ(clip.y, after.y);
}

TEST_P(ImGuiImportedFixture, preservesTimelineSemantics)
{
    const std::filesystem::path fixtures(TIMELINE_TEST_FIXTURE_DIR);
    timeline_par_animator::JsonImportOptions options{};
    options.beat_keys_config_path = fixtures / "beat-keys/adapter.beat-keys.json";
    const timeline_par_animator::JsonImportResult imported =
        timeline_par_animator::import_timeline_json(fixtures / GetParam().path, options);
    ASSERT_TRUE(imported.succeeded());
    ASSERT_TRUE(imported.diagnostics.empty());
    Control control(*imported.document);

    prime(control);

    ASSERT_TRUE(control.layout());
    const Layout expected(*control.document(), *control.viewport(), *control.layout_metrics(), *control.interaction());
    EXPECT_EQ(render_snapshot(expected.display_list()), render_snapshot(control.layout()->display_list()));
    EXPECT_EQ(inspect_frame(*control.document(), 0)->lanes.size(), control.inspection()->lanes.size());
}

INSTANTIATE_TEST_SUITE_P(SharedWxFixtures, ImGuiImportedFixture,
    testing::Values(ImportCase{"TimelineEvents", "beat-keys/timeline-events.json"},
        ImportCase{"WindowedFeatures", "par-beatdown/gold-write-windowed-features.json"},
        ImportCase{"RowPulses", "beat-keys/gold-write-row-pulses.json"}),
    import_case_name);

} // namespace
