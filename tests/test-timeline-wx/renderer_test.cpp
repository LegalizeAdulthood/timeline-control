// Copyright (c) 2026 Richard Thomson

#include <wxTimeline/wxTimelineRenderer.h>

#include <timeline/DisplayList.h>

#include <gtest/gtest.h>

#include <wx/bitmap.h>
#include <wx/dcmemory.h>
#include <wx/image.h>
#include <wx/settings.h>

#include <utility>

namespace
{

const wxColour BACKGROUND(255, 255, 255);
const wxTimelinePalette PALETTE{BACKGROUND, wxColour(0, 0, 0), wxColour(0, 100, 200)};

wxColour pixel(const wxImage &image, int x, int y)
{
    return wxColour(image.GetRed(x, y), image.GetGreen(x, y), image.GetBlue(x, y));
}

bool contains_drawing(const wxImage &image, const wxRect &region)
{
    for (int y = region.GetTop(); y <= region.GetBottom(); ++y)
    {
        for (int x = region.GetLeft(); x <= region.GetRight(); ++x)
        {
            if (pixel(image, x, y) != BACKGROUND)
            {
                return true;
            }
        }
    }
    return false;
}

int vertical_coverage(const wxImage &image, int x, int top, int bottom)
{
    int covered = 0;
    for (int y = top; y <= bottom; ++y)
    {
        covered += pixel(image, x, y) == BACKGROUND ? 0 : 1;
    }
    return covered;
}

wxImage render(const timeline::DisplayList &display_list, wxPoint origin, const wxTimelineStyleColors &style_colors,
    int stroke_width, const wxRect &clip)
{
    wxBitmap bitmap(140, 50, 24);
    wxMemoryDC dc(bitmap);
    dc.SetBackground(wxBrush(BACKGROUND));
    dc.Clear();
    dc.SetFont(*wxNORMAL_FONT);
    dc.SetClippingRegion(clip);
    draw_timeline_display_list(dc, display_list, origin, PALETTE, style_colors, stroke_width, true);
    dc.SelectObject(wxNullBitmap);
    return bitmap.ConvertToImage();
}

} // namespace

TEST(WxRenderer, drawsEveryPrimitiveThroughNativeOperations)
{
    timeline::StringTableBuilder strings;
    const timeline::StringId value = strings.intern("X");
    timeline::DisplayList display_list(std::move(strings).build());
    display_list.add(timeline::Line{0, 0, 10, 0, timeline::StyleRole::RULER, {}});
    display_list.add(timeline::Rectangle{15, 0, 10, 10, timeline::StyleRole::INTERVAL_SPAN, {}});
    display_list.add(timeline::Marker{30, 0, 10, 10, timeline::StyleRole::INSTANT_MARKER, {}});
    display_list.add(timeline::Polyline{{{45, 0}, {50, 5}, {55, 0}}, timeline::StyleRole::CURVE, {}});
    display_list.add(timeline::Swatch{65, 0, 10, 10, timeline::RgbColor(7, 11, 19), timeline::StyleRole::PALETTE, {}});
    display_list.add(timeline::Text{85, 0, value, timeline::StyleRole::RULER_LABEL, {}});
    wxTimelineStyleColors style_colors;
    style_colors.emplace(timeline::StyleRole::RULER, wxColour(10, 20, 30));
    style_colors.emplace(timeline::StyleRole::INTERVAL_SPAN, wxColour(20, 30, 40));
    style_colors.emplace(timeline::StyleRole::INSTANT_MARKER, wxColour(30, 40, 50));
    style_colors.emplace(timeline::StyleRole::CURVE, wxColour(40, 50, 60));
    style_colors.emplace(timeline::StyleRole::PALETTE, wxColour(50, 60, 70));
    style_colors.emplace(timeline::StyleRole::RULER_LABEL, wxColour(60, 70, 80));

    const wxImage image = render(display_list, wxPoint(5, 5), style_colors, 1, wxRect(0, 0, 140, 50));

    ASSERT_TRUE(image.IsOk());
    EXPECT_TRUE(contains_drawing(image, wxRect(5, 4, 11, 3)));
    EXPECT_TRUE(contains_drawing(image, wxRect(20, 5, 10, 10)));
    EXPECT_TRUE(contains_drawing(image, wxRect(35, 5, 10, 10)));
    EXPECT_TRUE(contains_drawing(image, wxRect(50, 5, 11, 11)));
    EXPECT_TRUE(contains_drawing(image, wxRect(70, 5, 10, 10)));
    EXPECT_TRUE(contains_drawing(image, wxRect(90, 5, 20, 20)));
}

TEST(WxRenderer, preservesSourceRgb)
{
    timeline::DisplayList display_list;
    display_list.add(
        timeline::Swatch{10, 10, 20, 15, timeline::RgbColor(25, 140, 230), timeline::StyleRole::PALETTE, {}});

    const wxImage image = render(display_list, wxPoint(0, 0), {}, 1, wxRect(0, 0, 140, 50));

    ASSERT_TRUE(image.IsOk());
    EXPECT_EQ(wxColour(25, 140, 230), pixel(image, 20, 15));
}

TEST(WxRenderer, respectsExistingClippingRegion)
{
    timeline::DisplayList display_list;
    display_list.add(timeline::Rectangle{0, 0, 40, 20, timeline::StyleRole::INTERVAL_SPAN, {}});
    wxTimelineStyleColors style_colors;
    style_colors.emplace(timeline::StyleRole::INTERVAL_SPAN, wxColour(10, 20, 30));

    const wxImage image = render(display_list, wxPoint(0, 0), style_colors, 1, wxRect(0, 0, 20, 50));

    ASSERT_TRUE(image.IsOk());
    EXPECT_EQ(wxColour(10, 20, 30), pixel(image, 10, 10));
    EXPECT_EQ(BACKGROUND, pixel(image, 30, 10));
}

TEST(WxRenderer, translatesOriginAndScalesStrokeWidth)
{
    timeline::DisplayList display_list;
    display_list.add(timeline::Line{0, 0, 20, 0, timeline::StyleRole::RULER, {}});
    wxTimelineStyleColors style_colors;
    style_colors.emplace(timeline::StyleRole::RULER, wxColour(10, 20, 30));

    const wxImage image = render(display_list, wxPoint(10, 10), style_colors, 3, wxRect(0, 0, 140, 50));

    ASSERT_TRUE(image.IsOk());
    EXPECT_EQ(BACKGROUND, pixel(image, 5, 5));
    EXPECT_GE(vertical_coverage(image, 20, 7, 13), 2);
}
