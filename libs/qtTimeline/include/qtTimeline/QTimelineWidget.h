// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/Interaction.h>
#include <timeline/Layout.h>
#include <timeline/Query.h>

#include <QAbstractScrollArea>

/// Document-owning Qt view of core layout, selection, and frame inspection.
/// Native rendering and input translation contain no source-track semantics.
///
class QTimelineWidget : public QAbstractScrollArea
{
    Q_OBJECT

public:
    QTimelineWidget();
    explicit QTimelineWidget(QWidget &parent);
    void set_document(timeline::Document document);
    void zoom_in()
    {
        zoom_by(1.25);
    }
    void zoom_out()
    {
        zoom_by(1.0 / 1.25);
    }
    void fit_view();
    void clear_selection();
    std::string snapshot() const;
    const std::optional<timeline::Document> &document() const
    {
        return m_document;
    }
    const std::optional<timeline::Interaction> &interaction() const
    {
        return m_interaction;
    }
    const std::optional<timeline::FrameInspection> &inspection() const
    {
        return m_inspection;
    }
    const std::optional<timeline::HitResult> &hit_result() const
    {
        return m_hit;
    }
    const std::optional<timeline::Layout> &layout() const
    {
        return m_layout;
    }
    const std::optional<timeline::Viewport> &timeline_viewport() const
    {
        return m_viewport;
    }
    const std::optional<timeline::LayoutMetrics> &layout_metrics() const
    {
        return m_metrics;
    }

signals:
    void inspection_changed();

private:
    explicit QTimelineWidget(QWidget *parent);
    void rebuild();
    void update_inspection();
    void zoom_by(double factor);
    void end_drag();
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool viewportEvent(QEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    std::optional<timeline::Navigation> m_navigation;
    std::optional<timeline::Point> m_hover;
    bool m_dragging{false};
    bool m_rebuilding{false};
    int m_wheel_remainder{0};
    std::optional<timeline::Document> m_document;
    std::optional<timeline::Interaction> m_interaction;
    std::optional<timeline::FrameInspection> m_inspection;
    std::optional<timeline::HitResult> m_hit;
    std::optional<timeline::Layout> m_layout;
    std::optional<timeline::Viewport> m_viewport;
    std::optional<timeline::LayoutMetrics> m_metrics;
};
