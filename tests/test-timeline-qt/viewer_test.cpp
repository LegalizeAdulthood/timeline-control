// Copyright (c) 2026 Richard Thomson

#include <Viewer.h>

#include <timeline/size_cast.h>

#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QMenuBar>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include <fstream>
#include <iterator>

namespace
{
const std::filesystem::path fixtures(TIMELINE_TEST_FIXTURE_DIR);
}

TEST(QtViewer, opensSharedAnimationAndInspectsSelectedFrames)
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
    EXPECT_EQ(QString::fromStdString(viewer.inspector_text()), viewer.findChild<QPlainTextEdit *>()->toPlainText());
    ASSERT_TRUE(viewer.load_file(fixtures / "empty-animation.json"));
    EXPECT_EQ(0, viewer.control().document()->lane_count());
    EXPECT_TRUE(viewer.control().layout());
}

TEST(QtViewer, retainsDocumentAndSelectionAfterFailedImport)
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

TEST(QtViewer, delegatesSupportedJsonDispatchAndExposesFileOpen)
{
    timeline_qt_viewer::Viewer viewer;
    const QList<QAction *> menus = viewer.menuBar()->actions();
    ASSERT_GE(menus.size(), 1);
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

TEST(QtViewer, composesMusicRecipesAndResetsThemOnReplacement)
{
    timeline_qt_viewer::Viewer viewer;
    viewer.resize(1000, 640);
    viewer.show();
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json"));
    ASSERT_TRUE(viewer.load_file(fixtures / "beat-keys/rms.beat-keys.json", true));
    QApplication::processEvents();
    EXPECT_EQ(29, viewer.control().document()->lane_count());
    ASSERT_EQ(1, timeline::size_cast(viewer.mappings()));
    EXPECT_EQ(3, timeline::size_cast(viewer.mappings().front().recipes()));
    EXPECT_NE(std::string::npos, viewer.inspector_text().find("music.rms -> camera.zoom"));
    EXPECT_NE(std::string::npos, viewer.inspector_text().find("Scale: 2"));
    EXPECT_NE(std::string::npos, viewer.inspector_text().find("Clamp:"));
    ASSERT_TRUE(viewer.load_file(fixtures / "beat-keys/timeline-features.json", true));
    EXPECT_GT(viewer.control().document()->lane_count(), 29);
    QTest::keyClick(&viewer.control(), Qt::Key_Right, Qt::ShiftModifier);
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json"));
    EXPECT_TRUE(viewer.mappings().empty());
    EXPECT_EQ(std::string::npos, viewer.inspector_text().find("Mapping recipes:"));
    EXPECT_FALSE(viewer.control().interaction()->selected_frames());
    EXPECT_EQ(0, *viewer.control().interaction()->playhead_frame());
}

TEST(QtViewer, comparisonImportAndCompositionFailuresPreserveDisplayedState)
{
    timeline_qt_viewer::Viewer viewer;
    viewer.show();
    ASSERT_TRUE(viewer.load_file(fixtures / "beat-keys/rms.beat-keys.json"));
    QApplication::processEvents();
    QTest::keyClick(&viewer.control(), Qt::Key_Right, Qt::ShiftModifier);
    const std::string before = viewer.control().snapshot();
    const std::string inspector = viewer.inspector_text();
    const QString title = viewer.windowTitle();
    EXPECT_FALSE(viewer.load_file(fixtures / "invalid-schema.json", true));
    EXPECT_FALSE(viewer.diagnostics().empty());
    EXPECT_EQ(before, viewer.control().snapshot());
    EXPECT_EQ(inspector, viewer.inspector_text());
    EXPECT_EQ(title, viewer.windowTitle());
    EXPECT_EQ(1, timeline::size_cast(viewer.mappings()));
    EXPECT_FALSE(viewer.load_file(fixtures / "invalid-schema.json"));
    EXPECT_EQ(before, viewer.control().snapshot());
    EXPECT_EQ(inspector, viewer.inspector_text());
    EXPECT_EQ(1, timeline::size_cast(viewer.mappings()));
    viewer.control().set_document(timeline::Document(120000));
    const std::string empty = viewer.inspector_text();
    EXPECT_FALSE(viewer.load_file(fixtures / "extreme-normalized-vectors.json", true));
    EXPECT_FALSE(viewer.diagnostics().empty());
    EXPECT_FALSE(viewer.control().document()->frame_grid());
    EXPECT_EQ(empty, viewer.inspector_text());
    EXPECT_EQ(1, timeline::size_cast(viewer.mappings()));
}

TEST(QtViewer, comparisonReusesTheDisplayedTimebaseAndFrameGrid)
{
    timeline_qt_viewer::Viewer viewer;
    timeline_par_animator::JsonImportOptions options;
    options.ticks_per_second = 60000;
    options.frames_per_second_numerator = 24;
    timeline_par_animator::JsonImportResult base =
        timeline_par_animator::import_timeline_json(fixtures / "extreme-normalized-vectors.json", options);
    ASSERT_TRUE(base.succeeded());
    viewer.control().set_document(std::move(*base.document));
    ASSERT_TRUE(viewer.load_file(fixtures / "beat-keys/gold-write-rms-keyframes.json", true));
    EXPECT_EQ(60000, viewer.control().document()->timebase().ticks_per_second());
    EXPECT_EQ(24, viewer.control().document()->frame_grid()->frames_per_second_numerator());
    EXPECT_GT(viewer.control().document()->lane_count(), 25);
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json", true));
    EXPECT_EQ(24, viewer.control().document()->frame_grid()->frames_per_second_numerator());
}

TEST(QtViewer, exposesGenerationMetadataAttributesOutputsAndPalettes)
{
    timeline_qt_viewer::Viewer viewer;
    ASSERT_TRUE(viewer.load_file(fixtures / "beat-keys/gold-write-rms-keyframes.json"));
    const std::string generated = viewer.inspector_text();
    EXPECT_NE(std::string::npos, generated.find("Schema: par-beatdown.beat-keys-overlay v1"));
    EXPECT_NE(std::string::npos, generated.find("Generator: beat-keys 0.1.0"));
    EXPECT_NE(std::string::npos, generated.find("Input base_animation:"));
    EXPECT_NE(std::string::npos, generated.find("Target camera.zoom: 3"));
    EXPECT_NE(std::string::npos, generated.find("Source music.rms: 9"));
    EXPECT_NE(std::string::npos, generated.find("Frame extent: 0 to 4"));
    EXPECT_NE(std::string::npos, generated.find("operation: replace"));
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json"));
    EXPECT_NE(std::string::npos, viewer.inspector_text().find("Parameter output:"));
    EXPECT_NE(std::string::npos, viewer.inspector_text().find("(exact)"));
    EXPECT_NE(std::string::npos, viewer.inspector_text().find("Ticks per second: 120000"));
    ASSERT_TRUE(viewer.load_file(fixtures / "color-map-gradient.json"));
    EXPECT_NE(std::string::npos, viewer.inspector_text().find("Palette:"));
    EXPECT_NE(std::string::npos, viewer.inspector_text().find("RGB"));
}

TEST(QtViewer, exposesNativeMenuCommandsAndDelegatesViewActions)
{
    timeline_qt_viewer::Viewer viewer;
    viewer.resize(1000, 640);
    viewer.show();
    QAction *add = viewer.findChild<QAction *>("add_comparison");
    QAction *export_action = viewer.findChild<QAction *>("export_snapshot");
    QAction *zoom_in = viewer.findChild<QAction *>("zoom_in");
    QAction *zoom_out = viewer.findChild<QAction *>("zoom_out");
    QAction *fit = viewer.findChild<QAction *>("fit_view");
    QAction *clear = viewer.findChild<QAction *>("clear_selection");
    ASSERT_NE(nullptr, add);
    ASSERT_NE(nullptr, export_action);
    ASSERT_NE(nullptr, zoom_in);
    ASSERT_NE(nullptr, zoom_out);
    ASSERT_NE(nullptr, fit);
    ASSERT_NE(nullptr, clear);
    EXPECT_FALSE(add->isEnabled());
    EXPECT_FALSE(export_action->isEnabled());
    EXPECT_FALSE(zoom_in->isEnabled());
    EXPECT_EQ(QKeySequence("Ctrl+Shift+O"), add->shortcut());
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json"));
    QApplication::processEvents();
    EXPECT_TRUE(add->isEnabled());
    EXPECT_TRUE(export_action->isEnabled());
    const timeline::Ticks full =
        viewer.control().timeline_viewport()->end().ticks() - viewer.control().timeline_viewport()->start().ticks();
    zoom_in->trigger();
    EXPECT_LT(
        viewer.control().timeline_viewport()->end().ticks() - viewer.control().timeline_viewport()->start().ticks(),
        full);
    zoom_out->trigger();
    EXPECT_EQ(full,
        viewer.control().timeline_viewport()->end().ticks() - viewer.control().timeline_viewport()->start().ticks());
    fit->trigger();
    EXPECT_EQ(full,
        viewer.control().timeline_viewport()->end().ticks() - viewer.control().timeline_viewport()->start().ticks());
    QTest::keyClick(&viewer.control(), Qt::Key_Right, Qt::ShiftModifier);
    EXPECT_NE(std::string::npos, viewer.inspector_text().find("Selected range:"));
    clear->trigger();
    EXPECT_FALSE(viewer.control().interaction()->selected_frames());
}

TEST(QtViewer, exportsCurrentCoreSnapshotAndKeepsStateOnWriteFailure)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const std::filesystem::path path = std::filesystem::u8path(directory.path().toStdString()) / "timeline.txt";
    timeline_qt_viewer::Viewer viewer;
    EXPECT_FALSE(viewer.export_snapshot(path));
    EXPECT_FALSE(viewer.diagnostics().empty());
    viewer.show();
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json"));
    QApplication::processEvents();
    QTest::keyClick(&viewer.control(), Qt::Key_Right, Qt::ShiftModifier);
    const std::string before = viewer.control().snapshot();
    const std::string inspector = viewer.inspector_text();
    ASSERT_TRUE(viewer.export_snapshot(path));
    std::ifstream input(path, std::ios::binary);
    const std::string actual((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    EXPECT_EQ(before, actual);
    EXPECT_TRUE(viewer.diagnostics().empty());
    EXPECT_FALSE(viewer.export_snapshot(path / "missing" / "snapshot.txt"));
    EXPECT_FALSE(viewer.diagnostics().empty());
    EXPECT_EQ(before, viewer.control().snapshot());
    EXPECT_EQ(inspector, viewer.inspector_text());
}

TEST(QtViewer, acceptsOptionalStartupPathAndReportsStartupFailure)
{
    timeline_qt_viewer::Viewer viewer(fixtures / "extreme-normalized-vectors.json");
    ASSERT_TRUE(viewer.control().document());
    EXPECT_EQ(25, viewer.control().document()->lane_count());
    timeline_qt_viewer::Viewer failed(fixtures / "invalid-schema.json");
    EXPECT_FALSE(failed.control().document());
    EXPECT_FALSE(failed.diagnostics().empty());
}
