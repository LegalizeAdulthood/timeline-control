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

/// Fresh native viewer for unloaded and alternate-document scenarios.
///
class QtViewerTest : public testing::Test
{
protected:
    timeline_qt_viewer::Viewer m_viewer;
};

/// Native viewer showing the shared animation at canonical geometry.
///
class LoadedQtViewerTest : public QtViewerTest
{
protected:
    void SetUp() override
    {
        m_viewer.resize(1000, 640);
        m_viewer.show();
        ASSERT_TRUE(m_viewer.load_file(fixtures / "extreme-normalized-vectors.json"));
        QApplication::processEvents();
    }
};
} // namespace

TEST_F(LoadedQtViewerTest, opensSharedAnimation)
{
    ASSERT_TRUE(m_viewer.control().layout());
    EXPECT_EQ(25, m_viewer.control().document()->lane_count());
}

TEST_F(LoadedQtViewerTest, presentsInitialFrameInspection)
{
    const std::string text = m_viewer.inspector_text();

    EXPECT_NE(std::string::npos, text.find("Frame: 0"));
    EXPECT_EQ(QString::fromStdString(text), m_viewer.findChild<QPlainTextEdit *>()->toPlainText());
}

TEST_F(LoadedQtViewerTest, updatesFrameInspectionAfterNavigation)
{
    m_viewer.control().setFocus();

    QTest::keyClick(&m_viewer.control(), Qt::Key_Right);

    EXPECT_NE(std::string::npos, m_viewer.inspector_text().find("Frame: 1"));
    EXPECT_EQ(QString::fromStdString(m_viewer.inspector_text()), m_viewer.findChild<QPlainTextEdit *>()->toPlainText());
}

TEST_F(LoadedQtViewerTest, replacesTheDocumentWithEmptyAnimation)
{
    const bool loaded = m_viewer.load_file(fixtures / "empty-animation.json");

    ASSERT_TRUE(loaded);
    EXPECT_EQ(0, m_viewer.control().document()->lane_count());
    EXPECT_TRUE(m_viewer.control().layout());
}

TEST_F(LoadedQtViewerTest, preservesDocumentAndSelectionAfterFailedImport)
{
    QTest::keyClick(&m_viewer.control(), Qt::Key_Right, Qt::ShiftModifier);
    const std::string before = m_viewer.control().snapshot();
    const std::string inspector = m_viewer.inspector_text();
    ASSERT_FALSE(before.empty());

    const bool loaded = m_viewer.load_file(fixtures / "invalid-schema.json");

    EXPECT_FALSE(loaded);
    EXPECT_FALSE(m_viewer.diagnostics().empty());
    EXPECT_EQ(before, m_viewer.control().snapshot());
    EXPECT_EQ(inspector, m_viewer.inspector_text());
    ASSERT_TRUE(m_viewer.control().interaction()->selected_frames());
    EXPECT_EQ(1, *m_viewer.control().interaction()->playhead_frame());
}

TEST_F(QtViewerTest, exposesTheFileOpenShortcut)
{
    const QList<QAction *> menus = m_viewer.menuBar()->actions();
    ASSERT_GE(menus.size(), 1);
    ASSERT_NE(nullptr, menus.front()->menu());
    const QList<QAction *> actions = menus.front()->menu()->actions();
    ASSERT_FALSE(actions.empty());

    const QKeySequence shortcut = actions.front()->shortcut();

    EXPECT_EQ(QKeySequence(QKeySequence::Open), shortcut);
}

TEST_F(QtViewerTest, loadsBeatKeysJson)
{
    const bool loaded = m_viewer.load_file(fixtures / "beat-keys/rms.beat-keys.json");

    ASSERT_TRUE(loaded);
    EXPECT_EQ(4, m_viewer.control().document()->lane_count());
    EXPECT_TRUE(m_viewer.diagnostics().empty());
}

TEST_F(QtViewerTest, loadsTimelineFeaturesJson)
{
    const bool loaded = m_viewer.load_file(fixtures / "beat-keys/timeline-features.json");

    ASSERT_TRUE(loaded);
    EXPECT_TRUE(m_viewer.control().document()->source_summary());
}

TEST_F(LoadedQtViewerTest, composesMusicComparison)
{
    const bool loaded = m_viewer.load_file(fixtures / "beat-keys/rms.beat-keys.json", true);

    ASSERT_TRUE(loaded);
    EXPECT_EQ(29, m_viewer.control().document()->lane_count());
}

