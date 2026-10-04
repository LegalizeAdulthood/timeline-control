// Copyright (c) 2026 Richard Thomson

#include <cstring>
#include <gtest/gtest.h>
#include <limits>
#include <timelineParAnimator/TimelineJson.h>
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

void refresh_layout(wxTimelineControl &control)
{
    control.snapshot();
}

} // namespace

TEST(CairoRenderer, drawsNonblankAntialiasedCurvesInSemanticColours)
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

TEST(CairoRenderer, clipsTranslatedCurvesAndScalesDevicePixels)
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

TEST(CairoRenderer, keepsSharpJoinsWithinTheStrokeBounds)
{
    const timeline::Polyline curve{{{14, 24}, {20, 8}, {26, 24}}, timeline::StyleRole::CURVE, {}};
    const wxImage image =
        render_cairo_curve(curve, wxSize(32, 28), wxPoint(0, 0), wxRect(0, 0, 32, 28), LIGHT.foreground, 1, 1.0);
    ASSERT_TRUE(image.IsOk());
    EXPECT_EQ(0, image.GetAlpha(20, 5));
    EXPECT_GT(image.GetAlpha(20, 8), 0);
}

TEST(CairoRenderer, fallsBackToNativeWhenDeviceScaleIsInvalid)
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

TEST(CairoRenderer, handlesEmptyAndSinglePointCurvesWithoutStrayPixels)
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

TEST(CairoRenderer, preservesSourceRgbAndPrimitiveOrderWhenPresenting)
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

TEST(CairoRenderer, drawsAllFilledRolesAndSourceRgbInOrder)
{
    for (const wxTimelinePalette &palette : {LIGHT, DARK})
    {
        timeline::DisplayList list;
        int x = 2;
        for (timeline::StyleRole role : {timeline::StyleRole::LANE_BACKGROUND, timeline::StyleRole::INTERVAL_SPAN,
                 timeline::StyleRole::ENVELOPE_ATTACK, timeline::StyleRole::ENVELOPE_SUSTAIN,
                 timeline::StyleRole::ENVELOPE_DECAY, timeline::StyleRole::SELECTED_LANE,
                 timeline::StyleRole::SELECTED_RANGE})
        {
            list.add(timeline::Rectangle{x, 2, 6, 12, role, {}});
            x += 8;
        }
        list.add(timeline::Marker{2, 18, 6, 8, timeline::StyleRole::INSTANT_MARKER, {}});
        list.add(timeline::Marker{10, 18, 6, 8, timeline::StyleRole::KEYFRAME_MARKER, {}});
        list.add(timeline::Marker{18, 18, 6, 8, timeline::StyleRole::SELECTED_ITEM, {}});
        list.add(
            timeline::Swatch{26, 18, 6, 8, timeline::RgbColor(12, 34, 56), timeline::StyleRole::SELECTED_ITEM, {}});
        list.add(timeline::Rectangle{40, 18, 12, 8, timeline::StyleRole::SELECTED_RANGE, {}});
        list.add(timeline::Marker{44, 20, 4, 4, timeline::StyleRole::INSTANT_MARKER, {}});
        const wxImage image = render_cairo_display_list(
            list, wxSize(64, 32), wxPoint(0, 0), wxRect(0, 0, 64, 32), palette, 1, true, *wxNORMAL_FONT, 1.0);
        ASSERT_TRUE(image.IsOk());
        x = 2;
        for (int index = 0; index < 7; ++index)
        {
            const timeline::Rectangle &rectangle = std::get<timeline::Rectangle>(list.primitives()[index]);
            EXPECT_EQ(timeline_style_colour(rectangle.style, palette, true), pixel(image, x + 2, 6));
            EXPECT_EQ(255, image.GetAlpha(x + 2, 6));
            x += 8;
        }
        EXPECT_EQ(timeline_style_colour(timeline::StyleRole::INSTANT_MARKER, palette, true), pixel(image, 4, 22));
        EXPECT_EQ(timeline_style_colour(timeline::StyleRole::KEYFRAME_MARKER, palette, true), pixel(image, 12, 22));
        EXPECT_EQ(timeline_style_colour(timeline::StyleRole::SELECTED_ITEM, palette, true), pixel(image, 20, 22));
        EXPECT_EQ(wxColour(12, 34, 56), pixel(image, 28, 22));
        EXPECT_EQ(timeline_style_colour(timeline::StyleRole::INSTANT_MARKER, palette, true), pixel(image, 45, 21));
        EXPECT_EQ(0, image.GetAlpha(0, 0));
    }
}

