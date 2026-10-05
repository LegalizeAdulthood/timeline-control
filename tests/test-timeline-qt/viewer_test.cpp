// Copyright (c) 2026 Richard Thomson

#include <Viewer.h>

#include <timeline/size_cast.h>

#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

namespace
{
const std::filesystem::path fixtures(TIMELINE_TEST_FIXTURE_DIR);
}

TEST(QtViewer, opensSharedAnimation)
{
    timeline_qt_viewer::Viewer viewer;
    viewer.resize(1000, 640);
    viewer.show();

    const bool loaded = viewer.load_file(fixtures / "extreme-normalized-vectors.json");
    QApplication::processEvents();

    ASSERT_TRUE(loaded);
    ASSERT_TRUE(viewer.control().layout());
    EXPECT_EQ(25, viewer.control().document()->lane_count());
}

TEST(QtViewer, presentsInitialFrameInspection)
{
    timeline_qt_viewer::Viewer viewer;
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json"));

    const std::string text = viewer.inspector_text();

    EXPECT_NE(std::string::npos, text.find("Frame: 0"));
    EXPECT_EQ(QString::fromStdString(text), viewer.findChild<QPlainTextEdit *>()->toPlainText());
}

TEST(QtViewer, updatesFrameInspectionAfterNavigation)
{
    timeline_qt_viewer::Viewer viewer;
    viewer.show();
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json"));
    QApplication::processEvents();
    viewer.control().setFocus();

    QTest::keyClick(&viewer.control(), Qt::Key_Right);

    EXPECT_NE(std::string::npos, viewer.inspector_text().find("Frame: 1"));
    EXPECT_EQ(QString::fromStdString(viewer.inspector_text()), viewer.findChild<QPlainTextEdit *>()->toPlainText());
}

TEST(QtViewer, replacesTheDocumentWithEmptyAnimation)
{
    timeline_qt_viewer::Viewer viewer;
    viewer.resize(1000, 640);
    viewer.show();
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json"));
    QApplication::processEvents();

    const bool loaded = viewer.load_file(fixtures / "empty-animation.json");

    ASSERT_TRUE(loaded);
    EXPECT_EQ(0, viewer.control().document()->lane_count());
    EXPECT_TRUE(viewer.control().layout());
}

TEST(QtViewer, preservesDocumentAndSelectionAfterFailedImport)
{
    timeline_qt_viewer::Viewer viewer;
    viewer.show();
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json"));
    QApplication::processEvents();
    QTest::keyClick(&viewer.control(), Qt::Key_Right, Qt::ShiftModifier);
    const std::string before = viewer.control().snapshot();
    const std::string inspector = viewer.inspector_text();
    ASSERT_FALSE(before.empty());

    const bool loaded = viewer.load_file(fixtures / "invalid-schema.json");

    EXPECT_FALSE(loaded);
    EXPECT_FALSE(viewer.diagnostics().empty());
    EXPECT_EQ(before, viewer.control().snapshot());
    EXPECT_EQ(inspector, viewer.inspector_text());
    ASSERT_TRUE(viewer.control().interaction()->selected_frames());
    EXPECT_EQ(1, *viewer.control().interaction()->playhead_frame());
}

TEST(QtViewer, exposesTheFileOpenShortcut)
{
    timeline_qt_viewer::Viewer viewer;
    const QList<QAction *> menus = viewer.menuBar()->actions();
    ASSERT_GE(menus.size(), 1);
    ASSERT_NE(nullptr, menus.front()->menu());
    const QList<QAction *> actions = menus.front()->menu()->actions();
    ASSERT_FALSE(actions.empty());

    const QKeySequence shortcut = actions.front()->shortcut();

    EXPECT_EQ(QKeySequence(QKeySequence::Open), shortcut);
}

TEST(QtViewer, loadsBeatKeysJson)
{
    timeline_qt_viewer::Viewer viewer;

    const bool loaded = viewer.load_file(fixtures / "beat-keys/rms.beat-keys.json");

    ASSERT_TRUE(loaded);
    EXPECT_EQ(4, viewer.control().document()->lane_count());
    EXPECT_TRUE(viewer.diagnostics().empty());
}

