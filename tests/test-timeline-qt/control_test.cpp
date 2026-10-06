// Copyright (c) 2026 Richard Thomson

#include <qtTimeline/QTimelineWidget.h>

#include <timelineParAnimator/TimelineJson.h>

#include <timeline/size_cast.h>
#include <timeline/Snapshot.h>

#include <gtest/gtest.h>

#include <QApplication>
#include <QFocusEvent>
#include <QImage>
#include <QMouseEvent>
#include <QScrollBar>
#include <QSignalSpy>
#include <QtTest/QTest>
#include <QWheelEvent>

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace
{

const std::filesystem::path fixtures(TIMELINE_TEST_FIXTURE_DIR);

/// Real headless Qt widget exercising shared source fixtures and native input.
///
class QtTimeline : public testing::Test
{
protected:
    void SetUp() override;
    QPoint item_point(int frame) const;
    QPoint lane_point(int frame) const;
    std::optional<QPoint> source_item_point() const;
    void drag_move(QPoint point);

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

QPoint QtTimeline::item_point(int frame) const
{
    const timeline::LayoutMetrics &metrics = *m_widget.layout_metrics();
    const int x = timeline::frame_x(frame, *m_widget.document()->frame_grid(), *m_widget.timeline_viewport(), metrics);
    return QPoint(x, metrics.ruler_height() + metrics.item_padding());
}

QPoint QtTimeline::lane_point(int frame) const
{
    const timeline::LayoutMetrics &metrics = *m_widget.layout_metrics();
    const int x = timeline::frame_x(frame, *m_widget.document()->frame_grid(), *m_widget.timeline_viewport(), metrics);
    return QPoint(x, metrics.ruler_height() + metrics.lane_height() / 2);
}

std::optional<QPoint> QtTimeline::source_item_point() const
{
    const timeline::DisplayList list = m_widget.layout()->display_list();
    for (const timeline::Primitive &primitive : list.primitives())
    {
        if (const timeline::Polyline *line = std::get_if<timeline::Polyline>(&primitive);
            line && list.strings().lookup(line->id.lane_id) == "animation-0[0]")
        {
            const timeline::Point point = line->points[line->points.size() / 4];
            return QPoint(point.x, point.y);
        }
    }
    return {};
}

void QtTimeline::drag_move(QPoint point)
{
    QMouseEvent event(QEvent::MouseMove, QPointF(point), QPointF(point), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(m_widget.viewport(), &event);
}

TEST_F(QtTimeline, exposesTheImportedDocument)
{
    const std::optional<timeline::Document> &document = m_widget.document();
    ASSERT_TRUE(document);

    const int lanes = document->lane_count();

    EXPECT_EQ(25, lanes);
}

TEST_F(QtTimeline, roundTripsStyleColor)
{
    const QColor expected(12, 34, 56);

    m_widget.set_style_color(timeline::StyleRole::CURVE, expected);

    EXPECT_EQ(expected, m_widget.style_color(timeline::StyleRole::CURVE));
}

TEST_F(QtTimeline, usesTheCoreDisplayList)
{
    ASSERT_TRUE(m_widget.layout());
    ASSERT_TRUE(m_widget.timeline_viewport());
    ASSERT_TRUE(m_widget.layout_metrics());
    const timeline::Layout expected(
        *m_widget.document(), *m_widget.timeline_viewport(), *m_widget.layout_metrics(), *m_widget.interaction());

    const std::string snapshot = m_widget.snapshot();

    EXPECT_EQ(timeline::render_snapshot(expected.display_list()), snapshot);
}

TEST_F(QtTimeline, paintsTheTimeline)
{
    const QColor background = m_widget.palette().color(QPalette::Base);

    const QImage image = m_widget.viewport()->grab().toImage();
    bool painted = false;
    for (int y = 0; y < image.height(); y += 7)
    {
        for (int x = 0; x < image.width(); x += 11)
        {
            painted = painted || image.pixelColor(x, y) != background;
        }
    }

    EXPECT_TRUE(painted);
}

TEST_F(QtTimeline, selectsSourceItems)
{
    ASSERT_TRUE(m_widget.layout_metrics());
    ASSERT_TRUE(m_widget.timeline_viewport());
    const QPoint point = item_point(1);

    QTest::mouseClick(m_widget.viewport(), Qt::LeftButton, Qt::NoModifier, point);

    ASSERT_TRUE(m_widget.interaction());
    EXPECT_EQ(1, *m_widget.interaction()->playhead_frame());
    EXPECT_EQ("animation-0[0]", m_widget.document()->strings().lookup(*m_widget.interaction()->selected_lane()));
}

TEST_F(QtTimeline, stepsFramesWithNativeKeys)
{
    QTest::mouseClick(m_widget.viewport(), Qt::LeftButton, Qt::NoModifier, item_point(1));

    QTest::keyClick(&m_widget, Qt::Key_Right);

    EXPECT_EQ(2, *m_widget.interaction()->playhead_frame());
}

TEST_F(QtTimeline, extendsFrameSelectionWithShift)
{
    QTest::mouseClick(m_widget.viewport(), Qt::LeftButton, Qt::NoModifier, item_point(1));
    QTest::keyClick(&m_widget, Qt::Key_Right);

    QTest::keyClick(&m_widget, Qt::Key_Right, Qt::ShiftModifier);

    ASSERT_TRUE(m_widget.interaction()->selected_frames());
    EXPECT_EQ(2, m_widget.interaction()->selected_frames()->first());
    EXPECT_EQ(3, m_widget.inspection()->frame);
}

TEST_F(QtTimeline, clearsSelectionWithEscape)
{
    QTest::mouseClick(m_widget.viewport(), Qt::LeftButton, Qt::NoModifier, item_point(1));

    QTest::keyClick(&m_widget, Qt::Key_Escape);

    EXPECT_TRUE(m_widget.interaction()->selected_items().empty());
}

TEST_F(QtTimeline, zoomsIn)
{
    const timeline::Ticks before =
        m_widget.timeline_viewport()->end().ticks() - m_widget.timeline_viewport()->start().ticks();

    m_widget.zoom_in();
    QApplication::processEvents();

    EXPECT_LT(m_widget.timeline_viewport()->end().ticks() - m_widget.timeline_viewport()->start().ticks(), before);
}

TEST_F(QtTimeline, scrollsVertically)
{
    const int first_lane = m_widget.timeline_viewport()->first_lane();

    m_widget.verticalScrollBar()->setValue(2);

    EXPECT_EQ(first_lane + 2, m_widget.timeline_viewport()->first_lane());
}

TEST_F(QtTimeline, fitsTheCompleteView)
{
    const timeline::Ticks before =
        m_widget.timeline_viewport()->end().ticks() - m_widget.timeline_viewport()->start().ticks();
    m_widget.zoom_in();
    QApplication::processEvents();

    m_widget.fit_view();
    QApplication::processEvents();

    EXPECT_EQ(before, m_widget.timeline_viewport()->end().ticks() - m_widget.timeline_viewport()->start().ticks());
}

TEST_F(QtTimeline, replacesTheDocumentWithEmptyContent)
{
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
}

TEST_F(QtTimeline, leavesFramelessReplacementUnlaidOut)
{
    const timeline::Document document(100);

    m_widget.set_document(document);

    EXPECT_FALSE(m_widget.layout());
    EXPECT_FALSE(m_widget.inspection());
}

TEST_F(QtTimeline, dragsFrameRanges)
{
    const QPoint first = lane_point(1);
    const QPoint last = lane_point(3);
    QSignalSpy changed(&m_widget, &QTimelineWidget::inspection_changed);

    QTest::mousePress(m_widget.viewport(), Qt::LeftButton, Qt::NoModifier, first);
    QTest::mouseMove(m_widget.viewport(), last);
    QTest::mouseRelease(m_widget.viewport(), Qt::LeftButton, Qt::NoModifier, last);

    ASSERT_TRUE(m_widget.interaction()->selected_frames());
    EXPECT_EQ(1, m_widget.interaction()->selected_frames()->first());
    EXPECT_EQ(3, m_widget.interaction()->selected_frames()->last());
    EXPECT_GT(changed.count(), 0);
}

TEST_F(QtTimeline, clearsHoverOnLeave)
{
    const std::optional<QPoint> point = source_item_point();
    ASSERT_TRUE(point);
    QTest::mouseClick(m_widget.viewport(), Qt::LeftButton, Qt::NoModifier, *point);
    ASSERT_TRUE(m_widget.hit_result());
    QEvent leave(QEvent::Leave);

    QApplication::sendEvent(m_widget.viewport(), &leave);

    EXPECT_FALSE(m_widget.hit_result());
}

TEST_F(QtTimeline, translatesCtrlWheelToZoom)
{
    const timeline::Ticks before =
        m_widget.timeline_viewport()->end().ticks() - m_widget.timeline_viewport()->start().ticks();
    QWheelEvent zoom(QPointF(250, 80), QPointF(250, 80), QPoint(), QPoint(0, 120), Qt::NoButton, Qt::ControlModifier,
        Qt::NoScrollPhase, false);

    QApplication::sendEvent(m_widget.viewport(), &zoom);

    EXPECT_LT(m_widget.timeline_viewport()->end().ticks() - m_widget.timeline_viewport()->start().ticks(), before);
}

TEST_F(QtTimeline, translatesWheelToLaneScroll)
{
    QWheelEvent scroll(QPointF(250, 80), QPointF(250, 80), QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier,
        Qt::NoScrollPhase, false);

    QApplication::sendEvent(m_widget.viewport(), &scroll);

    EXPECT_EQ(1, m_widget.timeline_viewport()->first_lane());
}

TEST_F(QtTimeline, reflowsLayoutForFontChanges)
{
    const int old_height = m_widget.layout_metrics()->lane_height();
    QFont font = m_widget.font();
    font.setPointSize(20);

    m_widget.setFont(font);
    QApplication::processEvents();

    EXPECT_GT(m_widget.layout_metrics()->lane_height(), old_height);
}

TEST_F(QtTimeline, updatesViewportAfterResize)
{
    const int width = m_widget.timeline_viewport()->width();

    m_widget.resize(240, 160);
    QApplication::processEvents();

    ASSERT_TRUE(m_widget.timeline_viewport());
    EXPECT_LT(m_widget.timeline_viewport()->width(), width);
    EXPECT_LT(m_widget.timeline_viewport()->width(), 240);
}

TEST_F(QtTimeline, preservesSourceItemIdentity)
{
    const std::optional<QPoint> point = source_item_point();
    ASSERT_TRUE(point);

    QTest::mouseClick(m_widget.viewport(), Qt::LeftButton, Qt::NoModifier, *point);

    ASSERT_EQ(1, timeline::size_cast(m_widget.interaction()->selected_items()));
    EXPECT_EQ("animation-0-key-0",
        m_widget.document()->strings().lookup(m_widget.interaction()->selected_items().front().item_id));
}

TEST_F(QtTimeline, ownsACopyOfTheDocument)
{
    timeline::Document copy = *m_widget.document();

    m_widget.set_document(copy);
    copy = timeline::Document(100);

    EXPECT_EQ(25, m_widget.document()->lane_count());
}

TEST_F(QtTimeline, clearsSelectionOnDocumentReplacement)
{
    const std::optional<QPoint> point = source_item_point();
    ASSERT_TRUE(point);
    QTest::mouseClick(m_widget.viewport(), Qt::LeftButton, Qt::NoModifier, *point);
    ASSERT_FALSE(m_widget.interaction()->selected_items().empty());
    const timeline::Document copy = *m_widget.document();

    m_widget.set_document(copy);

    EXPECT_TRUE(m_widget.interaction()->selected_items().empty());
}

TEST_F(QtTimeline, cancelsDraggingOnFocusLoss)
{
    const QPoint first = lane_point(1);
    const QPoint last = lane_point(3);
    QTest::mousePress(m_widget.viewport(), Qt::LeftButton, Qt::NoModifier, first);
    QFocusEvent focus(QEvent::FocusOut);

    QApplication::sendEvent(&m_widget, &focus);
    drag_move(last);

    EXPECT_FALSE(m_widget.interaction()->selected_frames());
}

TEST_F(QtTimeline, cancelsDraggingOnCaptureLoss)
{
    const QPoint first = lane_point(1);
    const QPoint last = lane_point(3);
    QTest::mousePress(m_widget.viewport(), Qt::LeftButton, Qt::NoModifier, first);
    QEvent ungrab(QEvent::UngrabMouse);

    QApplication::sendEvent(m_widget.viewport(), &ungrab);
    drag_move(last);

    EXPECT_FALSE(m_widget.interaction()->selected_frames());
}

TEST_F(QtTimeline, finishesDraggingOutsideTheView)
{
    const QPoint first = lane_point(1);
    const int y = first.y();
    const QPoint outside(m_widget.timeline_viewport()->width() + 10, y);

    QTest::mousePress(m_widget.viewport(), Qt::LeftButton, Qt::NoModifier, first);
    QTest::mouseRelease(m_widget.viewport(), Qt::LeftButton, Qt::NoModifier, outside);
    drag_move(first);

    ASSERT_TRUE(m_widget.interaction()->selected_frames());
    EXPECT_EQ(1, m_widget.interaction()->selected_frames()->first());
    EXPECT_EQ(4, m_widget.interaction()->selected_frames()->last());
    EXPECT_EQ(4, *m_widget.interaction()->playhead_frame());
}

TEST_F(QtTimeline, scrollsHorizontally)
{
    const timeline::Time before = m_widget.timeline_viewport()->start();
    m_widget.zoom_in();

    m_widget.horizontalScrollBar()->setValue(m_widget.horizontalScrollBar()->maximum());

    EXPECT_GT(m_widget.timeline_viewport()->start().ticks(), before.ticks());
}

TEST_F(QtTimeline, rendersImportedPaletteColors)
{
    timeline_par_animator::JsonImportResult imported =
        timeline_par_animator::import_timeline_json(fixtures / "color-map-gradient.json");
    ASSERT_TRUE(imported.succeeded());
    m_widget.set_document(std::move(*imported.document));
    QApplication::processEvents();
    ASSERT_TRUE(m_widget.layout());

    const QImage image = m_widget.viewport()->grab().toImage();
    int checked = 0;
    bool colors_match = true;
    for (const timeline::Primitive &primitive : m_widget.layout()->display_list().primitives())
    {
        if (const timeline::Swatch *swatch = std::get_if<timeline::Swatch>(&primitive);
            swatch && swatch->width > 0 && swatch->height > 0)
        {
            colors_match = colors_match &&
                QColor(swatch->color.red(), swatch->color.green(), swatch->color.blue()) ==
                    image.pixelColor(swatch->x + swatch->width / 2, swatch->y + swatch->height / 2);
            ++checked;
        }
    }

    EXPECT_GT(checked, 0);
    EXPECT_TRUE(colors_match);
}

TEST_F(QtTimeline, handlesAnUnloadedWidget)
{
    QTimelineWidget empty;
    empty.resize(240, 160);
    empty.show();

    empty.zoom_in();
    empty.zoom_out();
    empty.fit_view();
    empty.clear_selection();

    EXPECT_FALSE(empty.document());
    EXPECT_TRUE(empty.snapshot().empty());
}

TEST_F(QtTimeline, translatesHorizontalTrackpadPixelsWithoutScrollingLanes)
{
    m_widget.zoom_in();
    const timeline::Time before = m_widget.timeline_viewport()->start();
    QWheelEvent scroll(QPointF(250, 80), QPointF(250, 80), QPoint(-20, 0), QPoint(), Qt::NoButton, Qt::NoModifier,
        Qt::ScrollUpdate, false);

    QApplication::sendEvent(m_widget.viewport(), &scroll);

    EXPECT_LT(before, m_widget.timeline_viewport()->start());
    EXPECT_EQ(0, m_widget.timeline_viewport()->first_lane());
}

} // namespace