TEST(CairoRenderer, clipsAndAntialiasesRulersPlayheadsAndKeyframeSegments)
{
    timeline::DisplayList list;
    list.add(timeline::Line{0, 0, 31, 27, timeline::StyleRole::RULER, {}});
    list.add(timeline::Line{20, 0, 20, 27, timeline::StyleRole::PLAYHEAD, {}});
    list.add(timeline::Polyline{{{0, 24}, {31, 8}}, timeline::StyleRole::KEYFRAME_SEGMENT, {}});
    const wxRect clip(5, 4, 24, 20);
    const wxImage image =
        render_cairo_display_list(list, wxSize(36, 32), wxPoint(2, 1), clip, LIGHT, 1, true, *wxNORMAL_FONT, 2.0);
    ASSERT_TRUE(image.IsOk());
    EXPECT_EQ(72, image.GetWidth());
    EXPECT_EQ(64, image.GetHeight());
    int partial = 0;
    int covered = 0;
    for (int y = 0; y < image.GetHeight(); ++y)
    {
        for (int x = 0; x < image.GetWidth(); ++x)
        {
            const int alpha = image.GetAlpha(x, y);
            if (alpha > 0)
            {
                ++covered;
                EXPECT_TRUE(clip.Contains(x / 2, y / 2));
                partial += alpha < 255;
            }
        }
    }
    EXPECT_GT(covered, 0);
    EXPECT_GT(partial, 0);
    EXPECT_EQ(timeline_style_colour(timeline::StyleRole::PLAYHEAD, LIGHT, true), pixel(image, 44, 12));
}

TEST(CairoRenderer, compositesNativeFontCoverageWithClippingAndDrawOrder)
{
    wxBitmap bitmap(96, 40, 24);
    wxMemoryDC dc(bitmap);
    dc.SetFont(*wxNORMAL_FONT);
    dc.SetBackground(*wxBLACK_BRUSH);
    dc.Clear();
    dc.SetTextForeground(*wxWHITE);
    dc.SetBackgroundMode(wxTRANSPARENT);
    dc.DrawText("Timeline", 3, 2);
    dc.SelectObject(wxNullBitmap);
    const wxImage native = bitmap.ConvertToImage();
    timeline::DisplayList list;
    list.add(timeline::Text{3, 2, "Timeline", timeline::StyleRole::LANE_LABEL, {}});
    list.add(timeline::Text{3, 24, "Time", timeline::StyleRole::RULER_LABEL, {}});
    list.add(timeline::Marker{20, 0, 5, 40, timeline::StyleRole::SELECTED_ITEM, {}});
    const wxRect clip(0, 0, 80, 40);
    const wxImage image =
        render_cairo_display_list(list, wxSize(96, 40), wxPoint(0, 0), clip, DARK, 1, false, *wxNORMAL_FONT, 1.0);
    ASSERT_TRUE(image.IsOk());
    int covered = 0;
    for (int y = 0; y < 20; ++y)
    {
        for (int x = 0; x < 96; ++x)
        {
            if (x >= 20 && x < 25)
            {
                EXPECT_EQ(timeline_style_colour(timeline::StyleRole::SELECTED_ITEM, DARK, false), pixel(image, x, y));
            }
            else if (x < 80)
            {
                const bool native_covered = native.GetRed(x, y) || native.GetGreen(x, y) || native.GetBlue(x, y);
                EXPECT_EQ(native_covered, image.GetAlpha(x, y) > 0);
                const int coverage = (native.GetRed(x, y) + native.GetGreen(x, y) + native.GetBlue(x, y) + 2) / 3;
                EXPECT_EQ(coverage, image.GetAlpha(x, y));
                covered += image.GetAlpha(x, y) > 0;
            }
            else
            {
                EXPECT_EQ(0, image.GetAlpha(x, y));
            }
        }
    }
    EXPECT_GT(covered, 0);
}

