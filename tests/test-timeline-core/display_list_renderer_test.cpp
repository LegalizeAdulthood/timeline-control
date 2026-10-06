// Copyright (c) 2026 Richard Thomson

#include <timeline/DisplayListRenderer.h>
#include <timeline/size_cast.h>

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace timeline;

namespace
{

/// Records display-list rendering calls for core dispatch tests.
///
class RecordingRenderer final : public DisplayListRenderer
{
public:
    void draw_line(const Line &) override
    {
        m_calls.emplace_back("line");
    }

    void fill_rectangle(const Rectangle &) override
    {
        m_calls.emplace_back("rectangle");
    }

    void draw_text(const Text &, std::string_view value) override
    {
        m_calls.emplace_back("text");
        m_text = value;
    }

    void draw_marker(const Marker &) override
    {
        m_calls.emplace_back("marker");
    }

    void draw_polyline(const Polyline &polyline) override
    {
        m_calls.emplace_back("polyline");
        m_point_count = size_cast(polyline.points);
    }

    void draw_swatch(const Swatch &swatch) override
    {
        m_calls.emplace_back("swatch");
        m_swatch_color = swatch.color;
    }

    const std::vector<std::string> &calls() const
    {
        return m_calls;
    }

    const std::string &text() const
    {
        return m_text;
    }

    int point_count() const
    {
        return m_point_count;
    }

    const std::optional<RgbColor> &swatch_color() const
    {
        return m_swatch_color;
    }

private:
    std::vector<std::string> m_calls;
    std::string m_text;
    int m_point_count{-1};
    std::optional<RgbColor> m_swatch_color;
};

} // namespace

TEST(DisplayListRenderer, dispatchesLine)
{
    DisplayList display_list;
    display_list.add(Line{1, 2, 3, 4, StyleRole::RULER, {}});
    RecordingRenderer renderer;

    render_display_list(renderer, display_list);

    ASSERT_EQ(1, size_cast(renderer.calls()));
    EXPECT_EQ("line", renderer.calls().front());
}

TEST(DisplayListRenderer, dispatchesRectangle)
{
    DisplayList display_list;
    display_list.add(Rectangle{1, 2, 3, 4, StyleRole::LANE_BACKGROUND, {}});
    RecordingRenderer renderer;

    render_display_list(renderer, display_list);

    ASSERT_EQ(1, size_cast(renderer.calls()));
    EXPECT_EQ("rectangle", renderer.calls().front());
}

TEST(DisplayListRenderer, dispatchesText)
{
    DisplayList display_list;
    display_list.add(Text{1, 2, {}, StyleRole::RULER_LABEL, {}});
    RecordingRenderer renderer;

    render_display_list(renderer, display_list);

    ASSERT_EQ(1, size_cast(renderer.calls()));
    EXPECT_EQ("text", renderer.calls().front());
}

TEST(DisplayListRenderer, dispatchesMarker)
{
    DisplayList display_list;
    display_list.add(Marker{1, 2, 3, 4, StyleRole::INSTANT_MARKER, {}});
    RecordingRenderer renderer;

    render_display_list(renderer, display_list);

    ASSERT_EQ(1, size_cast(renderer.calls()));
    EXPECT_EQ("marker", renderer.calls().front());
}

TEST(DisplayListRenderer, dispatchesPolyline)
{
    DisplayList display_list;
    display_list.add(Polyline{{{1, 2}, {3, 4}}, StyleRole::CURVE, {}});
    RecordingRenderer renderer;

    render_display_list(renderer, display_list);

    ASSERT_EQ(1, size_cast(renderer.calls()));
    EXPECT_EQ("polyline", renderer.calls().front());
}

TEST(DisplayListRenderer, dispatchesSwatch)
{
    DisplayList display_list;
    display_list.add(Swatch{1, 2, 3, 4, RgbColor(5, 6, 7), StyleRole::PALETTE, {}});
    RecordingRenderer renderer;

    render_display_list(renderer, display_list);

    ASSERT_EQ(1, size_cast(renderer.calls()));
    EXPECT_EQ("swatch", renderer.calls().front());
}

TEST(DisplayListRenderer, preservesPrimitiveOrder)
{
    DisplayList display_list;
    display_list.add(Line{1, 2, 3, 4, StyleRole::RULER, {}});
    display_list.add(Rectangle{1, 2, 3, 4, StyleRole::LANE_BACKGROUND, {}});
    display_list.add(Text{1, 2, {}, StyleRole::RULER_LABEL, {}});
    display_list.add(Marker{1, 2, 3, 4, StyleRole::INSTANT_MARKER, {}});
    display_list.add(Polyline{{{1, 2}, {3, 4}}, StyleRole::CURVE, {}});
    display_list.add(Swatch{1, 2, 3, 4, RgbColor(5, 6, 7), StyleRole::PALETTE, {}});
    const std::vector<std::string> expected{"line", "rectangle", "text", "marker", "polyline", "swatch"};
    RecordingRenderer renderer;

    render_display_list(renderer, display_list);

    EXPECT_EQ(expected, renderer.calls());
}

TEST(DisplayListRenderer, resolvesText)
{
    StringTableBuilder strings;
    const StringId value = strings.intern("resolved text");
    DisplayList display_list(std::move(strings).build());
    display_list.add(Text{1, 2, value, StyleRole::RULER_LABEL, {}});
    RecordingRenderer renderer;

    render_display_list(renderer, display_list);

    EXPECT_EQ("resolved text", renderer.text());
}

TEST(DisplayListRenderer, dispatchesEmptyPolyline)
{
    DisplayList display_list;
    display_list.add(Polyline{{}, StyleRole::CURVE, {}});
    RecordingRenderer renderer;

    render_display_list(renderer, display_list);

    EXPECT_EQ(0, renderer.point_count());
}

TEST(DisplayListRenderer, preservesSourceColor)
{
    const RgbColor source_color(5, 6, 7);
    DisplayList display_list;
    display_list.add(Swatch{1, 2, 3, 4, source_color, StyleRole::PALETTE, {}});
    RecordingRenderer renderer;

    render_display_list(renderer, display_list);

    ASSERT_TRUE(renderer.swatch_color());
    EXPECT_EQ(source_color, *renderer.swatch_color());
}
