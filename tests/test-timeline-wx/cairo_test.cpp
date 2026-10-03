// Copyright (c) 2026 Richard Thomson

#include <gtest/gtest.h>
#include <timelineParAnimator/TimelineJson.h>
#include <wx/app.h>
#include <wx/dcmemory.h>
#include <wx/frame.h>
#include <wxTimeline/wxCairoTimeline.h>
#include <wxTimeline/wxCairoTimelineRenderer.h>

namespace
{

const wxTimelinePalette LIGHT{wxColour(255, 255, 255), wxColour(0, 0, 0), wxColour(0, 100, 200)};
const wxTimelinePalette DARK{wxColour(24, 24, 24), wxColour(235, 235, 235), wxColour(70, 160, 230)};
const timeline::Polyline DIAGONAL{{{3, 4}, {27, 22}}, timeline::StyleRole::CURVE, {"lane", "curve"}};

wxColour pixel(const wxImage &image, int x, int y)
{
    return wxColour(image.GetRed(x, y), image.GetGreen(x, y), image.GetBlue(x, y));
}

void settle(wxTimelineControl &control)
{
    for (int pass = 0; pass < 3; ++pass)
    {
        wxTheApp->Yield();
        control.Update();
    }
}

} // namespace

TEST(CairoRenderer, draws_nonblank_antialiased_curves_in_semantic_colours)
{
    for (const wxTimelinePalette &palette : {LIGHT, DARK})
    {
        const wxColour colour = timeline_style_colour(DIAGONAL.style, palette, true);
        const wxImage image =
            render_cairo_curve(DIAGONAL, wxSize(32, 28), wxPoint(0, 0), wxRect(0, 0, 32, 28), colour, 1, 1.0);
        ASSERT_TRUE(image.IsOk());
        ASSERT_TRUE(image.HasAlpha());
        int solid = 0;
        int partial = 0;
        for (int y = 0; y < image.GetHeight(); ++y)
        {
            for (int x = 0; x < image.GetWidth(); ++x)
            {
                const int alpha = image.GetAlpha(x, y);
                if (alpha == 255)
                {
                    ++solid;
                    EXPECT_EQ(colour, pixel(image, x, y));
                }
                else if (alpha > 0)
                {
                    ++partial;
                }
            }
        }
        EXPECT_GT(solid, 0);
        EXPECT_GT(partial, 0);
        EXPECT_EQ(0, image.GetAlpha(0, 27));
    }
}

TEST(CairoRenderer, clips_translated_curves_and_scales_device_pixels)
{
    const wxRect clip(10, 8, 12, 10);
    const wxImage image = render_cairo_curve(DIAGONAL, wxSize(32, 28), wxPoint(2, 1), clip, LIGHT.foreground, 1, 2.0);
    ASSERT_TRUE(image.IsOk());
    EXPECT_EQ(64, image.GetWidth());
    EXPECT_EQ(56, image.GetHeight());
    int covered = 0;
    for (int y = 0; y < image.GetHeight(); ++y)
    {
        for (int x = 0; x < image.GetWidth(); ++x)
        {
            if (image.GetAlpha(x, y) > 0)
            {
                ++covered;
                EXPECT_TRUE(clip.Contains(x / 2, y / 2));
            }
        }
    }
    EXPECT_GT(covered, 0);
}

TEST(CairoRenderer, keeps_sharp_joins_within_the_stroke_bounds)
{
    const timeline::Polyline curve{{{14, 24}, {20, 8}, {26, 24}}, timeline::StyleRole::CURVE, {}};
    const wxImage image =
        render_cairo_curve(curve, wxSize(32, 28), wxPoint(0, 0), wxRect(0, 0, 32, 28), LIGHT.foreground, 1, 1.0);
    ASSERT_TRUE(image.IsOk());
    EXPECT_EQ(0, image.GetAlpha(20, 5));
    EXPECT_GT(image.GetAlpha(20, 8), 0);
}

TEST(CairoRenderer, falls_back_to_native_when_device_scale_is_invalid)
{
    timeline::DisplayList list;
    list.add(DIAGONAL);
    wxBitmap native(32, 28, 24);
    wxMemoryDC native_dc(native);
    native_dc.SetBackground(wxBrush(LIGHT.background));
    native_dc.Clear();
    draw_timeline_display_list(native_dc, list, wxPoint(0, 0), LIGHT, 1, true);
    native_dc.SelectObject(wxNullBitmap);
    wxBitmap fallback(32, 28, 24);
    wxMemoryDC fallback_dc(fallback);
    fallback_dc.SetBackground(wxBrush(LIGHT.background));
    fallback_dc.Clear();
    draw_cairo_timeline_display_list(fallback_dc, list, wxPoint(0, 0), LIGHT, 1, true, wxSize(32, 28), 0.0);
    fallback_dc.SelectObject(wxNullBitmap);
    const wxImage expected = native.ConvertToImage();
    const wxImage actual = fallback.ConvertToImage();
    for (int y = 0; y < 28; ++y)
    {
        for (int x = 0; x < 32; ++x)
        {
            EXPECT_EQ(pixel(expected, x, y), pixel(actual, x, y));
        }
    }
}

