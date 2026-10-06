// Copyright (c) 2026 Richard Thomson

#include <wxTimeline/wxCairoTimeline.h>
#include <wxTimeline/wxCairoTimelineRenderer.h>

#include <timelineParAnimator/TimelineJson.h>

#include <gtest/gtest.h>

#include <wx/app.h>
#include <wx/dcmemory.h>
#include <wx/frame.h>

#include <cstring>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{

const wxTimelinePalette LIGHT{wxColour(255, 255, 255), wxColour(0, 0, 0), wxColour(0, 100, 200)};
const wxTimelinePalette DARK{wxColour(24, 24, 24), wxColour(235, 235, 235), wxColour(70, 160, 230)};

/// Cairo renderer test with a canonical diagonal curve primitive.
///
class DiagonalCairoRendererTest : public testing::Test
{
protected:
    const timeline::Polyline m_diagonal{
        {{3, 4}, {27, 22}}, timeline::StyleRole::CURVE, {timeline::StringId{1}, timeline::StringId{2}}};
};

struct AlphaCounts
{
    int solid = 0;
    int partial = 0;
};

struct InteractionState
{
    timeline::Time playhead;
    timeline::Ticks first;
    timeline::Ticks last;
    timeline::Ticks frame;
};

wxColour pixel(const wxImage &image, int x, int y)
{
    return wxColour(image.GetRed(x, y), image.GetGreen(x, y), image.GetBlue(x, y));
}

AlphaCounts count_alpha(const wxImage &image)
{
    AlphaCounts result;
    for (int y = 0; y < image.GetHeight(); ++y)
    {
        for (int x = 0; x < image.GetWidth(); ++x)
        {
            const int alpha = image.GetAlpha(x, y);
            result.solid += alpha == 255;
            result.partial += alpha > 0 && alpha < 255;
        }
    }
    return result;
}

int count_covered(const wxImage &image)
{
    int result = 0;
    for (int y = 0; y < image.GetHeight(); ++y)
    {
        for (int x = 0; x < image.GetWidth(); ++x)
        {
            result += image.GetAlpha(x, y) > 0;
        }
    }
    return result;
}

bool same_rgb(const wxImage &lhs, const wxImage &rhs)
{
    if (lhs.GetWidth() != rhs.GetWidth() || lhs.GetHeight() != rhs.GetHeight())
    {
        return false;
    }
    const int pixels = lhs.GetWidth() * lhs.GetHeight();
    return std::memcmp(lhs.GetData(), rhs.GetData(), pixels * 3) == 0;
}

bool same_pixels(const wxImage &lhs, const wxImage &rhs)
{
    if (!same_rgb(lhs, rhs) || !lhs.HasAlpha() || !rhs.HasAlpha())
    {
        return false;
    }
    const int pixels = lhs.GetWidth() * lhs.GetHeight();
    return std::memcmp(lhs.GetAlpha(), rhs.GetAlpha(), pixels) == 0;
}

bool solid_pixels_match(const wxImage &image, const wxColour &expected)
{
    for (int y = 0; y < image.GetHeight(); ++y)
    {
        for (int x = 0; x < image.GetWidth(); ++x)
        {
            if (image.GetAlpha(x, y) == 255 && pixel(image, x, y) != expected)
            {
                return false;
            }
        }
    }
    return true;
}

timeline::DisplayList presentation_list()
{
    timeline::StringTableBuilder strings;
    const timeline::StringId text = strings.intern("Timeline");
    timeline::DisplayList result(std::move(strings).build());
    result.add(timeline::Rectangle{0, 0, 160, 80, timeline::StyleRole::LANE_BACKGROUND, {}});
    result.add(timeline::Text{4, 4, text, timeline::StyleRole::LANE_LABEL, {}});
    result.add(timeline::Swatch{8, 48, 12, 12, timeline::RgbColor(12, 34, 56), timeline::StyleRole::PALETTE, {}});
    return result;
}

/// Cairo renderer test with the canonical presentation inputs.
///
class PresentationCairoRendererTest : public testing::Test
{
protected:
    const timeline::DisplayList m_list = presentation_list();
    const wxTimelinePalette m_palette = LIGHT;
    const wxFont m_font = *wxNORMAL_FONT;
    const wxSize m_surface{160, 80};
    const wxRect m_clip{0, 0, 160, 80};
};