TEST(QtViewer, loadsTimelineFeaturesJson)
{
    timeline_qt_viewer::Viewer viewer;

    const bool loaded = viewer.load_file(fixtures / "beat-keys/timeline-features.json");

    ASSERT_TRUE(loaded);
    EXPECT_TRUE(viewer.control().document()->source_summary());
}

TEST(QtViewer, composesMusicComparison)
{
    timeline_qt_viewer::Viewer viewer;
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json"));

    const bool loaded = viewer.load_file(fixtures / "beat-keys/rms.beat-keys.json", true);

    ASSERT_TRUE(loaded);
    EXPECT_EQ(29, viewer.control().document()->lane_count());
}

TEST(QtViewer, presentsMappingRecipes)
{
    timeline_qt_viewer::Viewer viewer;
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json"));

    const bool loaded = viewer.load_file(fixtures / "beat-keys/rms.beat-keys.json", true);
    const std::string text = viewer.inspector_text();

    ASSERT_TRUE(loaded);
    ASSERT_EQ(1, timeline::size_cast(viewer.mappings()));
    EXPECT_EQ(3, timeline::size_cast(viewer.mappings().front().recipes()));
    EXPECT_NE(std::string::npos, text.find("music.rms -> camera.zoom"));
    EXPECT_NE(std::string::npos, text.find("Scale: 2"));
    EXPECT_NE(std::string::npos, text.find("Clamp:"));
}

TEST(QtViewer, addsTimelineFeatureComparison)
{
    timeline_qt_viewer::Viewer viewer;
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json"));
    ASSERT_TRUE(viewer.load_file(fixtures / "beat-keys/rms.beat-keys.json", true));
    const int lanes = viewer.control().document()->lane_count();

    const bool loaded = viewer.load_file(fixtures / "beat-keys/timeline-features.json", true);

    ASSERT_TRUE(loaded);
    EXPECT_GT(viewer.control().document()->lane_count(), lanes);
}

TEST(QtViewer, replacementClearsRecipesAndSelection)
{
    timeline_qt_viewer::Viewer viewer;
    viewer.show();
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json"));
    ASSERT_TRUE(viewer.load_file(fixtures / "beat-keys/rms.beat-keys.json", true));
    QApplication::processEvents();
    QTest::keyClick(&viewer.control(), Qt::Key_Right, Qt::ShiftModifier);

    const bool loaded = viewer.load_file(fixtures / "extreme-normalized-vectors.json");

    ASSERT_TRUE(loaded);
    EXPECT_TRUE(viewer.mappings().empty());
    EXPECT_EQ(std::string::npos, viewer.inspector_text().find("Mapping recipes:"));
    EXPECT_FALSE(viewer.control().interaction()->selected_frames());
    EXPECT_EQ(0, *viewer.control().interaction()->playhead_frame());
}

TEST(QtViewer, failedComparisonImportPreservesDisplayedState)
{
    timeline_qt_viewer::Viewer viewer;
    viewer.show();
    ASSERT_TRUE(viewer.load_file(fixtures / "beat-keys/rms.beat-keys.json"));
    QApplication::processEvents();
    QTest::keyClick(&viewer.control(), Qt::Key_Right, Qt::ShiftModifier);
    const std::string before = viewer.control().snapshot();
    const std::string inspector = viewer.inspector_text();
    const QString title = viewer.windowTitle();

    const bool loaded = viewer.load_file(fixtures / "invalid-schema.json", true);

    EXPECT_FALSE(loaded);
    EXPECT_FALSE(viewer.diagnostics().empty());
    EXPECT_EQ(before, viewer.control().snapshot());
    EXPECT_EQ(inspector, viewer.inspector_text());
    EXPECT_EQ(title, viewer.windowTitle());
    EXPECT_EQ(1, timeline::size_cast(viewer.mappings()));
}

