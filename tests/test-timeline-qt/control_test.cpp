// Copyright (c) 2026 Richard Thomson

#include <gtest/gtest.h>
#include <QApplication>
#include <QFocusEvent>
#include <QImage>
#include <QMouseEvent>
#include <QScrollBar>
#include <QSignalSpy>
#include <QtTest/QTest>
#include <qtTimeline/QTimelineWidget.h>
#include <QWheelEvent>
#include <timeline/Snapshot.h>
#include <timelineParAnimator/TimelineJson.h>

namespace
{

const std::filesystem::path fixtures(TIMELINE_TEST_FIXTURE_DIR);

/// Real headless Qt widget exercising shared source fixtures and native input.
///
class QtTimeline : public testing::Test
{
protected:
    void SetUp() override;
    QTimelineWidget m_widget;
};

void QtTimeline::SetUp()
{
    timeline_par_animator::JsonImportResult imported =
        timeline_par_animator::import_timeline_json(fixtures / "extreme-normalized-vectors.json");
    ASSERT_TRUE(imported.succeeded());
    m_widget.resize(1000, 640);
    m_widget.show();
    m_widget.set_document(std::move(*imported.document));
    QApplication::processEvents();
}

TEST_F(QtTimeline, owns_document_and_uses_the_core_display_list)
{
    ASSERT_TRUE(m_widget.document());
    ASSERT_TRUE(m_widget.layout());
    ASSERT_TRUE(m_widget.timeline_viewport());
    ASSERT_TRUE(m_widget.layout_metrics());
    EXPECT_EQ(25, m_widget.document()->lane_count());
    const timeline::Layout expected(
        *m_widget.document(), *m_widget.timeline_viewport(), *m_widget.layout_metrics(), *m_widget.interaction());
    EXPECT_EQ(timeline::render_snapshot(expected.display_list()), m_widget.snapshot());
    const QImage image = m_widget.viewport()->grab().toImage();
    bool painted = false;
    for (int y = 0; y < image.height(); y += 7)
    {
        for (int x = 0; x < image.width(); x += 11)
        {
            painted = painted || image.pixelColor(x, y) != m_widget.palette().color(QPalette::Base);
        }
    }
    EXPECT_TRUE(painted);
}

TEST_F(QtTimeline, selects_source_items_and_steps_frames_with_native_keys)
{
    ASSERT_TRUE(m_widget.layout_metrics());
    ASSERT_TRUE(m_widget.timeline_viewport());
    const timeline::LayoutMetrics &metrics = *m_widget.layout_metrics();
    const int x = timeline::frame_x(1, *m_widget.document()->frame_grid(), *m_widget.timeline_viewport(), metrics);
    const QPoint point(x, metrics.ruler_height() + metrics.item_padding());
    QTest::mouseClick(m_widget.viewport(), Qt::LeftButton, Qt::NoModifier, point);
    ASSERT_TRUE(m_widget.interaction());
    EXPECT_EQ(1, *m_widget.interaction()->playhead_frame());
    EXPECT_EQ("animation-0[0]", *m_widget.interaction()->selected_lane());
    QTest::keyClick(&m_widget, Qt::Key_Right);
    EXPECT_EQ(2, *m_widget.interaction()->playhead_frame());
    QTest::keyClick(&m_widget, Qt::Key_Right, Qt::ShiftModifier);
    ASSERT_TRUE(m_widget.interaction()->selected_frames());
    EXPECT_EQ(2, m_widget.interaction()->selected_frames()->first());
    EXPECT_EQ(3, m_widget.inspection()->frame);
    QTest::keyClick(&m_widget, Qt::Key_Escape);
    EXPECT_TRUE(m_widget.interaction()->selected_items().empty());
}

TEST_F(QtTimeline, zooms_scrolls_and_resets_owned_state_on_replacement)
{
    ASSERT_TRUE(m_widget.timeline_viewport());
    const timeline::Ticks before =
        m_widget.timeline_viewport()->end().ticks() - m_widget.timeline_viewport()->start().ticks();
    m_widget.zoom_in();
    QApplication::processEvents();
    EXPECT_LT(m_widget.timeline_viewport()->end().ticks() - m_widget.timeline_viewport()->start().ticks(), before);
    m_widget.verticalScrollBar()->setValue(2);
    EXPECT_EQ(2, m_widget.timeline_viewport()->first_lane());
    m_widget.fit_view();
    QApplication::processEvents();
    EXPECT_EQ(before, m_widget.timeline_viewport()->end().ticks() - m_widget.timeline_viewport()->start().ticks());
    timeline_par_animator::JsonImportResult imported =
        timeline_par_animator::import_timeline_json(fixtures / "empty-animation.json");
    ASSERT_TRUE(imported.succeeded());
    m_widget.set_document(std::move(*imported.document));
    QApplication::processEvents();
    EXPECT_EQ(0, m_widget.document()->lane_count());
    EXPECT_TRUE(m_widget.layout());
    EXPECT_FALSE(m_widget.hit_result());
    EXPECT_TRUE(m_widget.interaction()->selected_items().empty());
    EXPECT_EQ(0, m_widget.verticalScrollBar()->maximum());
    m_widget.set_document(timeline::Document(100));
    EXPECT_FALSE(m_widget.layout());
    EXPECT_FALSE(m_widget.inspection());
}

TEST_F(QtTimeline, drags_frame_ranges_and_clears_hover_on_leave)
{
    ASSERT_TRUE(m_widget.layout_metrics());
    ASSERT_TRUE(m_widget.timeline_viewport());
    const timeline::LayoutMetrics &metrics = *m_widget.layout_metrics();
    const int first = timeline::frame_x(1, *m_widget.document()->frame_grid(), *m_widget.timeline_viewport(), metrics);
    const int last = timeline::frame_x(3, *m_widget.document()->frame_grid(), *m_widget.timeline_viewport(), metrics);
    const int y = metrics.ruler_height() + metrics.lane_height() / 2;
    QSignalSpy changed(&m_widget, &QTimelineWidget::inspection_changed);
    QTest::mousePress(m_widget.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(first, y));
    QTest::mouseMove(m_widget.viewport(), QPoint(last, y));
    QTest::mouseRelease(m_widget.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(last, y));
    ASSERT_TRUE(m_widget.interaction()->selected_frames());
    EXPECT_EQ(1, m_widget.interaction()->selected_frames()->first());
    EXPECT_EQ(3, m_widget.interaction()->selected_frames()->last());
    EXPECT_GT(changed.count(), 0);
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(m_widget.viewport(), &leave);
    EXPECT_FALSE(m_widget.hit_result());
}

TEST_F(QtTimeline, translates_wheel_zoom_and_lane_scroll_then_reflows_fonts)
{
    ASSERT_TRUE(m_widget.timeline_viewport());
    const timeline::Ticks before =
        m_widget.timeline_viewport()->end().ticks() - m_widget.timeline_viewport()->start().ticks();
    QWheelEvent zoom(QPointF(250, 80), QPointF(250, 80), QPoint(), QPoint(0, 120), Qt::NoButton, Qt::ControlModifier,
        Qt::NoScrollPhase, false);
    QApplication::sendEvent(m_widget.viewport(), &zoom);
    EXPECT_LT(m_widget.timeline_viewport()->end().ticks() - m_widget.timeline_viewport()->start().ticks(), before);
    QWheelEvent scroll(QPointF(250, 80), QPointF(250, 80), QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier,
        Qt::NoScrollPhase, false);
    QApplication::sendEvent(m_widget.viewport(), &scroll);
    EXPECT_EQ(1, m_widget.timeline_viewport()->first_lane());
    const int old_height = m_widget.layout_metrics()->lane_height();
    QFont font = m_widget.font();
    font.setPointSize(20);
    m_widget.setFont(font);
    QApplication::processEvents();
    EXPECT_GT(m_widget.layout_metrics()->lane_height(), old_height);
    m_widget.resize(240, 160);
    QApplication::processEvents();
    ASSERT_TRUE(m_widget.timeline_viewport());
    EXPECT_LT(m_widget.timeline_viewport()->width(), 240);
}

TEST_F(QtTimeline, preserves_source_item_identity_and_owns_a_copy_of_the_document)
{
    ASSERT_TRUE(m_widget.layout());
    const timeline::DisplayList list = m_widget.layout()->display_list();
    bool selected = false;
    for (const timeline::Primitive &primitive : list.primitives())
    {
        if (const timeline::Polyline *line = std::get_if<timeline::Polyline>(&primitive);
            line && line->id.lane_id == "animation-0[0]")
        {
            const timeline::Point point = line->points[line->points.size() / 4];
            QTest::mouseClick(m_widget.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(point.x, point.y));
            selected = true;
            break;
        }
    }
    ASSERT_TRUE(selected);
    ASSERT_EQ(1, timeline::size_cast(m_widget.interaction()->selected_items()));
    EXPECT_EQ("animation-0-key-0", m_widget.interaction()->selected_items().front().item_id);
    timeline::Document copy = *m_widget.document();
    m_widget.set_document(copy);
    copy = timeline::Document(100);
    EXPECT_EQ(25, m_widget.document()->lane_count());
    EXPECT_TRUE(m_widget.interaction()->selected_items().empty());
}

TEST_F(QtTimeline, cancels_drags_on_focus_or_capture_loss_and_finishes_outside_the_view)
{
    ASSERT_TRUE(m_widget.layout_metrics());
    const timeline::LayoutMetrics metrics = *m_widget.layout_metrics();
    const timeline::Viewport view = *m_widget.timeline_viewport();
    const int first = timeline::frame_x(1, *m_widget.document()->frame_grid(), view, metrics);
    const int last = timeline::frame_x(3, *m_widget.document()->frame_grid(), view, metrics);
    const int y = metrics.ruler_height() + metrics.lane_height() / 2;
    const auto move = [&](int x)
    {
        QMouseEvent event(
            QEvent::MouseMove, QPointF(x, y), QPointF(x, y), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(m_widget.viewport(), &event);
    };
    QTest::mousePress(m_widget.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(first, y));
    QFocusEvent focus(QEvent::FocusOut);
    QApplication::sendEvent(&m_widget, &focus);
    move(last);
    EXPECT_FALSE(m_widget.interaction()->selected_frames());
    QTest::mouseRelease(m_widget.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(last, y));
    QTest::mousePress(m_widget.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(first, y));
    QEvent ungrab(QEvent::UngrabMouse);
    QApplication::sendEvent(m_widget.viewport(), &ungrab);
    move(last);
    EXPECT_FALSE(m_widget.interaction()->selected_frames());
    QTest::mouseRelease(m_widget.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(last, y));
    QTest::mousePress(m_widget.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(first, y));
    QTest::mouseRelease(m_widget.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(view.width() + 10, y));
    ASSERT_TRUE(m_widget.interaction()->selected_frames());
    EXPECT_EQ(1, m_widget.interaction()->selected_frames()->first());
    EXPECT_EQ(4, m_widget.interaction()->selected_frames()->last());
    move(first);
    EXPECT_EQ(4, *m_widget.interaction()->playhead_frame());
    m_widget.zoom_in();
    m_widget.horizontalScrollBar()->setValue(m_widget.horizontalScrollBar()->maximum());
    EXPECT_GT(m_widget.timeline_viewport()->start().ticks(), view.start().ticks());
}

TEST_F(QtTimeline, renders_imported_palette_colors_and_handles_an_unloaded_widget)
{
    QTimelineWidget empty;
    EXPECT_FALSE(empty.document());
    empty.resize(240, 160);
    empty.show();
    empty.zoom_in();
    empty.zoom_out();
    empty.fit_view();
    empty.clear_selection();
    EXPECT_TRUE(empty.snapshot().empty());
    timeline_par_animator::JsonImportResult imported =
        timeline_par_animator::import_timeline_json(fixtures / "color-map-gradient.json");
    ASSERT_TRUE(imported.succeeded());
    m_widget.set_document(std::move(*imported.document));
    QApplication::processEvents();
    ASSERT_TRUE(m_widget.layout());
    const QImage image = m_widget.viewport()->grab().toImage();
    int checked = 0;
    for (const timeline::Primitive &primitive : m_widget.layout()->display_list().primitives())
    {
        if (const timeline::Swatch *swatch = std::get_if<timeline::Swatch>(&primitive);
            swatch && swatch->width > 0 && swatch->height > 0)
        {
            EXPECT_EQ(QColor(swatch->color.red(), swatch->color.green(), swatch->color.blue()),
                image.pixelColor(swatch->x + swatch->width / 2, swatch->y + swatch->height / 2));
            ++checked;
        }
    }
    EXPECT_GT(checked, 0);
}

TEST_F(QtTimeline, translates_horizontal_trackpad_pixels_without_scrolling_lanes)
{
    m_widget.zoom_in();
    ASSERT_TRUE(m_widget.timeline_viewport());
    const timeline::Time before = m_widget.timeline_viewport()->start();
    QWheelEvent scroll(QPointF(250, 80), QPointF(250, 80), QPoint(-20, 0), QPoint(), Qt::NoButton, Qt::NoModifier,
        Qt::ScrollUpdate, false);
    QApplication::sendEvent(m_widget.viewport(), &scroll);
    EXPECT_LT(before, m_widget.timeline_viewport()->start());
    EXPECT_EQ(0, m_widget.timeline_viewport()->first_lane());
}

} // namespace