timeline::DisplayList filled_roles_list()
{
    timeline::DisplayList result;
    int x = 2;
    for (timeline::StyleRole role :
        {timeline::StyleRole::LANE_BACKGROUND, timeline::StyleRole::INTERVAL_SPAN, timeline::StyleRole::ENVELOPE_ATTACK,
            timeline::StyleRole::ENVELOPE_SUSTAIN, timeline::StyleRole::ENVELOPE_DECAY,
            timeline::StyleRole::SELECTED_LANE, timeline::StyleRole::SELECTED_RANGE})
    {
        result.add(timeline::Rectangle{x, 2, 6, 12, role, {}});
        x += 8;
    }
    result.add(timeline::Marker{2, 18, 6, 8, timeline::StyleRole::INSTANT_MARKER, {}});
    result.add(timeline::Marker{10, 18, 6, 8, timeline::StyleRole::KEYFRAME_MARKER, {}});
    result.add(timeline::Marker{18, 18, 6, 8, timeline::StyleRole::SELECTED_ITEM, {}});
    return result;
}

bool filled_roles_match(const timeline::DisplayList &list, const wxImage &image, const wxTimelinePalette &palette)
{
    int x = 2;
    for (int index = 0; index < 7; ++index)
    {
        const timeline::Rectangle &rectangle = std::get<timeline::Rectangle>(list.primitives()[index]);
        if (timeline_style_colour(rectangle.style, palette, true) != pixel(image, x + 2, 6) ||
            image.GetAlpha(x + 2, 6) != 255)
        {
            return false;
        }
        x += 8;
    }
    return timeline_style_colour(timeline::StyleRole::INSTANT_MARKER, palette, true) == pixel(image, 4, 22) &&
        timeline_style_colour(timeline::StyleRole::KEYFRAME_MARKER, palette, true) == pixel(image, 12, 22) &&
        timeline_style_colour(timeline::StyleRole::SELECTED_ITEM, palette, true) == pixel(image, 20, 22);
}

timeline::Document import_document(const std::filesystem::path &path)
{
    timeline_par_animator::JsonImportResult result = timeline_par_animator::import_timeline_json(path);
    if (!result.succeeded())
    {
        throw std::runtime_error("failed to import Cairo test fixture");
    }
    return std::move(*result.document);
}

timeline::Document animation_document()
{
    return import_document(std::filesystem::path(TIMELINE_FIXTURE_DIR) / "extreme-normalized-vectors.json");
}

timeline::Document mapping_document()
{
    return import_document(std::filesystem::path(TIMELINE_FIXTURE_DIR) / "beat-keys/rms.beat-keys.json");
}

timeline::Document palette_document()
{
    return import_document(std::filesystem::path(TIMELINE_FIXTURE_DIR) / "color-map-mixed.json");
}

timeline::Document combined_document()
{
    return timeline::combine_documents(animation_document(), mapping_document());
}

void refresh_layout(wxTimelineControl &control)
{
    control.snapshot();
}

wxCairoTimeline &show_control(wxFrame &frame, timeline::Document document)
{
    wxCairoTimeline &control = *new wxCairoTimeline(&frame);
    control.SetSize(600, 400);
    control.set_document(std::move(document));
    frame.Show();
    refresh_layout(control);
    control.SetFocus();
    refresh_layout(control);
    return control;
}

InteractionState select_first_frame(wxCairoTimeline &control)
{
    control.zoom_in();
    refresh_layout(control);
    control.step_playhead(1, true);
    refresh_layout(control);
    if (!control.interaction()->playhead() || !control.interaction()->selected_frames() || !control.inspection())
    {
        throw std::runtime_error("failed to select a frame in Cairo test fixture");
    }
    return {*control.interaction()->playhead(), control.interaction()->selected_frames()->first(),
        control.interaction()->selected_frames()->last(), control.inspection()->frame};
}

void expect_interaction(const wxCairoTimeline &control, const InteractionState &expected)
{
    ASSERT_TRUE(control.interaction()->selected_frames());
    ASSERT_TRUE(control.inspection());
    EXPECT_EQ(expected.playhead, control.interaction()->playhead());
    EXPECT_EQ(expected.first, control.interaction()->selected_frames()->first());
    EXPECT_EQ(expected.last, control.interaction()->selected_frames()->last());
    EXPECT_EQ(expected.frame, control.inspection()->frame);
}

/// Shown Cairo control displaying the shared combined document.
///
class CairoControlTest : public testing::Test
{
protected:
    CairoControlTest();
    void TearDown() override;

    const timeline::Document m_document;
    wxFrame &m_frame;
    wxCairoTimeline &m_control;
};

/// Shown Cairo control with its first frame selected.
///
class SelectedCairoControlTest : public CairoControlTest
{
protected:
    SelectedCairoControlTest();

    const InteractionState m_before;
};

/// Selected Cairo control with changed size and theme presentation.
///
class PresentationChangedCairoControlTest : public SelectedCairoControlTest
{
protected:
    PresentationChangedCairoControlTest();
};

CairoControlTest::CairoControlTest() :
    m_document(combined_document()),
    m_frame(*new wxFrame(nullptr, wxID_ANY, "Cairo control test", wxDefaultPosition, wxSize(640, 480))),
    m_control(show_control(m_frame, m_document))
{
}