TEST(QtViewer, failedReplacementImportPreservesDisplayedState)
{
    timeline_qt_viewer::Viewer viewer;
    viewer.show();
    ASSERT_TRUE(viewer.load_file(fixtures / "beat-keys/rms.beat-keys.json"));
    QApplication::processEvents();
    const std::string before = viewer.control().snapshot();
    const std::string inspector = viewer.inspector_text();
    ASSERT_FALSE(before.empty());

    const bool loaded = viewer.load_file(fixtures / "invalid-schema.json");

    EXPECT_FALSE(loaded);
    EXPECT_EQ(before, viewer.control().snapshot());
    EXPECT_EQ(inspector, viewer.inspector_text());
    EXPECT_EQ(1, timeline::size_cast(viewer.mappings()));
}

TEST(QtViewer, failedComparisonCompositionPreservesDisplayedState)
{
    timeline_qt_viewer::Viewer viewer;
    ASSERT_TRUE(viewer.load_file(fixtures / "beat-keys/rms.beat-keys.json"));
    viewer.control().set_document(timeline::Document(120000));
    const std::string inspector = viewer.inspector_text();

    const bool loaded = viewer.load_file(fixtures / "extreme-normalized-vectors.json", true);

    EXPECT_FALSE(loaded);
    EXPECT_FALSE(viewer.diagnostics().empty());
    EXPECT_FALSE(viewer.control().document()->frame_grid());
    EXPECT_EQ(inspector, viewer.inspector_text());
    EXPECT_EQ(1, timeline::size_cast(viewer.mappings()));
}

TEST(QtViewer, beatKeysComparisonReusesDisplayedTimebaseAndFrameGrid)
{
    timeline_qt_viewer::Viewer viewer;
    timeline_par_animator::JsonImportOptions options;
    options.ticks_per_second = 60000;
    options.frames_per_second_numerator = 24;
    timeline_par_animator::JsonImportResult base =
        timeline_par_animator::import_timeline_json(fixtures / "extreme-normalized-vectors.json", options);
    ASSERT_TRUE(base.succeeded());
    viewer.control().set_document(std::move(*base.document));

    const bool loaded = viewer.load_file(fixtures / "beat-keys/gold-write-rms-keyframes.json", true);

    ASSERT_TRUE(loaded);
    EXPECT_EQ(60000, viewer.control().document()->timebase().ticks_per_second());
    EXPECT_EQ(24, viewer.control().document()->frame_grid()->frames_per_second_numerator());
    EXPECT_GT(viewer.control().document()->lane_count(), 25);
}

TEST(QtViewer, animationComparisonReusesDisplayedFrameGrid)
{
    timeline_qt_viewer::Viewer viewer;
    timeline_par_animator::JsonImportOptions options;
    options.ticks_per_second = 60000;
    options.frames_per_second_numerator = 24;
    timeline_par_animator::JsonImportResult base =
        timeline_par_animator::import_timeline_json(fixtures / "extreme-normalized-vectors.json", options);
    ASSERT_TRUE(base.succeeded());
    viewer.control().set_document(std::move(*base.document));

    const bool loaded = viewer.load_file(fixtures / "extreme-normalized-vectors.json", true);

    ASSERT_TRUE(loaded);
    EXPECT_EQ(24, viewer.control().document()->frame_grid()->frames_per_second_numerator());
}

TEST(QtViewer, presentsGenerationMetadata)
{
    timeline_qt_viewer::Viewer viewer;

    const bool loaded = viewer.load_file(fixtures / "beat-keys/gold-write-rms-keyframes.json");
    const std::string text = viewer.inspector_text();

    ASSERT_TRUE(loaded);
    EXPECT_NE(std::string::npos, text.find("Schema: par-beatdown.beat-keys-overlay v1"));
    EXPECT_NE(std::string::npos, text.find("Generator: beat-keys 0.1.0"));
    EXPECT_NE(std::string::npos, text.find("Input base_animation:"));
    EXPECT_NE(std::string::npos, text.find("Target camera.zoom: 3"));
    EXPECT_NE(std::string::npos, text.find("Source music.rms: 9"));
    EXPECT_NE(std::string::npos, text.find("Frame extent: 0 to 4"));
}

TEST(QtViewer, presentsItemAttributes)
{
    timeline_qt_viewer::Viewer viewer;

    const bool loaded = viewer.load_file(fixtures / "beat-keys/gold-write-rms-keyframes.json");
    const std::string text = viewer.inspector_text();

    ASSERT_TRUE(loaded);
    EXPECT_NE(std::string::npos, text.find("operation: replace"));
}

