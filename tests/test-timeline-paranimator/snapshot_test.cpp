// Copyright (c) 2026 Richard Thomson

#include <timeline/Layout.h>
#include <timeline/Snapshot.h>
#include <timelineParAnimator/TimelineJson.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using namespace timeline_par_animator;

namespace
{

void expect_snapshot(
    const std::filesystem::path &source, const std::filesystem::path &golden, const JsonImportOptions &options)
{
    const JsonImportResult imported = import_timeline_json(std::filesystem::path("fixtures") / source, options);
    ASSERT_TRUE(imported.succeeded());
    ASSERT_TRUE(imported.diagnostics.empty());
    ASSERT_TRUE(imported.document->frame_grid());
    const timeline::FrameGrid &grid = *imported.document->frame_grid();
    const timeline::Layout layout(*imported.document, timeline::Viewport(400, 100, grid.offset(), grid.end_time()),
        timeline::LayoutMetrics(100, 20, 30, 4));

    std::ifstream input(std::filesystem::path("fixtures/snapshots") / golden);
    ASSERT_TRUE(input);
    std::string expected;
    for (std::string line; std::getline(input, line);)
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        expected += line + '\n';
    }
    ASSERT_TRUE(input.eof());
    const std::string rendered = timeline::render_snapshot(layout.display_list());
    EXPECT_EQ(expected, rendered);
    EXPECT_EQ(rendered, timeline::render_snapshot(layout.display_list()));
}

} // namespace

TEST(ImportedSnapshot, rendersEmptyTimeline)
{
    expect_snapshot("empty-animation.json", "empty.txt", {});
}

TEST(ImportedSnapshot, rendersEventLane)
{
    JsonImportOptions options{};
    options.beat_keys_config_path = "fixtures/beat-keys/adapter.beat-keys.json";
    expect_snapshot("beat-keys/timeline-events.json", "events.txt", options);
}

TEST(ImportedSnapshot, rendersCurveLane)
{
    expect_snapshot("par-beatdown/gold-write-windowed-features.json", "curve.txt", {});
}

TEST(ImportedSnapshot, rendersKeyframeLane)
{
    expect_snapshot("beat-keys/gold-write-row-pulses.json", "keyframes.txt", {});
}