TEST(CairoControl, switchesRendererWithoutReplacingDocumentOrInspectionState)
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
    refresh_layout(control);
    control.SetFocus();
    refresh_layout(control);
    control.zoom_in();
    refresh_layout(control);
    control.step_playhead(1, true);
    refresh_layout(control);
    const std::string before = control.snapshot();
    ASSERT_FALSE(before.empty());
    ASSERT_TRUE(control.inspection());
    const timeline::Ticks selected = control.inspection()->frame;
    ASSERT_TRUE(control.interaction()->selected_frames());
    const timeline::Ticks first = control.interaction()->selected_frames()->first();
    const timeline::Ticks last = control.interaction()->selected_frames()->last();
    EXPECT_TRUE(control.cairo_enabled());
    control.set_cairo_enabled(false);
    refresh_layout(control);
    EXPECT_FALSE(control.cairo_enabled());
    EXPECT_EQ(before, control.snapshot());
    EXPECT_EQ(selected, control.inspection()->frame);
    EXPECT_EQ(first, control.interaction()->selected_frames()->first());
    EXPECT_EQ(last, control.interaction()->selected_frames()->last());
    EXPECT_EQ(29, control.document()->lane_count());
    control.set_cairo_enabled(true);
    refresh_layout(control);
    EXPECT_TRUE(control.cairo_enabled());
    EXPECT_EQ(before, control.snapshot());
    control.set_document(std::move(*animation.document));
    refresh_layout(control);
    EXPECT_FALSE(control.interaction()->selected_frames());
    EXPECT_EQ(25, control.document()->lane_count());
}

TEST(CairoRenderer, partialRepaintsPreserveDevicePixelAlignment)
{
    timeline::DisplayList list;
    list.add(timeline::Rectangle{0, 0, 96, 64, timeline::StyleRole::LANE_BACKGROUND, {}});
    list.add(timeline::Polyline{{{0, 0}, {95, 63}}, timeline::StyleRole::CURVE, {}});
    list.add(timeline::Line{19, 0, 19, 63, timeline::StyleRole::PLAYHEAD, {}});
    list.add(timeline::Text{8, 8, "Timeline", timeline::StyleRole::LANE_LABEL, {}});
    list.add(timeline::Swatch{40, 28, 20, 16, timeline::RgbColor(12, 34, 56), timeline::StyleRole::PALETTE, {}});
    for (double scale : {1.0, 1.25, 1.5, 2.0})
    {
        SCOPED_TRACE(scale);
        const auto paint = [&](wxRect clip)
        {
            wxBitmap bitmap(96, 64, 24);
            wxMemoryDC dc(bitmap);
            dc.SetFont(*wxNORMAL_FONT);
            dc.SetBackground(*wxWHITE_BRUSH);
            dc.Clear();
            dc.SetClippingRegion(clip);
            draw_cairo_timeline_display_list(dc, list, wxPoint(0, 0), LIGHT, 1, true, wxSize(96, 64), scale);
            dc.SelectObject(wxNullBitmap);
            return bitmap.ConvertToImage();
        };
        const wxImage full = paint(wxRect(0, 0, 96, 64));
        const wxRect clip(7, 5, 70, 48);
        const wxImage partial = paint(clip);
        int mismatched = 0;
        for (int y = 0; y < 64; ++y)
        {
            for (int x = 0; x < 96; ++x)
            {
                if (clip.Contains(x, y))
                {
                    mismatched += pixel(full, x, y) != pixel(partial, x, y);
                }
                else
                {
                    EXPECT_EQ(LIGHT.background, pixel(partial, x, y));
                }
            }
        }
        EXPECT_EQ(0, mismatched);
    }
}