TEST(QtViewer, presentsParameterOutput)
{
    timeline_qt_viewer::Viewer viewer;

    const bool loaded = viewer.load_file(fixtures / "extreme-normalized-vectors.json");
    const std::string text = viewer.inspector_text();

    ASSERT_TRUE(loaded);
    EXPECT_NE(std::string::npos, text.find("Parameter output:"));
    EXPECT_NE(std::string::npos, text.find("(exact)"));
    EXPECT_NE(std::string::npos, text.find("Ticks per second: 120000"));
}

TEST(QtViewer, presentsPalette)
{
    timeline_qt_viewer::Viewer viewer;

    const bool loaded = viewer.load_file(fixtures / "color-map-gradient.json");
    const std::string text = viewer.inspector_text();

    ASSERT_TRUE(loaded);
    EXPECT_NE(std::string::npos, text.find("Palette:"));
    EXPECT_NE(std::string::npos, text.find("RGB"));
}

TEST(QtViewer, constructsNativeMenuCommands)
{
    timeline_qt_viewer::Viewer viewer;
    QAction *add = viewer.findChild<QAction *>("add_comparison");
    QAction *export_action = viewer.findChild<QAction *>("export_snapshot");
    QAction *zoom_in = viewer.findChild<QAction *>("zoom_in");
    QAction *zoom_out = viewer.findChild<QAction *>("zoom_out");
    QAction *fit = viewer.findChild<QAction *>("fit_view");
    QAction *clear = viewer.findChild<QAction *>("clear_selection");

    const bool commands_exist = add && export_action && zoom_in && zoom_out && fit && clear;

    ASSERT_TRUE(commands_exist);
    EXPECT_FALSE(add->isEnabled());
    EXPECT_FALSE(export_action->isEnabled());
    EXPECT_FALSE(zoom_in->isEnabled());
    EXPECT_EQ(QKeySequence("Ctrl+Shift+O"), add->shortcut());
}

TEST(QtViewer, enablesDocumentCommandsAfterLoading)
{
    timeline_qt_viewer::Viewer viewer;
    viewer.show();
    QAction *add = viewer.findChild<QAction *>("add_comparison");
    QAction *export_action = viewer.findChild<QAction *>("export_snapshot");
    ASSERT_NE(nullptr, add);
    ASSERT_NE(nullptr, export_action);

    const bool loaded = viewer.load_file(fixtures / "extreme-normalized-vectors.json");
    QApplication::processEvents();

    ASSERT_TRUE(loaded);
    EXPECT_TRUE(add->isEnabled());
    EXPECT_TRUE(export_action->isEnabled());
}

TEST(QtViewer, delegatesZoomInAction)
{
    timeline_qt_viewer::Viewer viewer;
    viewer.resize(1000, 640);
    viewer.show();
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json"));
    QApplication::processEvents();
    QAction *zoom_in = viewer.findChild<QAction *>("zoom_in");
    ASSERT_NE(nullptr, zoom_in);
    const timeline::Ticks full =
        viewer.control().timeline_viewport()->end().ticks() - viewer.control().timeline_viewport()->start().ticks();

    zoom_in->trigger();

    EXPECT_LT(
        viewer.control().timeline_viewport()->end().ticks() - viewer.control().timeline_viewport()->start().ticks(),
        full);
}

TEST(QtViewer, delegatesZoomOutAction)
{
    timeline_qt_viewer::Viewer viewer;
    viewer.resize(1000, 640);
    viewer.show();
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json"));
    QApplication::processEvents();
    QAction *zoom_in = viewer.findChild<QAction *>("zoom_in");
    QAction *zoom_out = viewer.findChild<QAction *>("zoom_out");
    ASSERT_NE(nullptr, zoom_in);
    ASSERT_NE(nullptr, zoom_out);
    const timeline::Ticks full =
        viewer.control().timeline_viewport()->end().ticks() - viewer.control().timeline_viewport()->start().ticks();
    zoom_in->trigger();

    zoom_out->trigger();

    EXPECT_EQ(full,
        viewer.control().timeline_viewport()->end().ticks() - viewer.control().timeline_viewport()->start().ticks());
}