void CairoControlTest::TearDown()
{
    m_frame.Destroy();
    wxTheApp->ProcessPendingEvents();
}

SelectedCairoControlTest::SelectedCairoControlTest() :
    m_before(select_first_frame(m_control))
{
}

PresentationChangedCairoControlTest::PresentationChangedCairoControlTest()
{
    m_control.SetSize(440, 280);
    m_control.SetBackgroundColour(DARK.background);
    m_control.SetForegroundColour(DARK.foreground);
}

bool partial_repaints_match(const timeline::DisplayList &list, const wxRect &clip)
{
    const auto paint = [&](wxRect paint_clip, double scale)
    {
        wxBitmap bitmap(96, 64, 24);
        wxMemoryDC dc(bitmap);
        dc.SetFont(*wxNORMAL_FONT);
        dc.SetBackground(*wxWHITE_BRUSH);
        dc.Clear();
        dc.SetClippingRegion(paint_clip);
        draw_cairo_timeline_display_list(dc, list, wxPoint(0, 0), LIGHT, 1, true, wxSize(96, 64), scale);
        dc.SelectObject(wxNullBitmap);
        return bitmap.ConvertToImage();
    };
    for (double scale : {1.0, 1.25, 1.5, 2.0})
    {
        const wxImage full = paint(wxRect(0, 0, 96, 64), scale);
        const wxImage partial = paint(clip, scale);
        for (int y = 0; y < 64; ++y)
        {
            for (int x = 0; x < 96; ++x)
            {
                if ((clip.Contains(x, y) && pixel(full, x, y) != pixel(partial, x, y)) ||
                    (!clip.Contains(x, y) && pixel(partial, x, y) != LIGHT.background))
                {
                    return false;
                }
            }
        }
    }
    return true;
}

bool repeated_surface_changes_restore(const timeline::DisplayList &list, const wxFont &font, const wxImage &baseline)
{
    for (int pass = 0; pass < 12; ++pass)
    {
        wxFont larger_font(font);
        larger_font.SetFractionalPointSize(font.GetFractionalPointSize() * 1.5);
        const wxImage changed = render_cairo_display_list(
            list, wxSize(120, 64), wxPoint(0, 0), wxRect(0, 0, 120, 64), DARK, 2, false, larger_font, 1.5);
        const wxImage restored = render_cairo_display_list(
            list, wxSize(160, 80), wxPoint(0, 0), wxRect(0, 0, 160, 80), LIGHT, 1, true, font, 1.0);
        if (!changed.IsOk() || !restored.IsOk() || !same_pixels(baseline, restored))
        {
            return false;
        }
    }
    return true;
}

bool complete_control_lifetimes(const timeline::Document &document)
{
    for (int lifetime = 0; lifetime < 3; ++lifetime)
    {
        wxFrame frame(nullptr, wxID_ANY, "Cairo lifecycle test", wxDefaultPosition, wxSize(640, 480));
        wxCairoTimeline &control = show_control(frame, document);
        control.set_cairo_enabled(lifetime % 2 == 0);
        refresh_layout(control);
        if (control.snapshot().empty())
        {
            return false;
        }
    }
    return true;
}

} // namespace

TEST_F(DiagonalCairoRendererTest, drawsNonblankAntialiasedCurvesInLightColours)
{
    const wxColour colour = timeline_style_colour(m_diagonal.style, LIGHT, true);

    const wxImage image =
        render_cairo_curve(m_diagonal, wxSize(32, 28), wxPoint(0, 0), wxRect(0, 0, 32, 28), colour, 1, 1.0);
    const AlphaCounts counts = count_alpha(image);

    ASSERT_TRUE(image.IsOk());
    ASSERT_TRUE(image.HasAlpha());
    EXPECT_GT(counts.solid, 0);
    EXPECT_GT(counts.partial, 0);
    EXPECT_TRUE(solid_pixels_match(image, colour));
    EXPECT_EQ(0, image.GetAlpha(0, 27));
}

TEST_F(DiagonalCairoRendererTest, drawsNonblankAntialiasedCurvesInDarkColours)
{
    const wxColour colour = timeline_style_colour(m_diagonal.style, DARK, true);

    const wxImage image =
        render_cairo_curve(m_diagonal, wxSize(32, 28), wxPoint(0, 0), wxRect(0, 0, 32, 28), colour, 1, 1.0);
    const AlphaCounts counts = count_alpha(image);

    ASSERT_TRUE(image.IsOk());
    ASSERT_TRUE(image.HasAlpha());
    EXPECT_GT(counts.solid, 0);
    EXPECT_GT(counts.partial, 0);
    EXPECT_TRUE(solid_pixels_match(image, colour));
    EXPECT_EQ(0, image.GetAlpha(0, 27));
}