TEST(CairoRenderer, recreatesSurfacesForScaleFontThemeAndSizeChanges)
{
    timeline::DisplayList list;
    list.add(timeline::Rectangle{0, 0, 160, 80, timeline::StyleRole::LANE_BACKGROUND, {}});
    list.add(timeline::Text{4, 4, "Timeline", timeline::StyleRole::LANE_LABEL, {}});
    list.add(timeline::Swatch{8, 48, 12, 12, timeline::RgbColor(12, 34, 56), timeline::StyleRole::PALETTE, {}});
    const wxFont font = *wxNORMAL_FONT;
    const wxImage baseline = render_cairo_display_list(
        list, wxSize(160, 80), wxPoint(0, 0), wxRect(0, 0, 160, 80), LIGHT, 1, true, font, 1.0);
    ASSERT_TRUE(baseline.IsOk());
    for (int pass = 0; pass < 12; ++pass)
    {
        for (double scale : {1.0, 1.25, 1.5, 2.0})
        {
            SCOPED_TRACE(scale);
            wxFont larger_font(font);
            larger_font.SetFractionalPointSize(font.GetFractionalPointSize() * 1.5);
            const wxImage changed = render_cairo_display_list(
                list, wxSize(120, 64), wxPoint(0, 0), wxRect(0, 0, 120, 64), DARK, 2, false, larger_font, scale);
            ASSERT_TRUE(changed.IsOk());
            EXPECT_EQ(static_cast<int>(120 * scale), changed.GetWidth());
            EXPECT_EQ(static_cast<int>(64 * scale), changed.GetHeight());
            EXPECT_EQ(timeline_style_colour(timeline::StyleRole::LANE_BACKGROUND, DARK, false),
                pixel(changed, static_cast<int>(110 * scale), static_cast<int>(60 * scale)));
            EXPECT_EQ(wxColour(12, 34, 56), pixel(changed, static_cast<int>(12 * scale), static_cast<int>(54 * scale)));
            const wxImage restored = render_cairo_display_list(
                list, wxSize(160, 80), wxPoint(0, 0), wxRect(0, 0, 160, 80), LIGHT, 1, true, font, 1.0);
            ASSERT_TRUE(restored.IsOk());
            EXPECT_EQ(0, std::memcmp(baseline.GetData(), restored.GetData(), 160 * 80 * 3));
            EXPECT_EQ(0, std::memcmp(baseline.GetAlpha(), restored.GetAlpha(), 160 * 80));
        }
    }
    timeline::DisplayList text;
    text.add(timeline::Text{4, 4, "Timeline", timeline::StyleRole::LANE_LABEL, {}});
    wxFont larger_font(font);
    larger_font.SetFractionalPointSize(font.GetFractionalPointSize() * 1.5);
    const wxImage small_image = render_cairo_display_list(
        text, wxSize(160, 80), wxPoint(0, 0), wxRect(0, 0, 160, 80), LIGHT, 1, true, font, 1.0);
    const wxImage large_image = render_cairo_display_list(
        text, wxSize(160, 80), wxPoint(0, 0), wxRect(0, 0, 160, 80), LIGHT, 1, true, larger_font, 1.0);
    ASSERT_TRUE(small_image.IsOk());
    ASSERT_TRUE(large_image.IsOk());
    EXPECT_NE(0, std::memcmp(small_image.GetAlpha(), large_image.GetAlpha(), 160 * 80));
}

TEST(CairoRenderer, recoversAfterInvalidSurfaceRequests)
{
    timeline::DisplayList list;
    list.add(DIAGONAL);
    for (double scale : {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(),
             std::numeric_limits<double>::max()})
    {
        EXPECT_FALSE(render_cairo_display_list(
            list, wxSize(32, 28), wxPoint(0, 0), wxRect(0, 0, 32, 28), LIGHT, 1, true, *wxNORMAL_FONT, scale)
                .IsOk());
    }
    EXPECT_FALSE(render_cairo_display_list(
        list, wxSize(0, 28), wxPoint(0, 0), wxRect(0, 0, 32, 28), LIGHT, 1, true, *wxNORMAL_FONT, 1.0)
            .IsOk());
    EXPECT_TRUE(render_cairo_display_list(
        list, wxSize(32, 28), wxPoint(0, 0), wxRect(0, 0, 32, 28), LIGHT, 1, true, *wxNORMAL_FONT, 1.0)
            .IsOk());
}

