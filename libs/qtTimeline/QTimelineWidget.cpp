// Copyright (c) 2026 Richard Thomson

#include <qtTimeline/QTimelineWidget.h>
#include <qtTimeline/Renderer.h>

#include <timeline/Snapshot.h>

#include <QFocusEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace
{

constexpr int SCROLL_RANGE = 10000;
constexpr double ZOOM_STEP = 1.25;

timeline::Point position(const QMouseEvent &event)
{
    return timeline::Point{qRound(event.position().x()), qRound(event.position().y())};
}

} // namespace

QTimelineWidget::QTimelineWidget() :
    QTimelineWidget(nullptr)
{
}
QTimelineWidget::QTimelineWidget(QWidget &parent) :
    QTimelineWidget(&parent)
{
}
QTimelineWidget::QTimelineWidget(QWidget *parent) :
    QAbstractScrollArea(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    viewport()->setMouseTracking(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setMinimumSize(200, 100);
}
void QTimelineWidget::set_document(timeline::Document document)
{
    end_drag();
    m_document = std::move(document);
    m_interaction.emplace(*m_document);
    m_inspection.reset();
    m_hit.reset();
    m_hover.reset();
    m_navigation.reset();
    m_wheel_remainder = 0;
    if (m_document->frame_grid() && m_document->frame_grid()->frame_count() > 0)
    {
        m_interaction->move_playhead_frame(0);
        m_inspection = timeline::inspect_frame(*m_document, 0);
    }
    const std::optional<timeline::Time> start = m_document->content_start();
    const std::optional<timeline::Time> end = m_document->content_end();
    if (start && end && *start < *end)
    {
        m_navigation.emplace(*start, *end, m_document->lane_count());
    }
    rebuild();
    emit inspection_changed();
}
void QTimelineWidget::fit_view()
{
    if (m_navigation)
    {
        m_navigation->fit();
        rebuild();
    }
}
void QTimelineWidget::clear_selection()
{
    end_drag();
    if (m_interaction)
    {
        m_interaction->clear_selection();
        update_inspection();
    }
}
std::string QTimelineWidget::snapshot() const
{
    if (m_layout)
    {
        return timeline::render_snapshot(m_layout->display_list());
    }
    return timeline::render_snapshot(timeline::DisplayList{});
}

void QTimelineWidget::zoom_by(double factor)
{
    if (m_navigation && m_viewport)
    {
        const timeline::Ticks middle =
            m_viewport->start().ticks() + (m_viewport->end().ticks() - m_viewport->start().ticks()) / 2;
        m_navigation->zoom_by(factor, timeline::Time::from_ticks(middle));
        rebuild();
    }
}

void QTimelineWidget::rebuild()
{
    if (m_rebuilding)
    {
        return;
    }
    const QScopedValueRollback<bool> guard(m_rebuilding, true);
    const QSignalBlocker horizontal(horizontalScrollBar());
    const QSignalBlocker vertical(verticalScrollBar());
    m_layout.reset();
    m_metrics.reset();
    m_viewport.reset();
    if (!m_navigation || viewport()->width() < 160 || viewport()->height() < 40)
    {
        horizontalScrollBar()->setRange(0, 0);
        verticalScrollBar()->setRange(0, 0);
        m_hit.reset();
        viewport()->update();
        return;
    }
    const QFontMetrics font_metrics(font());
    int label_width = 80;
    for (const timeline::Lane &lane : m_document->lanes())
    {
        const std::string_view label = m_document->strings().lookup(lane.label());
        label_width = std::max(label_width,
            font_metrics.horizontalAdvance(QString::fromUtf8(label.data(), timeline::size_cast(label))) + 16);
    }
    label_width = std::min(label_width, viewport()->width() / 2);
    m_metrics.emplace(label_width, font_metrics.height() + 8, font_metrics.height() + 16, 4);
    m_viewport = m_navigation->viewport(viewport()->width(), viewport()->height());
    const int lanes = std::max(1, timeline::visible_lane_count(*m_viewport, *m_metrics));
    m_navigation->scroll_to_lane(m_viewport->first_lane(), lanes);
    m_viewport = m_navigation->viewport(viewport()->width(), viewport()->height());
    m_layout.emplace(*m_document, *m_viewport, *m_metrics, *m_interaction);
    const std::optional<timeline::HitResult> hit = m_hover ? m_layout->hit_test(*m_hover, 3) : std::nullopt;
    if (hit != m_hit)
    {
        m_hit = hit;
        emit inspection_changed();
    }
    const int thumb =
        std::clamp(static_cast<int>(std::lround(SCROLL_RANGE / m_navigation->zoom_scale())), 1, SCROLL_RANGE);
    horizontalScrollBar()->setRange(0, SCROLL_RANGE - thumb);
    horizontalScrollBar()->setPageStep(thumb);
    horizontalScrollBar()->setValue(
        static_cast<int>(std::lround(m_navigation->horizontal_fraction() * (SCROLL_RANGE - thumb))));
    verticalScrollBar()->setRange(0, std::max(0, m_document->lane_count() - lanes));
    verticalScrollBar()->setPageStep(lanes);
    verticalScrollBar()->setValue(m_viewport->first_lane());
    viewport()->update();
}

void QTimelineWidget::update_inspection()
{
    if (m_document && m_interaction && m_interaction->playhead_frame())
    {
        m_inspection = timeline::inspect_frame(*m_document, *m_interaction->playhead_frame());
    }
    rebuild();
    emit inspection_changed();
}

void QTimelineWidget::end_drag()
{
    m_dragging = false;
    if (m_interaction)
    {
        m_interaction->end_range();
    }
}

void QTimelineWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(viewport());
    painter.fillRect(viewport()->rect(), palette().brush(QPalette::Base));
    painter.setClipRect(viewport()->rect());
    painter.setFont(font());
    painter.setRenderHint(QPainter::Antialiasing);
    if (m_layout)
    {
        timeline_qt::draw_display_list(
            painter, m_layout->display_list(), palette(), hasFocus(), m_metrics->lane_label_width());
    }
    else
    {
        painter.setPen(palette().color(QPalette::Text));
        painter.drawText(12, painter.fontMetrics().ascent() + 12,
            m_document ? QStringLiteral("No timeline content.") : QStringLiteral("No timeline loaded."));
    }
}

