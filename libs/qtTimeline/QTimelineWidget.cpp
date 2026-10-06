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
    m_state.set_document(std::move(document));
    m_hover.reset();
    m_wheel_remainder = 0;
    rebuild();
    emit inspection_changed();
}
void QTimelineWidget::fit_view()
{
    if (m_state.navigation())
    {
        m_state.fit_view();
        rebuild();
    }
}
void QTimelineWidget::clear_selection()
{
    end_drag();
    if (m_state.interaction())
    {
        m_state.clear_selection();
        update_control();
    }
}
std::string QTimelineWidget::snapshot() const
{
    if (m_state.layout())
    {
        return timeline::render_snapshot(m_state.layout()->display_list());
    }
    return timeline::render_snapshot(timeline::DisplayList{});
}

void QTimelineWidget::zoom_by(double factor)
{
    if (m_state.navigation() && m_state.viewport())
    {
        m_state.zoom_by(factor);
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
    const std::optional<timeline::HitResult> old_hit = m_state.hit_result();
    m_state.clear_layout();
    if (!m_state.navigation() || viewport()->width() < 160 || viewport()->height() < 40)
    {
        horizontalScrollBar()->setRange(0, 0);
        verticalScrollBar()->setRange(0, 0);
        viewport()->update();
        return;
    }
    const QFontMetrics font_metrics(font());
    int label_width = 80;
    for (const timeline::Lane &lane : m_state.document()->lanes())
    {
        const std::string_view label = m_state.document()->strings().lookup(lane.label());
        label_width = std::max(label_width,
            font_metrics.horizontalAdvance(QString::fromUtf8(label.data(), timeline::size_cast(label))) + 16);
    }
    label_width = std::min(label_width, viewport()->width() / 2);
    const timeline::LayoutMetrics metrics(label_width, font_metrics.height() + 8, font_metrics.height() + 16, 4);
    if (!m_state.rebuild_layout(viewport()->width(), viewport()->height(), metrics))
    {
        horizontalScrollBar()->setRange(0, 0);
        verticalScrollBar()->setRange(0, 0);
        viewport()->update();
        return;
    }
    if (m_hover)
    {
        m_state.hover_at(*m_hover, 3, false);
    }
    if (m_state.hit_result() != old_hit)
    {
        emit inspection_changed();
    }
    const timeline::Navigation &navigation = *m_state.navigation();
    const timeline::Viewport &timeline_viewport = *m_state.viewport();
    const int lanes = std::max(1, timeline::visible_lane_count(timeline_viewport, metrics));
    const int thumb =
        std::clamp(static_cast<int>(std::lround(SCROLL_RANGE / navigation.zoom_scale())), 1, SCROLL_RANGE);
    horizontalScrollBar()->setRange(0, SCROLL_RANGE - thumb);
    horizontalScrollBar()->setPageStep(thumb);
    horizontalScrollBar()->setValue(
        static_cast<int>(std::lround(navigation.horizontal_fraction() * (SCROLL_RANGE - thumb))));
    verticalScrollBar()->setRange(0, std::max(0, m_state.document()->lane_count() - lanes));
    verticalScrollBar()->setPageStep(lanes);
    verticalScrollBar()->setValue(timeline_viewport.first_lane());
    viewport()->update();
}

void QTimelineWidget::update_control()
{
    rebuild();
    emit inspection_changed();
}

void QTimelineWidget::end_drag()
{
    m_dragging = false;
    m_state.end_range();
}

void QTimelineWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(viewport());
    painter.fillRect(viewport()->rect(), palette().brush(QPalette::Base));
    painter.setClipRect(viewport()->rect());
    painter.setFont(font());
    painter.setRenderHint(QPainter::Antialiasing);
    if (m_state.layout())
    {
        timeline_qt::draw_display_list(painter, m_state.layout()->display_list(), palette(), hasFocus(),
            m_state.layout_metrics()->lane_label_width());
    }
    else
    {
        painter.setPen(palette().color(QPalette::Text));
        painter.drawText(12, painter.fontMetrics().ascent() + 12,
            m_state.document() ? QStringLiteral("No timeline content.") : QStringLiteral("No timeline loaded."));
    }
}

void QTimelineWidget::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);
    rebuild();
}

void QTimelineWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_state.layout())
    {
        QAbstractScrollArea::mousePressEvent(event);
        return;
    }
    setFocus(Qt::MouseFocusReason);
    m_hover = position(*event);
    m_dragging = m_state.begin_selection(*m_hover, 3, event->modifiers().testFlag(Qt::ControlModifier));
    update_control();
    event->accept();
}

void QTimelineWidget::mouseMoveEvent(QMouseEvent *event)
{
    m_hover = position(*event);
    if (!m_state.layout())
    {
        return;
    }
    if (m_dragging)
    {
        m_state.extend_range(*m_hover);
        update_control();
        return;
    }
    const std::optional<timeline::HitResult> old_hit = m_state.hit_result();
    const std::optional<timeline::Ticks> old_frame =
        m_state.inspection() ? std::optional<timeline::Ticks>(m_state.inspection()->frame) : std::nullopt;
    m_state.hover_at(*m_hover, 3, viewport()->rect().contains(event->position().toPoint()));
    const std::optional<timeline::Ticks> frame =
        m_state.inspection() ? std::optional<timeline::Ticks>(m_state.inspection()->frame) : std::nullopt;
    const bool changed = m_state.hit_result() != old_hit || frame != old_frame;
    if (changed)
    {
        emit inspection_changed();
    }
}

void QTimelineWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragging)
    {
        m_state.extend_range(position(*event));
        end_drag();
        update_control();
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
    if (m_state.interaction() && (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right))
    {
        m_state.step_playhead(event->key() == Qt::Key_Left ? -1 : 1, event->modifiers().testFlag(Qt::ShiftModifier));
        update_control();
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
    if (!m_state.navigation() || !m_state.viewport() || !m_state.layout_metrics())
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
            m_state.zoom_at(std::pow(ZOOM_STEP, steps), qRound(event->position().x()));
        }
        else if (horizontal || event->modifiers().testFlag(Qt::ShiftModifier))
        {
            const timeline::Ticks delta = std::max<timeline::Ticks>(
                1, (m_state.viewport()->end().ticks() - m_state.viewport()->start().ticks()) / 10);
            m_state.scroll_by(timeline::Duration::from_ticks(-steps * delta));
        }
        else
        {
            m_state.scroll_lanes(-steps);
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
        if (m_state.hit_result())
        {
            m_state.clear_hover();
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
    if (!m_rebuilding && m_state.navigation() && m_state.viewport() && m_state.layout_metrics())
    {
        const int maximum = horizontalScrollBar()->maximum();
        m_state.scroll_to_fraction(maximum > 0 ? static_cast<double>(horizontalScrollBar()->value()) / maximum : 0.0);
        m_state.scroll_lanes(verticalScrollBar()->value() - m_state.viewport()->first_lane());
        rebuild();
    }
}