TEST(CairoControl, preservesInteractionThroughPresentationChangesAndDestruction)
{
    const std::filesystem::path fixtures(TIMELINE_FIXTURE_DIR);
    const timeline_par_animator::JsonImportResult animation =
        timeline_par_animator::import_timeline_json(fixtures / "extreme-normalized-vectors.json");
    const timeline_par_animator::JsonImportResult mapping =
        timeline_par_animator::import_timeline_json(fixtures / "beat-keys/rms.beat-keys.json");
    const timeline_par_animator::JsonImportResult palette =
        timeline_par_animator::import_timeline_json(fixtures / "color-map-mixed.json");
    ASSERT_TRUE(animation.succeeded());
    ASSERT_TRUE(mapping.succeeded());
    ASSERT_TRUE(palette.succeeded());
    for (int lifetime = 0; lifetime < 3; ++lifetime)
    {
        wxFrame frame(nullptr, wxID_ANY, "Cairo lifecycle test", wxDefaultPosition, wxSize(640, 480));
        wxCairoTimeline &control = *new wxCairoTimeline(&frame);
        control.SetSize(600, 400);
        control.set_document(timeline::combine_documents(*animation.document, *mapping.document));
        frame.Show();
        refresh_layout(control);
        control.SetFocus();
        refresh_layout(control);
        control.zoom_in();
        refresh_layout(control);
        control.step_playhead(1, true);
        refresh_layout(control);
        ASSERT_TRUE(control.interaction()->selected_frames());
        const timeline::Ticks first = control.interaction()->selected_frames()->first();
        const timeline::Ticks last = control.interaction()->selected_frames()->last();
        const timeline::Time playhead = *control.interaction()->playhead();
        const timeline::Ticks frame_index = control.inspection()->frame;
        const wxFont font = control.GetFont();
        const wxColour background = control.GetBackgroundColour();
        const wxColour foreground = control.GetForegroundColour();
        const wxSize original_size = control.GetSize();
        const std::string baseline = control.snapshot();
        for (int pass = 0; pass < 6; ++pass)
        {
            control.set_cairo_enabled(pass % 2 == 0);
            control.SetSize(440, 280);
            wxFont larger_font(font);
            larger_font.SetFractionalPointSize(font.GetFractionalPointSize() * 1.5);
            control.SetFont(larger_font);
            control.SetBackgroundColour(DARK.background);
            control.SetForegroundColour(DARK.foreground);
            control.Refresh(false);
            refresh_layout(control);
            EXPECT_FALSE(control.snapshot().empty());
            EXPECT_NE(baseline, control.snapshot());
            wxDPIChangedEvent dpi(wxSize(96, 96), wxSize(144, 144));
            control.ProcessWindowEvent(dpi);
            refresh_layout(control);
            wxSysColourChangedEvent theme;
            control.ProcessWindowEvent(theme);
            refresh_layout(control);
            control.SetFont(font);
            control.SetBackgroundColour(background);
            control.SetForegroundColour(foreground);
            control.SetSize(original_size);
            control.Refresh(false);
            refresh_layout(control);
            EXPECT_EQ(baseline, control.snapshot());
            EXPECT_EQ(playhead, control.interaction()->playhead());
            EXPECT_EQ(first, control.interaction()->selected_frames()->first());
            EXPECT_EQ(last, control.interaction()->selected_frames()->last());
            EXPECT_EQ(frame_index, control.inspection()->frame);
            wxMouseEvent motion(wxEVT_MOTION);
            motion.SetPosition(wxPoint(4, control.FromDIP(40)));
            control.ProcessWindowEvent(motion);
            ASSERT_TRUE(control.hit_result());
            EXPECT_EQ("animation-0[0]", control.hit_result()->id.lane_id);
        }
        control.set_document(*palette.document);
        refresh_layout(control);
        EXPECT_EQ(1, control.document()->lane_count());
        EXPECT_FALSE(control.interaction()->selected_frames());
        EXPECT_NE(std::string::npos, control.snapshot().find("swatch PALETTE"));
        control.set_document(*animation.document);
        refresh_layout(control);
        EXPECT_EQ(25, control.document()->lane_count());
    }
}