TEST_F(DiagonalCairoRendererTest, clipsTranslatedCurves)
{
    const wxRect clip(10, 8, 12, 10);

    const wxImage image = render_cairo_curve(m_diagonal, wxSize(32, 28), wxPoint(2, 1), clip, LIGHT.foreground, 1, 1.0);

    ASSERT_TRUE(image.IsOk());
    EXPECT_GT(count_covered(image), 0);
    for (int y = 0; y < image.GetHeight(); ++y)
    {
        for (int x = 0; x < image.GetWidth(); ++x)
        {
            if (image.GetAlpha(x, y) > 0)
            {
                EXPECT_TRUE(clip.Contains(x, y));
            }
        }
    }
}

TEST_F(DiagonalCairoRendererTest, scalesCurveDevicePixels)
{
    const wxRect clip(0, 0, 32, 28);

    const wxImage image = render_cairo_curve(m_diagonal, wxSize(32, 28), wxPoint(0, 0), clip, LIGHT.foreground, 1, 2.0);

    ASSERT_TRUE(image.IsOk());
    EXPECT_EQ(64, image.GetWidth());
    EXPECT_EQ(56, image.GetHeight());
    EXPECT_GT(count_covered(image), 0);
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

TEST_F(DiagonalCairoRendererTest, fallsBackToNativeWhenDeviceScaleIsInvalid)
{
    timeline::DisplayList list;
    list.add(m_diagonal);
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

    EXPECT_TRUE(same_rgb(expected, actual));
}

TEST(CairoRenderer, ignoresEmptyCurves)
{
    const timeline::Polyline curve{{}, timeline::StyleRole::CURVE, {}};

    const wxImage image =
        render_cairo_curve(curve, wxSize(16, 16), wxPoint(0, 0), wxRect(0, 0, 16, 16), LIGHT.foreground, 1, 1.0);

    ASSERT_TRUE(image.IsOk());
    EXPECT_EQ(0, count_covered(image));
}

TEST(CairoRenderer, ignoresSinglePointCurves)
{
    const timeline::Polyline curve{{{5, 5}}, timeline::StyleRole::KEYFRAME_SEGMENT, {}};

    const wxImage image =
        render_cairo_curve(curve, wxSize(16, 16), wxPoint(0, 0), wxRect(0, 0, 16, 16), LIGHT.foreground, 1, 1.0);

    ASSERT_TRUE(image.IsOk());
    EXPECT_EQ(0, count_covered(image));
}

TEST_F(DiagonalCairoRendererTest, preservesPrimitiveOrder)
{
    timeline::DisplayList list;
    list.add(timeline::Rectangle{0, 0, 32, 28, timeline::StyleRole::LANE_BACKGROUND, {}});
    list.add(m_diagonal);
    list.add(timeline::Marker{14, 11, 6, 6, timeline::StyleRole::KEYFRAME_MARKER, {}});

    const wxImage image = render_cairo_display_list(
        list, wxSize(32, 28), wxPoint(0, 0), wxRect(0, 0, 32, 28), LIGHT, 1, true, *wxNORMAL_FONT, 1.0);

    ASSERT_TRUE(image.IsOk());
    EXPECT_EQ(timeline_style_colour(timeline::StyleRole::KEYFRAME_MARKER, LIGHT, true), pixel(image, 16, 13));
}

TEST_F(DiagonalCairoRendererTest, blendsAntialiasedCurvesWhenPresenting)
{
    timeline::DisplayList list;
    list.add(timeline::Rectangle{0, 0, 32, 28, timeline::StyleRole::LANE_BACKGROUND, {}});
    list.add(m_diagonal);
    const wxColour background = timeline_style_colour(timeline::StyleRole::LANE_BACKGROUND, LIGHT, true);
    const wxColour curve = timeline_style_colour(timeline::StyleRole::CURVE, LIGHT, true);

    const wxImage image = render_cairo_display_list(
        list, wxSize(32, 28), wxPoint(0, 0), wxRect(0, 0, 32, 28), LIGHT, 1, true, *wxNORMAL_FONT, 1.0);

    ASSERT_TRUE(image.IsOk());
    bool blended = false;
    for (int y = 4; y < 10; ++y)
    {
        for (int x = 3; x < 13; ++x)
        {
            const wxColour value = pixel(image, x, y);
            blended = blended || (value != background && value != curve);
        }
    }
    EXPECT_TRUE(blended);
}

TEST(CairoRenderer, preservesSourceRgb)
{
    timeline::DisplayList list;
    list.add(timeline::Swatch{2, 23, 5, 4, timeline::RgbColor(12, 34, 56), timeline::StyleRole::PALETTE, {}});

    const wxImage image = render_cairo_display_list(
        list, wxSize(32, 28), wxPoint(0, 0), wxRect(0, 0, 32, 28), LIGHT, 1, true, *wxNORMAL_FONT, 1.0);

    ASSERT_TRUE(image.IsOk());
    EXPECT_EQ(wxColour(12, 34, 56), pixel(image, 4, 25));
}

TEST(CairoRenderer, drawsEveryFilledRole)
{
    const timeline::DisplayList list = filled_roles_list();

    const wxImage image = render_cairo_display_list(
        list, wxSize(64, 32), wxPoint(0, 0), wxRect(0, 0, 64, 32), LIGHT, 1, true, *wxNORMAL_FONT, 1.0);

    ASSERT_TRUE(image.IsOk());
    EXPECT_TRUE(filled_roles_match(list, image, LIGHT));
}

TEST(CairoRenderer, drawsFilledRolesWithDarkTheme)
{
    const timeline::DisplayList list = filled_roles_list();

    const wxImage image = render_cairo_display_list(
        list, wxSize(64, 32), wxPoint(0, 0), wxRect(0, 0, 64, 32), DARK, 1, true, *wxNORMAL_FONT, 1.0);

    ASSERT_TRUE(image.IsOk());
    EXPECT_TRUE(filled_roles_match(list, image, DARK));
}

TEST(CairoRenderer, drawsLaterPrimitivesOverEarlierPrimitives)
{
    timeline::DisplayList list;
    list.add(timeline::Rectangle{40, 18, 12, 8, timeline::StyleRole::SELECTED_RANGE, {}});
    list.add(timeline::Marker{44, 20, 4, 4, timeline::StyleRole::INSTANT_MARKER, {}});

    const wxImage image = render_cairo_display_list(
        list, wxSize(64, 32), wxPoint(0, 0), wxRect(0, 0, 64, 32), LIGHT, 1, true, *wxNORMAL_FONT, 1.0);

    ASSERT_TRUE(image.IsOk());
    EXPECT_EQ(timeline_style_colour(timeline::StyleRole::INSTANT_MARKER, LIGHT, true), pixel(image, 45, 21));
}

TEST(CairoRenderer, clipsLinePrimitives)
{
    timeline::DisplayList list;
    list.add(timeline::Line{0, 0, 31, 27, timeline::StyleRole::RULER, {}});
    list.add(timeline::Line{20, 0, 20, 27, timeline::StyleRole::PLAYHEAD, {}});
    list.add(timeline::Polyline{{{0, 24}, {31, 8}}, timeline::StyleRole::KEYFRAME_SEGMENT, {}});
    const wxRect clip(5, 4, 24, 20);

    const wxImage image =
        render_cairo_display_list(list, wxSize(36, 32), wxPoint(2, 1), clip, LIGHT, 1, true, *wxNORMAL_FONT, 2.0);

    ASSERT_TRUE(image.IsOk());
    for (int y = 0; y < image.GetHeight(); ++y)
    {
        for (int x = 0; x < image.GetWidth(); ++x)
        {
            if (image.GetAlpha(x, y) > 0)
            {
                EXPECT_TRUE(clip.Contains(x / 2, y / 2));
            }
        }
    }
}

TEST(CairoRenderer, antialiasesLinePrimitives)
{
    timeline::DisplayList list;
    list.add(timeline::Line{0, 0, 31, 27, timeline::StyleRole::RULER, {}});
    list.add(timeline::Polyline{{{0, 24}, {31, 8}}, timeline::StyleRole::KEYFRAME_SEGMENT, {}});

    const wxImage image = render_cairo_display_list(
        list, wxSize(36, 32), wxPoint(0, 0), wxRect(0, 0, 36, 32), LIGHT, 1, true, *wxNORMAL_FONT, 2.0);
    const AlphaCounts counts = count_alpha(image);

    ASSERT_TRUE(image.IsOk());
    EXPECT_GT(counts.solid + counts.partial, 0);
    EXPECT_GT(counts.partial, 0);
}

TEST(CairoRenderer, appliesPlayheadSemanticColour)
{
    timeline::DisplayList list;
    list.add(timeline::Line{20, 0, 20, 27, timeline::StyleRole::PLAYHEAD, {}});

    const wxImage image = render_cairo_display_list(
        list, wxSize(36, 32), wxPoint(2, 1), wxRect(0, 0, 36, 32), LIGHT, 1, true, *wxNORMAL_FONT, 2.0);

    ASSERT_TRUE(image.IsOk());
    EXPECT_EQ(timeline_style_colour(timeline::StyleRole::PLAYHEAD, LIGHT, true), pixel(image, 44, 12));
}

TEST(CairoRenderer, compositesNativeFontCoverage)
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
    timeline::StringTableBuilder strings;
    const timeline::StringId text = strings.intern("Timeline");
    timeline::DisplayList list(std::move(strings).build());
    list.add(timeline::Text{3, 2, text, timeline::StyleRole::LANE_LABEL, {}});

    const wxImage image = render_cairo_display_list(
        list, wxSize(96, 40), wxPoint(0, 0), wxRect(0, 0, 96, 40), DARK, 1, false, *wxNORMAL_FONT, 1.0);

    ASSERT_TRUE(image.IsOk());
    EXPECT_GT(count_covered(image), 0);
    for (int y = 0; y < 20; ++y)
    {
        for (int x = 0; x < 96; ++x)
        {
            const bool native_covered = native.GetRed(x, y) || native.GetGreen(x, y) || native.GetBlue(x, y);
            EXPECT_EQ(native_covered, image.GetAlpha(x, y) > 0);
            const int coverage = (native.GetRed(x, y) + native.GetGreen(x, y) + native.GetBlue(x, y) + 2) / 3;
            EXPECT_EQ(coverage, image.GetAlpha(x, y));
        }
    }
}