TEST_F(LoadedQtViewerTest, presentsMappingRecipes)
{
    const bool loaded = m_viewer.load_file(fixtures / "beat-keys/rms.beat-keys.json", true);
    const std::string text = m_viewer.inspector_text();

    ASSERT_TRUE(loaded);
    ASSERT_EQ(1, timeline::size_cast(m_viewer.mappings()));
    EXPECT_EQ(3, timeline::size_cast(m_viewer.mappings().front().recipes()));
    EXPECT_NE(std::string::npos, text.find("music.rms -> camera.zoom"));
    EXPECT_NE(std::string::npos, text.find("Scale: 2"));
    EXPECT_NE(std::string::npos, text.find("Clamp:"));
}

TEST_F(LoadedQtViewerTest, addsTimelineFeatureComparison)
{
    ASSERT_TRUE(m_viewer.load_file(fixtures / "beat-keys/rms.beat-keys.json", true));
    const int lanes = m_viewer.control().document()->lane_count();

    const bool loaded = m_viewer.load_file(fixtures / "beat-keys/timeline-features.json", true);

    ASSERT_TRUE(loaded);
    EXPECT_GT(m_viewer.control().document()->lane_count(), lanes);
}

TEST_F(LoadedQtViewerTest, replacementClearsRecipesAndSelection)
{
    ASSERT_TRUE(m_viewer.load_file(fixtures / "beat-keys/rms.beat-keys.json", true));
    QTest::keyClick(&m_viewer.control(), Qt::Key_Right, Qt::ShiftModifier);

    const bool loaded = m_viewer.load_file(fixtures / "extreme-normalized-vectors.json");

    ASSERT_TRUE(loaded);
    EXPECT_TRUE(m_viewer.mappings().empty());
    EXPECT_EQ(std::string::npos, m_viewer.inspector_text().find("Mapping recipes:"));
    EXPECT_FALSE(m_viewer.control().interaction()->selected_frames());
    EXPECT_EQ(0, *m_viewer.control().interaction()->playhead_frame());
}

TEST_F(QtViewerTest, failedComparisonImportPreservesDisplayedState)
{
    m_viewer.show();
    ASSERT_TRUE(m_viewer.load_file(fixtures / "beat-keys/rms.beat-keys.json"));
    QApplication::processEvents();
    QTest::keyClick(&m_viewer.control(), Qt::Key_Right, Qt::ShiftModifier);
    const std::string before = m_viewer.control().snapshot();
    const std::string inspector = m_viewer.inspector_text();
    const QString title = m_viewer.windowTitle();

    const bool loaded = m_viewer.load_file(fixtures / "invalid-schema.json", true);

    EXPECT_FALSE(loaded);
    EXPECT_FALSE(m_viewer.diagnostics().empty());
    EXPECT_EQ(before, m_viewer.control().snapshot());
    EXPECT_EQ(inspector, m_viewer.inspector_text());
    EXPECT_EQ(title, m_viewer.windowTitle());
    EXPECT_EQ(1, timeline::size_cast(m_viewer.mappings()));
}

TEST_F(QtViewerTest, failedReplacementImportPreservesDisplayedState)
{
    m_viewer.show();
    ASSERT_TRUE(m_viewer.load_file(fixtures / "beat-keys/rms.beat-keys.json"));
    QApplication::processEvents();
    const std::string before = m_viewer.control().snapshot();
    const std::string inspector = m_viewer.inspector_text();
    ASSERT_FALSE(before.empty());

    const bool loaded = m_viewer.load_file(fixtures / "invalid-schema.json");

    EXPECT_FALSE(loaded);
    EXPECT_EQ(before, m_viewer.control().snapshot());
    EXPECT_EQ(inspector, m_viewer.inspector_text());
    EXPECT_EQ(1, timeline::size_cast(m_viewer.mappings()));
}

TEST_F(QtViewerTest, failedComparisonCompositionPreservesDisplayedState)
{
    ASSERT_TRUE(m_viewer.load_file(fixtures / "beat-keys/rms.beat-keys.json"));
    m_viewer.control().set_document(timeline::Document(120000));
    const std::string inspector = m_viewer.inspector_text();

    const bool loaded = m_viewer.load_file(fixtures / "extreme-normalized-vectors.json", true);

    EXPECT_FALSE(loaded);
    EXPECT_FALSE(m_viewer.diagnostics().empty());
    EXPECT_FALSE(m_viewer.control().document()->frame_grid());
    EXPECT_EQ(inspector, m_viewer.inspector_text());
    EXPECT_EQ(1, timeline::size_cast(m_viewer.mappings()));
}