TEST(CairoRenderer, handles_empty_and_single_point_curves_without_stray_pixels)
{
    for (const timeline::Polyline &curve : {timeline::Polyline{{}, timeline::StyleRole::CURVE, {}},
             timeline::Polyline{{{5, 5}}, timeline::StyleRole::KEYFRAME_SEGMENT, {}}})
    {
        const wxImage image =
            render_cairo_curve(curve, wxSize(16, 16), wxPoint(0, 0), wxRect(0, 0, 16, 16), LIGHT.foreground, 1, 1.0);
        ASSERT_TRUE(image.IsOk());
        for (int y = 0; y < 16; ++y)
        {
            for (int x = 0; x < 16; ++x)
            {
                EXPECT_EQ(0, image.GetAlpha(x, y));
            }
        }
    }
}

TEST(CairoRenderer, preserves_native_fallback_rgb_and_primitive_order)
{
    timeline::DisplayList list;
    list.add(timeline::Rectangle{0, 0, 32, 28, timeline::StyleRole::LANE_BACKGROUND, {}});
    list.add(DIAGONAL);
    list.add(timeline::Marker{14, 11, 6, 6, timeline::StyleRole::KEYFRAME_MARKER, {}});
    list.add(timeline::Swatch{2, 23, 5, 4, timeline::RgbColor(12, 34, 56), timeline::StyleRole::PALETTE, {}});
    wxBitmap bitmap(32, 28, 24);
    wxMemoryDC dc(bitmap);
    dc.SetBackground(wxBrush(LIGHT.background));
    dc.Clear();
    draw_cairo_timeline_display_list(dc, list, wxPoint(0, 0), LIGHT, 1, true, wxSize(32, 28), 1.0);
    dc.SelectObject(wxNullBitmap);
    const wxImage image = bitmap.ConvertToImage();
    EXPECT_EQ(timeline_style_colour(timeline::StyleRole::KEYFRAME_MARKER, LIGHT, true), pixel(image, 16, 13));
    EXPECT_EQ(wxColour(12, 34, 56), pixel(image, 4, 25));
    bool antialiased = false;
    const wxColour background = timeline_style_colour(timeline::StyleRole::LANE_BACKGROUND, LIGHT, true);
    const wxColour curve = timeline_style_colour(timeline::StyleRole::CURVE, LIGHT, true);
    for (int y = 4; y < 10; ++y)
    {
        for (int x = 3; x < 13; ++x)
        {
            const wxColour value = pixel(image, x, y);
            antialiased = antialiased || (value != background && value != curve);
        }
    }
    EXPECT_TRUE(antialiased);
}

TEST(CairoControl, switches_renderer_without_replacing_document_or_inspection_state)
{
    wxFrame frame(nullptr, wxID_ANY, "Cairo control test", wxDefaultPosition, wxSize(640, 480));
    wxCairoTimeline &control = *new wxCairoTimeline(&frame);
    control.SetSize(600, 400);
    const std::filesystem::path fixtures(TIMELINE_FIXTURE_DIR);
    timeline_par_animator::JsonImportResult animation =
        timeline_par_animator::import_timeline_json(fixtures / "extreme-normalized-vectors.json");
    timeline_par_animator::JsonImportResult mapping =
        timeline_par_animator::import_timeline_json(fixtures / "beat-keys/rms.beat-keys.json");
    ASSERT_TRUE(animation.succeeded());
    ASSERT_TRUE(mapping.succeeded());
    control.set_document(timeline::combine_documents(*animation.document, *mapping.document));
    frame.Show();
    settle(control);
    control.zoom_in();
    settle(control);
    wxKeyEvent key(wxEVT_CHAR_HOOK);
    key.m_keyCode = WXK_RIGHT;
    key.SetShiftDown(true);
    control.ProcessWindowEvent(key);
    settle(control);
    const std::string before = control.snapshot();
    ASSERT_FALSE(before.empty());
    ASSERT_TRUE(control.inspection());
    const timeline::Ticks selected = control.inspection()->frame;
    ASSERT_TRUE(control.interaction()->selected_frames());
    const timeline::Ticks first = control.interaction()->selected_frames()->first();
    const timeline::Ticks last = control.interaction()->selected_frames()->last();
    EXPECT_TRUE(control.cairo_enabled());
    control.set_cairo_enabled(false);
    settle(control);
    EXPECT_FALSE(control.cairo_enabled());
    EXPECT_EQ(before, control.snapshot());
    EXPECT_EQ(selected, control.inspection()->frame);
    EXPECT_EQ(first, control.interaction()->selected_frames()->first());
    EXPECT_EQ(last, control.interaction()->selected_frames()->last());
    EXPECT_EQ(29, control.document()->lane_count());
    control.set_cairo_enabled(true);
    settle(control);
    EXPECT_TRUE(control.cairo_enabled());
    EXPECT_EQ(before, control.snapshot());
    control.set_document(std::move(*animation.document));
    settle(control);
    EXPECT_FALSE(control.interaction()->selected_frames());
    EXPECT_EQ(25, control.document()->lane_count());
}