TEST(CairoRenderer, clipsNativeFontCoverage)
{
    timeline::StringTableBuilder strings;
    const timeline::StringId text = strings.intern("Timeline");
    timeline::DisplayList list(std::move(strings).build());
    list.add(timeline::Text{3, 2, text, timeline::StyleRole::LANE_LABEL, {}});

    const wxImage image = render_cairo_display_list(
        list, wxSize(96, 40), wxPoint(0, 0), wxRect(0, 0, 16, 40), DARK, 1, false, *wxNORMAL_FONT, 1.0);

    ASSERT_TRUE(image.IsOk());
    EXPECT_GT(count_covered(image), 0);
    for (int y = 0; y < 40; ++y)
    {
        for (int x = 16; x < 96; ++x)
        {
            EXPECT_EQ(0, image.GetAlpha(x, y));
        }
    }
}

TEST(CairoRenderer, drawsMarkersOverText)
{
    timeline::StringTableBuilder strings;
    const timeline::StringId text = strings.intern("Timeline");
    timeline::DisplayList list(std::move(strings).build());
    list.add(timeline::Text{3, 2, text, timeline::StyleRole::LANE_LABEL, {}});
    list.add(timeline::Marker{20, 0, 5, 40, timeline::StyleRole::SELECTED_ITEM, {}});

    const wxImage image = render_cairo_display_list(
        list, wxSize(96, 40), wxPoint(0, 0), wxRect(0, 0, 96, 40), DARK, 1, false, *wxNORMAL_FONT, 1.0);

    ASSERT_TRUE(image.IsOk());
    for (int y = 0; y < 20; ++y)
    {
        for (int x = 20; x < 25; ++x)
        {
            EXPECT_EQ(timeline_style_colour(timeline::StyleRole::SELECTED_ITEM, DARK, false), pixel(image, x, y));
        }
    }
}