TEST_F(QtViewerTest, beatKeysComparisonReusesDisplayedTimebaseAndFrameGrid)
{
    timeline_par_animator::JsonImportOptions options;
    options.ticks_per_second = 60000;
    options.frames_per_second_numerator = 24;
    timeline_par_animator::JsonImportResult base =
        timeline_par_animator::import_timeline_json(fixtures / "extreme-normalized-vectors.json", options);
    ASSERT_TRUE(base.succeeded());
    m_viewer.control().set_document(std::move(*base.document));

    const bool loaded = m_viewer.load_file(fixtures / "beat-keys/gold-write-rms-keyframes.json", true);

    ASSERT_TRUE(loaded);
    EXPECT_EQ(60000, m_viewer.control().document()->timebase().ticks_per_second());
    EXPECT_EQ(24, m_viewer.control().document()->frame_grid()->frames_per_second_numerator());
    EXPECT_GT(m_viewer.control().document()->lane_count(), 25);
}

TEST_F(QtViewerTest, animationComparisonReusesDisplayedFrameGrid)
{
    timeline_par_animator::JsonImportOptions options;
    options.ticks_per_second = 60000;
    options.frames_per_second_numerator = 24;
    timeline_par_animator::JsonImportResult base =
        timeline_par_animator::import_timeline_json(fixtures / "extreme-normalized-vectors.json", options);
    ASSERT_TRUE(base.succeeded());
    m_viewer.control().set_document(std::move(*base.document));

    const bool loaded = m_viewer.load_file(fixtures / "extreme-normalized-vectors.json", true);

    ASSERT_TRUE(loaded);
    EXPECT_EQ(24, m_viewer.control().document()->frame_grid()->frames_per_second_numerator());
}

TEST_F(QtViewerTest, presentsGenerationMetadata)
{
    const bool loaded = m_viewer.load_file(fixtures / "beat-keys/gold-write-rms-keyframes.json");
    const std::string text = m_viewer.inspector_text();

    ASSERT_TRUE(loaded);
    EXPECT_NE(std::string::npos, text.find("Schema: par-beatdown.beat-keys-overlay v1"));
    EXPECT_NE(std::string::npos, text.find("Generator: beat-keys 0.1.0"));
    EXPECT_NE(std::string::npos, text.find("Input base_animation:"));
    EXPECT_NE(std::string::npos, text.find("Target camera.zoom: 3"));
    EXPECT_NE(std::string::npos, text.find("Source music.rms: 9"));
    EXPECT_NE(std::string::npos, text.find("Frame extent: 0 to 4"));
}

TEST_F(QtViewerTest, presentsItemAttributes)
{
    const bool loaded = m_viewer.load_file(fixtures / "beat-keys/gold-write-rms-keyframes.json");
    const std::string text = m_viewer.inspector_text();

    ASSERT_TRUE(loaded);
    EXPECT_NE(std::string::npos, text.find("operation: replace"));
}

TEST_F(LoadedQtViewerTest, presentsParameterOutput)
{
    const std::string text = m_viewer.inspector_text();

    EXPECT_NE(std::string::npos, text.find("Parameter output:"));
    EXPECT_NE(std::string::npos, text.find("(exact)"));
    EXPECT_NE(std::string::npos, text.find("Ticks per second: 120000"));
}

TEST_F(QtViewerTest, presentsPalette)
{
    const bool loaded = m_viewer.load_file(fixtures / "color-map-gradient.json");
    const std::string text = m_viewer.inspector_text();

    ASSERT_TRUE(loaded);
    EXPECT_NE(std::string::npos, text.find("Palette:"));
    EXPECT_NE(std::string::npos, text.find("RGB"));
}

TEST_F(QtViewerTest, constructsNativeMenuCommands)
{
    QAction *add = m_viewer.findChild<QAction *>("add_comparison");
    QAction *export_action = m_viewer.findChild<QAction *>("export_snapshot");
    QAction *zoom_in = m_viewer.findChild<QAction *>("zoom_in");
    QAction *zoom_out = m_viewer.findChild<QAction *>("zoom_out");
    QAction *fit = m_viewer.findChild<QAction *>("fit_view");
    QAction *clear = m_viewer.findChild<QAction *>("clear_selection");

    const bool commands_exist = add && export_action && zoom_in && zoom_out && fit && clear;

    ASSERT_TRUE(commands_exist);
    EXPECT_FALSE(add->isEnabled());
    EXPECT_FALSE(export_action->isEnabled());
    EXPECT_FALSE(zoom_in->isEnabled());
    EXPECT_EQ(QKeySequence("Ctrl+Shift+O"), add->shortcut());
}

