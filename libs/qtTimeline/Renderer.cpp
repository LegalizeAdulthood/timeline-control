// Copyright (c) 2026 Richard Thomson

#include <qtTimeline/Renderer.h>

#include <timeline/size_cast.h>

#include <QPolygon>

#include <algorithm>
#include <string_view>
#include <type_traits>

namespace
{

QColor mix(const QColor &first, const QColor &second, int weight)
{
    return QColor((first.red() * weight + second.red() * (100 - weight)) / 100,
        (first.green() * weight + second.green() * (100 - weight)) / 100,
        (first.blue() * weight + second.blue() * (100 - weight)) / 100);
}

} // namespace

namespace timeline_qt
{

QColor style_color(timeline::StyleRole role, const QPalette &palette, bool focused)
{
    return style_color(role, palette, StyleColors{}, focused);
}

QColor style_color(timeline::StyleRole role, const QPalette &palette, const StyleColors &style_colors, bool focused)
{
    const StyleColors::const_iterator found = style_colors.find(role);
    if (found != style_colors.end())
    {
        return found->second;
    }
    const QColor background = palette.color(QPalette::Base);
    const QColor foreground = palette.color(QPalette::Text);
    const QColor highlight = focused ? palette.color(QPalette::Highlight) : foreground;
    switch (role)
    {
    case timeline::StyleRole::RULER:
        return palette.color(QPalette::Mid);
    case timeline::StyleRole::RULER_LABEL:
    case timeline::StyleRole::LANE_LABEL:
    case timeline::StyleRole::PALETTE:
        return foreground;
    case timeline::StyleRole::LANE_BACKGROUND:
        return palette.color(QPalette::AlternateBase);
    case timeline::StyleRole::INSTANT_MARKER:
        return palette.color(QPalette::Accent);
    case timeline::StyleRole::INTERVAL_SPAN:
        return palette.color(QPalette::Link);
    case timeline::StyleRole::ENVELOPE_ATTACK:
        return palette.color(QPalette::Accent);
    case timeline::StyleRole::ENVELOPE_SUSTAIN:
        return palette.color(QPalette::Link);
    case timeline::StyleRole::ENVELOPE_DECAY:
        return palette.color(QPalette::LinkVisited);
    case timeline::StyleRole::CURVE:
        return palette.color(QPalette::Link);
    case timeline::StyleRole::KEYFRAME_SEGMENT:
        return palette.color(QPalette::Link);
    case timeline::StyleRole::KEYFRAME_MARKER:
        return palette.color(QPalette::Accent);
    case timeline::StyleRole::SELECTED_LANE:
        return mix(highlight, background, 25);
    case timeline::StyleRole::SELECTED_ITEM:
        return focused ? highlight : mix(foreground, background, 65);
    case timeline::StyleRole::SELECTED_RANGE:
        return mix(highlight, background, 18);
    case timeline::StyleRole::PLAYHEAD:
        return palette.color(QPalette::Accent);
    }
    return foreground;
}

void draw_display_list(
    QPainter &painter, const timeline::DisplayList &list, const QPalette &palette, bool focused, int label_width)
{
    draw_display_list(painter, list, palette, StyleColors{}, focused, label_width);
}

void draw_display_list(QPainter &painter, const timeline::DisplayList &list, const QPalette &palette,
    const StyleColors &style_colors, bool focused, int label_width)
{
    painter.save();
    for (const timeline::Primitive &primitive : list.primitives())
    {
        std::visit(
            [&](const auto &value)
            {
                using Value = std::decay_t<decltype(value)>;
                const QColor color = timeline_qt::style_color(value.style, palette, style_colors, focused);
                const bool overridden = style_colors.find(value.style) != style_colors.end();
                if constexpr (std::is_same_v<Value, timeline::Line>)
                {
                    painter.setPen(QPen(color, 1));
                    painter.drawLine(value.x1, value.y1, value.x2, value.y2);
                }
                else if constexpr (std::is_same_v<Value, timeline::Rectangle> ||
                    std::is_same_v<Value, timeline::Marker>)
                {
                    painter.fillRect(value.x, value.y, value.width, value.height, color);
                }
                else if constexpr (std::is_same_v<Value, timeline::Swatch>)
                {
                    const QColor fill =
                        overridden ? color : QColor(value.color.red(), value.color.green(), value.color.blue());
                    painter.fillRect(value.x, value.y, value.width, value.height, fill);
                }
                else if constexpr (std::is_same_v<Value, timeline::Polyline>)
                {
                    QPolygon points;
                    points.reserve(timeline::size_cast(value.points));
                    for (const timeline::Point &point : value.points)
                    {
                        points.append(QPoint(point.x, point.y));
                    }
                    painter.setPen(QPen(color, 2));
                    painter.drawPolyline(points);
                }
                else
                {
                    const std::string_view text = list.strings().lookup(value.value);
                    painter.save();
                    if (value.style == timeline::StyleRole::LANE_LABEL)
                    {
                        painter.setClipRect(value.x, value.y, std::max(0, label_width - value.x - 4),
                            painter.fontMetrics().height(), Qt::IntersectClip);
                    }
                    painter.setPen(color);
                    painter.drawText(value.x, value.y + painter.fontMetrics().ascent(),
                        QString::fromUtf8(text.data(), timeline::size_cast(text)));
                    painter.restore();
                }
            },
            primitive);
    }
    painter.restore();
}

} // namespace timeline_qt
