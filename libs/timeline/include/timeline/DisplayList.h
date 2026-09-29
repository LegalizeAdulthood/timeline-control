// Copyright (c) 2026 Richard Thomson

#pragma once

#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace timeline
{

/// Semantic rendering role interpreted by a GUI toolkit adapter.
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
    ENVELOPE_DECAY
};

/// Toolkit-neutral line drawing primitive.
struct Line
{
    int x1;
    int y1;
    int x2;
    int y2;
    StyleRole style;
};

/// Toolkit-neutral filled rectangle drawing primitive.
struct Rectangle
{
    int x;
    int y;
    int width;
    int height;
    StyleRole style;
};

/// Toolkit-neutral text drawing primitive.
struct Text
{
    int x;
    int y;
    std::string value;
    StyleRole style;
};

/// One toolkit-neutral drawing operation.
using Primitive = std::variant<Line, Rectangle, Text>;

/// Ordered rendering operations produced by timeline layout.
///
/// A display list records semantic primitives without native colors, fonts,
/// drawing contexts, or other GUI toolkit objects.
///
class DisplayList
{
public:
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
    const std::vector<Primitive> &primitives() const
    {
        return m_primitives;
    }

private:
    std::vector<Primitive> m_primitives;
};

} // namespace timeline