TEST_F(CairoControlTest, switchesRenderer)
{
    m_control.set_cairo_enabled(false);
    refresh_layout(m_control);

    EXPECT_FALSE(m_control.cairo_enabled());
}

TEST_F(CairoControlTest, enablesCairoRenderer)
{
    m_control.set_cairo_enabled(false);

    m_control.set_cairo_enabled(true);
    refresh_layout(m_control);

    EXPECT_TRUE(m_control.cairo_enabled());
}

TEST_F(CairoControlTest, rendererSwitchPreservesDocument)
{
    m_control.set_cairo_enabled(false);
    refresh_layout(m_control);

    ASSERT_TRUE(m_control.document());
    EXPECT_EQ(29, m_control.document()->lane_count());
}

TEST_F(CairoControlTest, rendererSwitchPreservesSnapshot)
{
    const std::string before = m_control.snapshot();

    m_control.set_cairo_enabled(false);
    refresh_layout(m_control);

    EXPECT_EQ(before, m_control.snapshot());
}

TEST_F(SelectedCairoControlTest, rendererSwitchPreservesInteraction)
{
    m_control.set_cairo_enabled(false);
    refresh_layout(m_control);

    expect_interaction(m_control, m_before);
}

TEST_F(SelectedCairoControlTest, replacementResetsInteraction)
{
    m_control.set_document(animation_document());
    refresh_layout(m_control);

    ASSERT_TRUE(m_control.document());
    EXPECT_EQ(25, m_control.document()->lane_count());
    EXPECT_FALSE(m_control.interaction()->selected_frames());
}

TEST(CairoRenderer, partialRepaintsPreserveDevicePixelAlignment)
{
    timeline::StringTableBuilder strings;
    const timeline::StringId text = strings.intern("Timeline");
    timeline::DisplayList list(std::move(strings).build());
    list.add(timeline::Rectangle{0, 0, 96, 64, timeline::StyleRole::LANE_BACKGROUND, {}});
    list.add(timeline::Polyline{{{0, 0}, {95, 63}}, timeline::StyleRole::CURVE, {}});
    list.add(timeline::Line{19, 0, 19, 63, timeline::StyleRole::PLAYHEAD, {}});
    list.add(timeline::Text{8, 8, text, timeline::StyleRole::LANE_LABEL, {}});
    list.add(timeline::Swatch{40, 28, 20, 16, timeline::RgbColor(12, 34, 56), timeline::StyleRole::PALETTE, {}});
    const wxRect clip(7, 5, 70, 48);

    const bool preserved = partial_repaints_match(list, clip);

    EXPECT_TRUE(preserved);
}

