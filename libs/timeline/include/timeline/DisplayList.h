// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/Palette.h>
#include <timeline/StringTable.h>

#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace timeline
{

/// Semantic rendering role interpreted by a GUI toolkit adapter.
///
enum class StyleRole
{
    RULER,
    RULER_LABEL,
    LANE_BACKGROUND,
    LANE_LABEL,
    INSTANT_MARKER,
    INTERVAL_SPAN,
    ENVELOPE_ATTACK,
    ENVELOPE_SUSTAIN,
    ENVELOPE_DECAY,
    CURVE,
    KEYFRAME_SEGMENT,
    KEYFRAME_MARKER,
    SELECTED_LANE,
    SELECTED_ITEM,
    SELECTED_RANGE,
    PLAYHEAD,
    PALETTE
};

/// Returns the stable name of a semantic rendering role.
std::string_view to_string(StyleRole value);

/// Stable source identity carried by a display-list primitive.
///
struct DisplayId
{
    StringId lane_id;
    std::string item_id;
};

/// Toolkit-neutral line drawing primitive.
///
struct Line
{
    int x1;
    int y1;
    int x2;
    int y2;
    StyleRole style;
    DisplayId id;
};

/// Toolkit-neutral filled rectangle drawing primitive.
///
struct Rectangle
{
    int x;
    int y;
    int width;
    int height;
    StyleRole style;
    DisplayId id;
};

/// Toolkit-neutral text drawing primitive.
///
struct Text
{
    int x;
    int y;
    std::string value;
    StyleRole style;
    DisplayId id;
};

/// Toolkit-neutral marker with rectangular hit-test bounds.
///
struct Marker
{
    int x;
    int y;
    int width;
    int height;
    StyleRole style;
    DisplayId id;
};

/// Toolkit-neutral integral point.
///
struct Point
{
    int x;
    int y;
};

/// Toolkit-neutral connected line segments.
///
struct Polyline
{
    std::vector<Point> points;
    StyleRole style;
    DisplayId id;
};

/// RGB swatch displaying source content rather than a theme color.
///
struct Swatch
{
    int x;
    int y;
    int width;
    int height;
    RgbColor color;
    StyleRole style;
    DisplayId id;
};

/// One toolkit-neutral drawing operation.
///
using Primitive = std::variant<Line, Rectangle, Text, Marker, Polyline, Swatch>;

/// Ordered rendering operations produced by timeline layout.
///
/// A display list records semantic primitives without native colors, fonts,
/// drawing contexts, or other GUI toolkit objects.
///
class DisplayList
{
public:
    DisplayList() = default;
    explicit DisplayList(StringTable strings) :
        m_strings(std::move(strings))
    {
    }

    void add(Line line)
    {
        m_primitives.emplace_back(line);
    }
    void add(Rectangle rectangle)
    {
        m_primitives.emplace_back(rectangle);
    }
    void add(Text text)
    {
        m_primitives.emplace_back(std::move(text));
    }
    void add(Marker marker)
    {
        m_primitives.emplace_back(std::move(marker));
    }
    void add(Polyline polyline)
    {
        m_primitives.emplace_back(std::move(polyline));
    }
    void add(Swatch swatch)
    {
        m_primitives.emplace_back(std::move(swatch));
    }
    const std::vector<Primitive> &primitives() const
    {
        return m_primitives;
    }
    const StringTable &strings() const
    {
        return m_strings;
    }

private:
    StringTable m_strings;
    std::vector<Primitive> m_primitives;
};

} // namespace timeline
