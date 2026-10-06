// Copyright (c) 2026 Richard Thomson

#include <qtTimeline/Renderer.h>

#include <timeline/DisplayListRenderer.h>
#include <timeline/size_cast.h>

#include <QPolygon>

#include <algorithm>
#include <string_view>

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

namespace
{

/// Adapts toolkit-neutral display-list operations to QPainter.
///
class QtDisplayListRenderer final : public timeline::DisplayListRenderer
{
public:
    QtDisplayListRenderer(
        QPainter &painter, const QPalette &palette, const StyleColors &style_colors, bool focused, int label_width);

    void draw_line(const timeline::Line &line) override;
    void fill_rectangle(const timeline::Rectangle &rectangle) override;
    void draw_text(const timeline::Text &text, std::string_view value) override;
    void draw_marker(const timeline::Marker &marker) override;
    void draw_polyline(const timeline::Polyline &polyline) override;
    void draw_swatch(const timeline::Swatch &swatch) override;

private:
    QColor color(timeline::StyleRole role) const
    {
        return style_color(role, m_palette, m_style_colors, m_focused);
    }

    QPainter &m_painter;
    const QPalette &m_palette;
    const StyleColors &m_style_colors;
    bool m_focused;
    int m_label_width;
};

} // namespace

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

namespace
{

QtDisplayListRenderer::QtDisplayListRenderer(
    QPainter &painter, const QPalette &palette, const StyleColors &style_colors, bool focused, int label_width) :
    m_painter(painter),
    m_palette(palette),
    m_style_colors(style_colors),
    m_focused(focused),
    m_label_width(label_width)
{
}

void QtDisplayListRenderer::draw_line(const timeline::Line &line)
{
    m_painter.setPen(QPen(color(line.style), 1));
    m_painter.drawLine(line.x1, line.y1, line.x2, line.y2);
}

void QtDisplayListRenderer::fill_rectangle(const timeline::Rectangle &rectangle)
{
    m_painter.fillRect(rectangle.x, rectangle.y, rectangle.width, rectangle.height, color(rectangle.style));
}

void QtDisplayListRenderer::draw_text(const timeline::Text &text, std::string_view value)
{
    m_painter.save();
    if (text.style == timeline::StyleRole::LANE_LABEL)
    {
        m_painter.setClipRect(text.x, text.y, std::max(0, m_label_width - text.x - 4), m_painter.fontMetrics().height(),
            Qt::IntersectClip);
    }
    m_painter.setPen(color(text.style));
    m_painter.drawText(
        text.x, text.y + m_painter.fontMetrics().ascent(), QString::fromUtf8(value.data(), timeline::size_cast(value)));
    m_painter.restore();
}

void QtDisplayListRenderer::draw_marker(const timeline::Marker &marker)
{
    m_painter.fillRect(marker.x, marker.y, marker.width, marker.height, color(marker.style));
}

void QtDisplayListRenderer::draw_polyline(const timeline::Polyline &polyline)
{
    QPolygon points;
    points.reserve(timeline::size_cast(polyline.points));
    for (const timeline::Point &point : polyline.points)
    {
        points.append(QPoint(point.x, point.y));
    }
    m_painter.setPen(QPen(color(polyline.style), 2));
    m_painter.drawPolyline(points);
}

void QtDisplayListRenderer::draw_swatch(const timeline::Swatch &swatch)
{
    QColor fill = color(swatch.style);
    if (m_style_colors.find(swatch.style) == m_style_colors.end())
    {
        fill = QColor(swatch.color.red(), swatch.color.green(), swatch.color.blue());
    }
    m_painter.fillRect(swatch.x, swatch.y, swatch.width, swatch.height, fill);
}

} // namespace

void draw_display_list(
    QPainter &painter, const timeline::DisplayList &list, const QPalette &palette, bool focused, int label_width)
{
    draw_display_list(painter, list, palette, StyleColors{}, focused, label_width);
}

void draw_display_list(QPainter &painter, const timeline::DisplayList &list, const QPalette &palette,
    const StyleColors &style_colors, bool focused, int label_width)
{
    painter.save();
    QtDisplayListRenderer renderer(painter, palette, style_colors, focused, label_width);
    timeline::render_display_list(renderer, list);
    painter.restore();
}

} // namespace timeline_qt
