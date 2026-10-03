// Copyright (c) 2026 Richard Thomson

#include "Viewer.h"
#include <gtest/gtest.h>
#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QMenuBar>
#include <QtTest/QTest>

namespace
{
const std::filesystem::path fixtures(TIMELINE_TEST_FIXTURE_DIR);
}

TEST(QtViewer, opens_shared_animation_and_inspects_selected_frames)
{
    timeline_qt_viewer::Viewer viewer;
    viewer.resize(1000, 640);
    viewer.show();
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json"));
    QApplication::processEvents();
    ASSERT_TRUE(viewer.control().layout());
    EXPECT_EQ(25, viewer.control().document()->lane_count());
    EXPECT_NE(std::string::npos, viewer.inspector_text().find("Frame: 0"));
    viewer.control().setFocus();
    QTest::keyClick(&viewer.control(), Qt::Key_Right);
    EXPECT_NE(std::string::npos, viewer.inspector_text().find("Frame: 1"));
    EXPECT_EQ(QString::fromUtf8(viewer.inspector_text().c_str()), viewer.findChild<QPlainTextEdit *>()->toPlainText());
    ASSERT_TRUE(viewer.load_file(fixtures / "empty-animation.json"));
    EXPECT_EQ(0, viewer.control().document()->lane_count());
    EXPECT_TRUE(viewer.control().layout());
}

TEST(QtViewer, retains_document_and_selection_after_failed_import)
{
    timeline_qt_viewer::Viewer viewer;
    viewer.show();
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json"));
    QApplication::processEvents();
    QTest::keyClick(&viewer.control(), Qt::Key_Right, Qt::ShiftModifier);
    const std::string before = viewer.control().snapshot();
    const std::string inspector = viewer.inspector_text();
    ASSERT_FALSE(before.empty());
    EXPECT_FALSE(viewer.load_file(fixtures / "invalid-schema.json"));
    EXPECT_FALSE(viewer.diagnostics().empty());
    EXPECT_EQ(before, viewer.control().snapshot());
    EXPECT_EQ(inspector, viewer.inspector_text());
    ASSERT_TRUE(viewer.control().interaction()->selected_frames());
    EXPECT_EQ(1, *viewer.control().interaction()->playhead_frame());
}

TEST(QtViewer, delegates_supported_json_dispatch_and_exposes_file_open)
{
    timeline_qt_viewer::Viewer viewer;
    const QList<QAction *> menus = viewer.menuBar()->actions();
    ASSERT_EQ(1, menus.size());
    ASSERT_NE(nullptr, menus.front()->menu());
    const QList<QAction *> actions = menus.front()->menu()->actions();
    ASSERT_FALSE(actions.empty());
    EXPECT_EQ(QKeySequence(QKeySequence::Open), actions.front()->shortcut());
    ASSERT_TRUE(viewer.load_file(fixtures / "beat-keys/rms.beat-keys.json"));
    EXPECT_EQ(4, viewer.control().document()->lane_count());
    EXPECT_TRUE(viewer.diagnostics().empty());
    ASSERT_TRUE(viewer.load_file(fixtures / "beat-keys/timeline-features.json"));
    EXPECT_TRUE(viewer.control().document()->source_summary());
}