TEST_F(LoadedQtViewerTest, enablesDocumentCommandsAfterLoading)
{
    QAction *add = m_viewer.findChild<QAction *>("add_comparison");
    QAction *export_action = m_viewer.findChild<QAction *>("export_snapshot");
    ASSERT_NE(nullptr, add);
    ASSERT_NE(nullptr, export_action);

    EXPECT_TRUE(add->isEnabled());
    EXPECT_TRUE(export_action->isEnabled());
}

TEST_F(LoadedQtViewerTest, delegatesZoomInAction)
{
    QAction *zoom_in = m_viewer.findChild<QAction *>("zoom_in");
    ASSERT_NE(nullptr, zoom_in);
    const timeline::Ticks full =
        m_viewer.control().timeline_viewport()->end().ticks() - m_viewer.control().timeline_viewport()->start().ticks();

    zoom_in->trigger();

    EXPECT_LT(
        m_viewer.control().timeline_viewport()->end().ticks() - m_viewer.control().timeline_viewport()->start().ticks(),
        full);
}

TEST_F(LoadedQtViewerTest, delegatesZoomOutAction)
{
    QAction *zoom_in = m_viewer.findChild<QAction *>("zoom_in");
    QAction *zoom_out = m_viewer.findChild<QAction *>("zoom_out");
    ASSERT_NE(nullptr, zoom_in);
    ASSERT_NE(nullptr, zoom_out);
    const timeline::Ticks full =
        m_viewer.control().timeline_viewport()->end().ticks() - m_viewer.control().timeline_viewport()->start().ticks();
    zoom_in->trigger();

    zoom_out->trigger();

    EXPECT_EQ(full,
        m_viewer.control().timeline_viewport()->end().ticks() -
            m_viewer.control().timeline_viewport()->start().ticks());
}

TEST_F(LoadedQtViewerTest, delegatesFitViewAction)
{
    QAction *zoom_in = m_viewer.findChild<QAction *>("zoom_in");
    QAction *fit = m_viewer.findChild<QAction *>("fit_view");
    ASSERT_NE(nullptr, zoom_in);
    ASSERT_NE(nullptr, fit);
    const timeline::Ticks full =
        m_viewer.control().timeline_viewport()->end().ticks() - m_viewer.control().timeline_viewport()->start().ticks();
    zoom_in->trigger();

    fit->trigger();

    EXPECT_EQ(full,
        m_viewer.control().timeline_viewport()->end().ticks() -
            m_viewer.control().timeline_viewport()->start().ticks());
}

TEST_F(LoadedQtViewerTest, delegatesClearSelectionAction)
{
    QAction *clear = m_viewer.findChild<QAction *>("clear_selection");
    ASSERT_NE(nullptr, clear);
    QTest::keyClick(&m_viewer.control(), Qt::Key_Right, Qt::ShiftModifier);
    ASSERT_TRUE(m_viewer.control().interaction()->selected_frames());

    clear->trigger();

    EXPECT_FALSE(m_viewer.control().interaction()->selected_frames());
}

TEST_F(QtViewerTest, rejectsSnapshotExportWithoutDocument)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const std::filesystem::path path = std::filesystem::u8path(directory.path().toStdString()) / "timeline.txt";

    const bool exported = m_viewer.export_snapshot(path);

    EXPECT_FALSE(exported);
    EXPECT_FALSE(m_viewer.diagnostics().empty());
}

TEST_F(LoadedQtViewerTest, exportsCurrentCoreSnapshot)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const std::filesystem::path path = std::filesystem::u8path(directory.path().toStdString()) / "timeline.txt";
    const std::string before = m_viewer.control().snapshot();

    const bool exported = m_viewer.export_snapshot(path);

    ASSERT_TRUE(exported);
    std::ifstream input(path, std::ios::binary);
    const std::string actual((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    EXPECT_EQ(before, actual);
    EXPECT_TRUE(m_viewer.diagnostics().empty());
}

TEST_F(LoadedQtViewerTest, preservesDisplayedStateOnSnapshotWriteFailure)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const std::filesystem::path path =
        std::filesystem::u8path(directory.path().toStdString()) / "missing" / "snapshot.txt";
    QTest::keyClick(&m_viewer.control(), Qt::Key_Right, Qt::ShiftModifier);
    const std::string before = m_viewer.control().snapshot();
    const std::string inspector = m_viewer.inspector_text();

    const bool exported = m_viewer.export_snapshot(path);

    EXPECT_FALSE(exported);
    EXPECT_FALSE(m_viewer.diagnostics().empty());
    EXPECT_EQ(before, m_viewer.control().snapshot());
    EXPECT_EQ(inspector, m_viewer.inspector_text());
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