TEST_F(PresentationCairoRendererTest, scalesSurfaceDimensions)
{
    const wxSize surface(120, 64);
    const wxRect clip(0, 0, 120, 64);

    const wxImage image =
        render_cairo_display_list(m_list, surface, wxPoint(0, 0), clip, m_palette, 1, true, m_font, 1.5);

    ASSERT_TRUE(image.IsOk());
    EXPECT_EQ(180, image.GetWidth());
    EXPECT_EQ(96, image.GetHeight());
}

TEST_F(PresentationCairoRendererTest, appliesChangedTheme)
{
    const wxImage image =
        render_cairo_display_list(m_list, m_surface, wxPoint(0, 0), m_clip, DARK, 1, false, m_font, 1.0);

    ASSERT_TRUE(image.IsOk());
    EXPECT_EQ(timeline_style_colour(timeline::StyleRole::LANE_BACKGROUND, DARK, false), pixel(image, 110, 60));
    EXPECT_EQ(wxColour(12, 34, 56), pixel(image, 12, 54));
}

TEST_F(PresentationCairoRendererTest, appliesChangedFont)
{
    timeline::StringTableBuilder strings;
    const timeline::StringId text = strings.intern("Timeline");
    timeline::DisplayList list(std::move(strings).build());
    list.add(timeline::Text{4, 4, text, timeline::StyleRole::LANE_LABEL, {}});
    wxFont larger_font(m_font);
    larger_font.SetFractionalPointSize(m_font.GetFractionalPointSize() * 1.5);

    const wxImage small_image =
        render_cairo_display_list(list, m_surface, wxPoint(0, 0), m_clip, m_palette, 1, true, m_font, 1.0);
    const wxImage large_image =
        render_cairo_display_list(list, m_surface, wxPoint(0, 0), m_clip, m_palette, 1, true, larger_font, 1.0);

    ASSERT_TRUE(small_image.IsOk());
    ASSERT_TRUE(large_image.IsOk());
    EXPECT_NE(0, std::memcmp(small_image.GetAlpha(), large_image.GetAlpha(), 160 * 80));
}

TEST_F(PresentationCairoRendererTest, appliesChangedSize)
{
    const wxSize surface(120, 64);
    const wxRect clip(0, 0, 120, 64);

    const wxImage image =
        render_cairo_display_list(m_list, surface, wxPoint(0, 0), clip, m_palette, 1, true, m_font, 1.0);

    ASSERT_TRUE(image.IsOk());
    EXPECT_EQ(120, image.GetWidth());
    EXPECT_EQ(64, image.GetHeight());
}

TEST_F(PresentationCairoRendererTest, repeatedSurfaceChangesRestoreOriginalPixels)
{
    const wxImage baseline =
        render_cairo_display_list(m_list, m_surface, wxPoint(0, 0), m_clip, m_palette, 1, true, m_font, 1.0);

    const bool restored = repeated_surface_changes_restore(m_list, m_font, baseline);

    EXPECT_TRUE(restored);
}

TEST_F(DiagonalCairoRendererTest, rejectsNonpositiveDeviceScales)
{
    timeline::DisplayList list;
    list.add(m_diagonal);

    const wxImage zero = render_cairo_display_list(
        list, wxSize(32, 28), wxPoint(0, 0), wxRect(0, 0, 32, 28), LIGHT, 1, true, *wxNORMAL_FONT, 0.0);
    const wxImage negative = render_cairo_display_list(
        list, wxSize(32, 28), wxPoint(0, 0), wxRect(0, 0, 32, 28), LIGHT, 1, true, *wxNORMAL_FONT, -1.0);

    EXPECT_FALSE(zero.IsOk());
    EXPECT_FALSE(negative.IsOk());
}

TEST_F(DiagonalCairoRendererTest, rejectsNonfiniteDeviceScales)
{
    timeline::DisplayList list;
    list.add(m_diagonal);

    const wxImage infinite = render_cairo_display_list(list, wxSize(32, 28), wxPoint(0, 0), wxRect(0, 0, 32, 28), LIGHT,
        1, true, *wxNORMAL_FONT, std::numeric_limits<double>::infinity());
    const wxImage nan = render_cairo_display_list(list, wxSize(32, 28), wxPoint(0, 0), wxRect(0, 0, 32, 28), LIGHT, 1,
        true, *wxNORMAL_FONT, std::numeric_limits<double>::quiet_NaN());

    EXPECT_FALSE(infinite.IsOk());
    EXPECT_FALSE(nan.IsOk());
}