TEST(QtViewer, delegatesFitViewAction)
{
    timeline_qt_viewer::Viewer viewer;
    viewer.resize(1000, 640);
    viewer.show();
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json"));
    QApplication::processEvents();
    QAction *zoom_in = viewer.findChild<QAction *>("zoom_in");
    QAction *fit = viewer.findChild<QAction *>("fit_view");
    ASSERT_NE(nullptr, zoom_in);
    ASSERT_NE(nullptr, fit);
    const timeline::Ticks full =
        viewer.control().timeline_viewport()->end().ticks() - viewer.control().timeline_viewport()->start().ticks();
    zoom_in->trigger();

    fit->trigger();

    EXPECT_EQ(full,
        viewer.control().timeline_viewport()->end().ticks() - viewer.control().timeline_viewport()->start().ticks());
}

TEST(QtViewer, delegatesClearSelectionAction)
{
    timeline_qt_viewer::Viewer viewer;
    viewer.show();
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json"));
    QApplication::processEvents();
    QAction *clear = viewer.findChild<QAction *>("clear_selection");
    ASSERT_NE(nullptr, clear);
    QTest::keyClick(&viewer.control(), Qt::Key_Right, Qt::ShiftModifier);
    ASSERT_TRUE(viewer.control().interaction()->selected_frames());

    clear->trigger();

    EXPECT_FALSE(viewer.control().interaction()->selected_frames());
}

TEST(QtViewer, rejectsSnapshotExportWithoutDocument)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const std::filesystem::path path = std::filesystem::u8path(directory.path().toStdString()) / "timeline.txt";
    timeline_qt_viewer::Viewer viewer;

    const bool exported = viewer.export_snapshot(path);

    EXPECT_FALSE(exported);
    EXPECT_FALSE(viewer.diagnostics().empty());
}

TEST(QtViewer, exportsCurrentCoreSnapshot)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const std::filesystem::path path = std::filesystem::u8path(directory.path().toStdString()) / "timeline.txt";
    timeline_qt_viewer::Viewer viewer;
    viewer.show();
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json"));
    QApplication::processEvents();
    const std::string before = viewer.control().snapshot();

    const bool exported = viewer.export_snapshot(path);

    ASSERT_TRUE(exported);
    std::ifstream input(path, std::ios::binary);
    const std::string actual((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    EXPECT_EQ(before, actual);
    EXPECT_TRUE(viewer.diagnostics().empty());
}

TEST(QtViewer, preservesDisplayedStateOnSnapshotWriteFailure)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const std::filesystem::path path =
        std::filesystem::u8path(directory.path().toStdString()) / "missing" / "snapshot.txt";
    timeline_qt_viewer::Viewer viewer;
    viewer.show();
    ASSERT_TRUE(viewer.load_file(fixtures / "extreme-normalized-vectors.json"));
    QApplication::processEvents();
    QTest::keyClick(&viewer.control(), Qt::Key_Right, Qt::ShiftModifier);
    const std::string before = viewer.control().snapshot();
    const std::string inspector = viewer.inspector_text();

    const bool exported = viewer.export_snapshot(path);

    EXPECT_FALSE(exported);
    EXPECT_FALSE(viewer.diagnostics().empty());
    EXPECT_EQ(before, viewer.control().snapshot());
    EXPECT_EQ(inspector, viewer.inspector_text());
}

TEST(QtViewer, loadsOptionalStartupPath)
{
    const std::filesystem::path path = fixtures / "extreme-normalized-vectors.json";

    timeline_qt_viewer::Viewer viewer(path);

    ASSERT_TRUE(viewer.control().document());
    EXPECT_EQ(25, viewer.control().document()->lane_count());
}

TEST(QtViewer, reportsStartupFailure)
{
    const std::filesystem::path path = fixtures / "invalid-schema.json";

    timeline_qt_viewer::Viewer viewer(path);

    EXPECT_FALSE(viewer.control().document());
    EXPECT_FALSE(viewer.diagnostics().empty());
}