void QTimelineWidget::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);
    rebuild();
}

void QTimelineWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_layout)
    {
        QAbstractScrollArea::mousePressEvent(event);
        return;
    }
    setFocus(Qt::MouseFocusReason);
    m_hover = position(*event);
    m_hit = m_layout->hit_test(*m_hover, 3);
    m_interaction->select_hit(m_hit, event->modifiers().testFlag(Qt::ControlModifier));
    if (m_hover->x >= m_metrics->lane_label_width())
    {
        m_interaction->begin_range(timeline::time_at_x(m_hover->x, *m_viewport, *m_metrics));
        m_dragging = true;
    }
    update_inspection();
    event->accept();
}

void QTimelineWidget::mouseMoveEvent(QMouseEvent *event)
{
    m_hover = position(*event);
    if (!m_layout)
    {
        return;
    }
    if (m_dragging)
    {
        m_interaction->extend_range(timeline::time_at_x(m_hover->x, *m_viewport, *m_metrics));
        update_inspection();
        return;
    }
    const std::optional<timeline::HitResult> hit = m_layout->hit_test(*m_hover, 3);
    bool changed = hit != m_hit;
    m_hit = hit;
    if (m_document->frame_grid() && m_hover->x >= m_metrics->lane_label_width() &&
        viewport()->rect().contains(event->position().toPoint()))
    {
        const std::optional<timeline::Ticks> frame =
            m_document->frame_grid()->nearest_frame(timeline::time_at_x(m_hover->x, *m_viewport, *m_metrics));
        if (frame && (!m_inspection || *frame != m_inspection->frame))
        {
            m_inspection = timeline::inspect_frame(*m_document, *frame);
            changed = true;
        }
    }
    if (changed)
    {
        emit inspection_changed();
    }
}

void QTimelineWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragging)
    {
        if (m_viewport && m_metrics)
        {
            m_interaction->extend_range(timeline::time_at_x(position(*event).x, *m_viewport, *m_metrics));
        }
        end_drag();
        update_inspection();
        event->accept();
        return;
    }
    QAbstractScrollArea::mouseReleaseEvent(event);
}

void QTimelineWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
    {
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }
    if (m_interaction && (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right))
    {
        m_interaction->step_playhead(
            event->key() == Qt::Key_Left ? -1 : 1, event->modifiers().testFlag(Qt::ShiftModifier));
        if (m_navigation && m_interaction->playhead())
        {
            m_navigation->reveal(*m_interaction->playhead());
        }
        update_inspection();
        event->accept();
    }
    else if (event->key() == Qt::Key_Escape)
    {
        clear_selection();
        event->accept();
    }
    else
    {
        QAbstractScrollArea::keyPressEvent(event);
    }
}

void QTimelineWidget::wheelEvent(QWheelEvent *event)
{
    if (!m_navigation || !m_viewport || !m_metrics)
    {
        event->ignore();
        return;
    }
    const bool horizontal = (event->angleDelta().y() == 0 && event->angleDelta().x() != 0) ||
        (event->angleDelta().isNull() && event->pixelDelta().y() == 0 && event->pixelDelta().x() != 0);
    m_wheel_remainder += horizontal ? event->angleDelta().x() : event->angleDelta().y();
    int steps = m_wheel_remainder / 120;
    m_wheel_remainder %= 120;
    if (steps == 0 && !event->pixelDelta().isNull())
    {
        const int delta = event->pixelDelta().y() != 0 ? event->pixelDelta().y() : event->pixelDelta().x();
        steps = delta > 0 ? 1 : -1;
    }
    if (steps != 0)
    {
        if (event->modifiers().testFlag(Qt::ControlModifier))
        {
            m_navigation->zoom_by(std::pow(ZOOM_STEP, steps),
                timeline::time_at_x(qRound(event->position().x()), *m_viewport, *m_metrics));
        }
        else if (horizontal || event->modifiers().testFlag(Qt::ShiftModifier))
        {
            const timeline::Ticks delta =
                std::max<timeline::Ticks>(1, (m_viewport->end().ticks() - m_viewport->start().ticks()) / 10);
            m_navigation->scroll_to(timeline::Time::from_ticks(m_viewport->start().ticks() - steps * delta));
        }
        else
        {
            m_navigation->scroll_to_lane(
                m_viewport->first_lane() - steps, std::max(1, timeline::visible_lane_count(*m_viewport, *m_metrics)));
        }
        rebuild();
    }
    event->accept();
}

void QTimelineWidget::focusInEvent(QFocusEvent *event)
{
    QAbstractScrollArea::focusInEvent(event);
    viewport()->update();
}

void QTimelineWidget::focusOutEvent(QFocusEvent *event)
{
    end_drag();
    QAbstractScrollArea::focusOutEvent(event);
    viewport()->update();
}

void QTimelineWidget::changeEvent(QEvent *event)
{
    QAbstractScrollArea::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::PaletteChange ||
        event->type() == QEvent::StyleChange)
    {
        rebuild();
        emit inspection_changed();
    }
}

bool QTimelineWidget::viewportEvent(QEvent *event)
{
    if (event->type() == QEvent::Leave)
    {
        m_hover.reset();
        if (m_hit)
        {
            m_hit.reset();
            emit inspection_changed();
        }
    }
    else if (event->type() == QEvent::UngrabMouse)
    {
        end_drag();
    }
    return QAbstractScrollArea::viewportEvent(event);
}

void QTimelineWidget::scrollContentsBy(int, int)
{
    if (!m_rebuilding && m_navigation && m_viewport && m_metrics)
    {
        const int maximum = horizontalScrollBar()->maximum();
        m_navigation->scroll_to_fraction(
            maximum > 0 ? static_cast<double>(horizontalScrollBar()->value()) / maximum : 0.0);
        m_navigation->scroll_to_lane(
            verticalScrollBar()->value(), std::max(1, timeline::visible_lane_count(*m_viewport, *m_metrics)));
        rebuild();
    }
}