TEST_F(DiagonalCairoRendererTest, rejectsOverflowingDeviceScales)
{
    timeline::DisplayList list;
    list.add(m_diagonal);

    const wxImage image = render_cairo_display_list(list, wxSize(32, 28), wxPoint(0, 0), wxRect(0, 0, 32, 28), LIGHT, 1,
        true, *wxNORMAL_FONT, std::numeric_limits<double>::max());

    EXPECT_FALSE(image.IsOk());
}

TEST_F(DiagonalCairoRendererTest, rejectsEmptySurfaceSizes)
{
    timeline::DisplayList list;
    list.add(m_diagonal);

    const wxImage image = render_cairo_display_list(
        list, wxSize(0, 28), wxPoint(0, 0), wxRect(0, 0, 32, 28), LIGHT, 1, true, *wxNORMAL_FONT, 1.0);

    EXPECT_FALSE(image.IsOk());
}

TEST_F(DiagonalCairoRendererTest, recoversAfterInvalidSurfaceRequest)
{
    timeline::DisplayList list;
    list.add(m_diagonal);
    const wxImage invalid = render_cairo_display_list(
        list, wxSize(0, 28), wxPoint(0, 0), wxRect(0, 0, 32, 28), LIGHT, 1, true, *wxNORMAL_FONT, 1.0);

    const wxImage recovered = render_cairo_display_list(
        list, wxSize(32, 28), wxPoint(0, 0), wxRect(0, 0, 32, 28), LIGHT, 1, true, *wxNORMAL_FONT, 1.0);

    EXPECT_FALSE(invalid.IsOk());
    EXPECT_TRUE(recovered.IsOk());
}

TEST_F(SelectedCairoControlTest, resizePreservesInteraction)
{
    m_control.SetSize(440, 280);
    m_control.Refresh(false);
    refresh_layout(m_control);

    expect_interaction(m_control, m_before);
}

TEST_F(SelectedCairoControlTest, fontChangePreservesInteraction)
{
    wxFont larger_font(m_control.GetFont());
    larger_font.SetFractionalPointSize(m_control.GetFont().GetFractionalPointSize() * 1.5);

    m_control.SetFont(larger_font);
    m_control.Refresh(false);
    refresh_layout(m_control);

    expect_interaction(m_control, m_before);
}

TEST_F(SelectedCairoControlTest, themeChangePreservesInteraction)
{
    m_control.SetBackgroundColour(DARK.background);
    m_control.SetForegroundColour(DARK.foreground);
    m_control.Refresh(false);
    refresh_layout(m_control);

    expect_interaction(m_control, m_before);
}

TEST_F(SelectedCairoControlTest, dpiChangePreservesInteraction)
{
    wxDPIChangedEvent dpi(wxSize(96, 96), wxSize(144, 144));

    m_control.ProcessWindowEvent(dpi);
    refresh_layout(m_control);

    expect_interaction(m_control, m_before);
}

TEST_F(SelectedCairoControlTest, systemColourChangePreservesInteraction)
{
    wxSysColourChangedEvent theme;

    m_control.ProcessWindowEvent(theme);
    refresh_layout(m_control);

    expect_interaction(m_control, m_before);
}

TEST_F(PresentationChangedCairoControlTest, presentationChangesPreserveHitIdentity)
{
    wxMouseEvent motion(wxEVT_MOTION);
    motion.SetPosition(wxPoint(4, m_control.FromDIP(40)));

    m_control.ProcessWindowEvent(motion);

    ASSERT_TRUE(m_control.hit_result());
    EXPECT_EQ("animation-0[0]", m_control.document()->strings().lookup(m_control.hit_result()->id.lane_id));
}

TEST_F(PresentationChangedCairoControlTest, paletteReplacementWorksAfterPresentationChanges)
{
    m_control.set_document(palette_document());
    refresh_layout(m_control);

    ASSERT_TRUE(m_control.document());
    EXPECT_EQ(1, m_control.document()->lane_count());
    EXPECT_FALSE(m_control.interaction()->selected_frames());
    EXPECT_NE(std::string::npos, m_control.snapshot().find("swatch PALETTE"));
}

TEST_F(PresentationChangedCairoControlTest, animationReplacementWorksAfterPresentationChanges)
{
    m_control.set_document(palette_document());

    m_control.set_document(animation_document());
    refresh_layout(m_control);

    ASSERT_TRUE(m_control.document());
    EXPECT_EQ(25, m_control.document()->lane_count());
}

TEST(CairoControl, supportsRepeatedDestruction)
{
    const timeline::Document document = combined_document();

    const bool completed = complete_control_lifetimes(document);

    EXPECT_TRUE(completed);
}
