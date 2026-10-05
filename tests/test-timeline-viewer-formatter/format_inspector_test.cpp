// Copyright (c) 2026 Richard Thomson

#include <timelineViewer/format_inspector.h>

#include <timelineParAnimator/TimelineJson.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace
{

const std::filesystem::path fixtures(TIMELINE_TEST_FIXTURE_DIR);

TEST(ViewerFormatter, assemblesComponentTextForEveryViewer)
{
    timeline_par_animator::JsonImportResult result =
        timeline_par_animator::import_timeline_json(fixtures / "extreme-normalized-vectors.json");
    ASSERT_TRUE(result.succeeded());
    std::optional<timeline::Interaction> interaction(std::in_place, *result.document);
    interaction->move_playhead_frame(1);
    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, 1);
    const std::optional<timeline::HitResult> hit;
    const std::vector<timeline_par_animator::BeatKeysMapping> mappings;

    const std::string text = timeline_viewer::format_inspector(result.document, hit, interaction, inspection, mappings);

    EXPECT_NE(std::string::npos, text.find("Frame: 1"));
    EXPECT_NE(std::string::npos, text.find("Time: "));
    EXPECT_NE(std::string::npos, text.find(" seconds"));
    EXPECT_NE(std::string::npos, text.find("Lanes: 25"));
    EXPECT_NE(std::string::npos, text.find("Source items: "));
    EXPECT_NE(std::string::npos, text.find("Parameter output: "));
}

} // namespace
