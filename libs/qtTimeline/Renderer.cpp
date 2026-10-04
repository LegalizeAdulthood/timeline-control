// Copyright (c) 2026 Richard Thomson

#include <qtTimeline/Renderer.h>

#include <timeline/size_cast.h>

#include <QPolygon>

#include <algorithm>
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
    const QColor background = palette.color(QPalette::Base);
    const QColor foreground = palette.color(QPalette::Text);
    const QColor highlight = focused ? palette.color(QPalette::Highlight) : foreground;
    const bool dark = background.lightnessF() < 0.5F;
    switch (role)
    {
    case timeline::StyleRole::RULER:
        return mix(foreground, background, 60);
    case timeline::StyleRole::RULER_LABEL:
    case timeline::StyleRole::LANE_LABEL:
    case timeline::StyleRole::PALETTE:
        return foreground;
    case timeline::StyleRole::LANE_BACKGROUND:
        return mix(foreground, background, 6);
    case timeline::StyleRole::INSTANT_MARKER:
        return dark ? QColor(240, 110, 100) : QColor(196, 58, 48);
    case timeline::StyleRole::INTERVAL_SPAN:
        return dark ? QColor(80, 190, 180) : QColor(32, 126, 120);
    case timeline::StyleRole::ENVELOPE_ATTACK:
        return dark ? QColor(235, 190, 80) : QColor(170, 110, 25);
    case timeline::StyleRole::ENVELOPE_SUSTAIN:
        return dark ? QColor(115, 195, 130) : QColor(67, 132, 78);
    case timeline::StyleRole::ENVELOPE_DECAY:
        return dark ? QColor(115, 160, 230) : QColor(66, 100, 166);
    case timeline::StyleRole::CURVE:
        return dark ? QColor(195, 135, 220) : QColor(126, 72, 154);
    case timeline::StyleRole::KEYFRAME_SEGMENT:
        return dark ? QColor(100, 160, 230) : QColor(47, 95, 164);
    case timeline::StyleRole::KEYFRAME_MARKER:
        return dark ? QColor(245, 185, 90) : QColor(190, 105, 20);
    case timeline::StyleRole::SELECTED_LANE:
        return mix(highlight, background, 25);
    case timeline::StyleRole::SELECTED_ITEM:
        return focused ? highlight : mix(foreground, background, 65);
    case timeline::StyleRole::SELECTED_RANGE:
        return mix(highlight, background, 18);
    case timeline::StyleRole::PLAYHEAD:
        return dark ? QColor(245, 100, 120) : QColor(185, 35, 55);
    }
    return foreground;
}
void draw_display_list(
    QPainter &painter, const timeline::DisplayList &list, const QPalette &palette, bool focused, int label_width)
{
    painter.save();
    for (const timeline::Primitive &primitive : list.primitives())
    {
        std::visit(
            [&](const auto &value)
            {
                using Value = std::decay_t<decltype(value)>;
                const QColor color = style_color(value.style, palette, focused);
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
                    painter.fillRect(value.x, value.y, value.width, value.height,
                        QColor(value.color.red(), value.color.green(), value.color.blue()));
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
                    painter.save();
                    if (value.style == timeline::StyleRole::LANE_LABEL)
                    {
                        painter.setClipRect(value.x, value.y, std::max(0, label_width - value.x - 4),
                            painter.fontMetrics().height(), Qt::IntersectClip);
                    }
                    painter.setPen(color);
                    painter.drawText(
                        value.x, value.y + painter.fontMetrics().ascent(), QString::fromStdString(value.value));
                    painter.restore();
                }
            },
            primitive);
    }
    painter.restore();
}

} // namespace timeline_qt
