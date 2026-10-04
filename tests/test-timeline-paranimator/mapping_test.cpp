// Copyright (c) 2026 Richard Thomson

#include <timelineParAnimator/TimelineJson.h>

#include <timeline/Layout.h>
#include <timeline/Snapshot.h>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace timeline_par_animator;
using Json = nlohmann::json;

namespace
{

std::filesystem::path fixture(const std::string &name)
{
    return std::filesystem::path("fixtures/beat-keys") / name;
}

void expect_golden(const std::string &mapping_file, const std::string &golden_file)
{
    const JsonImportResult result = import_timeline_json(fixture(mapping_file));
    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.mapping);
    const timeline::Document &document = *result.document;
    const int source_lanes = result.mapping->source_document().lane_count();
    ASSERT_EQ(source_lanes + timeline::size_cast(result.mapping->recipes()), document.lane_count());
    std::ifstream input(fixture(golden_file));
    Json golden;
    input >> golden;
    int key_count = 0;
    for (int index = 0; index < timeline::size_cast(result.mapping->recipes()); ++index)
    {
        const MappingRecipe &recipe = result.mapping->recipes()[index];
        const timeline::Lane &lane = document.lanes()[source_lanes + index];
        EXPECT_EQ("keyframes", lane.kind());
        EXPECT_EQ(recipe.target, lane.label());
        for (const timeline::Item &item : lane.items())
        {
            const timeline::Keyframe &key = std::get<timeline::Keyframe>(item);
            const timeline::Ticks frame = *document.frame_grid()->frame_at_or_before(key.time());
            bool found = false;
            for (const Json &expected : golden.at("keyframes"))
            {
                if (expected.at("target").get<std::string>() == recipe.target &&
                    expected.at("frame").get<timeline::Ticks>() == frame)
                {
                    EXPECT_DOUBLE_EQ(expected.at("value").get<double>(), key.value());
                    EXPECT_EQ(expected.at("op").get<std::string>(), key.attributes().at("op"));
                    EXPECT_EQ(recipe.source, key.attributes().at("source"));
                    found = true;
                }
            }
            EXPECT_TRUE(found);
            ++key_count;
        }
    }
    EXPECT_EQ(timeline::size_cast(golden.at("keyframes")), key_count);
    EXPECT_EQ(key_count, document.keyframe_count());
}

} // namespace

TEST(BeatKeysMapping, matchesRealizedRmsOverlay)
{
    expect_golden("rms.beat-keys.json", "gold-write-rms-keyframes.json");
}

TEST(BeatKeysMapping, matchesRealizedPeakOverlay)
{
    expect_golden("peak.beat-keys.json", "gold-write-peak-keyframes.json");
}

TEST(BeatKeysMapping, matchesRealizedRowPulsesWithUntransformedZeroReturns)
{
    expect_golden("row-pulses.beat-keys.json", "gold-write-row-pulses.json");
}

TEST(BeatKeysMapping, matchesCountedNoteAndEffectDecayWithExtendedExtent)
{
    expect_golden("note-pulses.beat-keys.json", "gold-write-note-pulses.json");
    const JsonImportResult result = import_timeline_json(fixture("note-pulses.beat-keys.json"));
    ASSERT_TRUE(result.document);
    EXPECT_EQ(7, result.document->frame_grid()->frame_count());
}

TEST(BeatKeysMapping, retainsRecipesAndRebuildsDisposableDisplayData)
{
    const JsonImportResult result = import_timeline_json(fixture("rms.beat-keys.json"));
    ASSERT_TRUE(result.mapping);
    EXPECT_DOUBLE_EQ(2.0, result.mapping->recipes()[0].scale);
    ASSERT_TRUE(result.mapping->recipes()[0].clamp);
    EXPECT_DOUBLE_EQ(0.875, result.mapping->recipes()[0].clamp->second);
    timeline::Document cache = result.mapping->materialize();
    const int count = cache.lane_count();
    cache.add_lane(timeline::Lane(
        "cache-only", "Temporary", "events", cache.frame_grid()->offset(), cache.frame_grid()->end_time()));
    const timeline::Document rebuilt = result.mapping->materialize();
    EXPECT_EQ(count, rebuilt.lane_count());
    EXPECT_EQ(1, result.mapping->source_document().lane_count());
    const timeline::Viewport viewport(600, 240, rebuilt.content_start().value(), rebuilt.content_end().value());
    const timeline::Layout layout(rebuilt, viewport, timeline::LayoutMetrics(130, 24, 36, 4));
    EXPECT_NE(std::string::npos, timeline::render_snapshot(layout.display_list()).find("camera.zoom"));
    EXPECT_NE(std::string::npos, timeline::render_snapshot(layout.display_list()).find("RMS"));
}

TEST(BeatKeysMapping, rejectsUnknownBindingsAndMissingRelativeInputs)
{
    for (const std::string &name : {"invalid-binding.beat-keys.json", "missing-input.beat-keys.json"})
    {
        const JsonImportResult result = import_timeline_json(fixture(name));
        EXPECT_FALSE(result.succeeded());
        ASSERT_FALSE(result.diagnostics.empty());
        EXPECT_FALSE(result.mapping);
    }
    const JsonImportResult unknown = import_timeline_json(fixture("invalid-binding.beat-keys.json"));
    ASSERT_FALSE(unknown.diagnostics.empty());
    EXPECT_NE(std::string::npos, unknown.diagnostics.back().find("unknown source reference: music.centroid"));
    const JsonImportResult missing = import_timeline_json(fixture("missing-input.beat-keys.json"));
    ASSERT_FALSE(missing.diagnostics.empty());
    EXPECT_NE(std::string::npos, missing.diagnostics.back().find("missing.music.json"));
}

TEST(BeatKeysMapping, sumsOverlappingDecayBeforeTransforming)
{
    const timeline::FrameGrid grid(timeline::Timebase(120000), 2, 10, 1);
    const timeline::Document source(grid, 0, 0);
    const MappingRecipe recipe{"music.note_pulse", "flash", "add", 2.0, 0.25, 0.2, std::nullopt};
    const BeatKeysMapping mapping(source, {recipe}, {{"music.note_pulse", 0, 1.0}, {"music.note_pulse", 1, 1.0}},
        {"overlay", "music"}, "mapping.json");
    const timeline::Document document = mapping.materialize();
    ASSERT_EQ(1, document.lane_count());
    const timeline::Lane &lane = document.lanes()[0];
    ASSERT_EQ(4, lane.item_count());
    EXPECT_DOUBLE_EQ(2.25, std::get<timeline::Keyframe>(lane.items()[0]).value());
    EXPECT_DOUBLE_EQ(3.463061, std::get<timeline::Keyframe>(lane.items()[1]).value());
    EXPECT_DOUBLE_EQ(1.463061, std::get<timeline::Keyframe>(lane.items()[2]).value());
    EXPECT_DOUBLE_EQ(0.0, std::get<timeline::Keyframe>(lane.items()[3]).value());
}

TEST(BeatKeysMapping, rejectsFractionalSourceFramesWithoutTruncating)
{
    const JsonImportResult result = import_timeline_json(fixture("fractional-frame.beat-keys.json"));
    EXPECT_FALSE(result.succeeded());
    EXPECT_FALSE(result.mapping);
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_NE(std::string::npos, result.diagnostics.back().find("source frame"));
}

TEST(BeatKeysMapping, validatesRecipesAndFrameAddressedInputs)
{
    const timeline::Document source(timeline::FrameGrid(timeline::Timebase(120000), 2, 30, 1), 0, 0);
    MappingRecipe recipe{"music.rms", "zoom", "replace"};
    const MappingOutput output{"overlay", "music"};
    recipe.decay_seconds = -1.0;
    EXPECT_THROW(BeatKeysMapping(source, {recipe}, {}, output, "mapping.json"), std::invalid_argument);
    recipe.decay_seconds = 0.0;
    recipe.clamp = std::pair<double, double>{2.0, 1.0};
    EXPECT_THROW(BeatKeysMapping(source, {recipe}, {}, output, "mapping.json"), std::invalid_argument);
    recipe.clamp.reset();
    recipe.scale = std::numeric_limits<double>::infinity();
    EXPECT_THROW(BeatKeysMapping(source, {recipe}, {}, output, "mapping.json"), std::invalid_argument);
    recipe.scale = 1.0;
    EXPECT_THROW(
        BeatKeysMapping(source, {recipe}, {{"music.rms", -1, 0.5}}, output, "mapping.json"), std::invalid_argument);
    EXPECT_THROW(
        BeatKeysMapping(source, {recipe}, {{"music.rms", 0, 0.5}, {"music.rms", 0, 0.75}}, output, "mapping.json"),
        std::invalid_argument);
}

TEST(BeatKeysMapping, keepsSourceAndOutputsAlignedWithASynchronizationOffset)
{
    const JsonImportResult result = import_timeline_json(fixture("offset-pulses.beat-keys.json"));
    ASSERT_TRUE(result.document);
    ASSERT_TRUE(result.mapping);
    const timeline::Document &document = *result.document;
    EXPECT_EQ(-18000, document.frame_grid()->offset().ticks());
    const timeline::Instant &event = std::get<timeline::Instant>(document.lanes()[0].items()[0]);
    const timeline::Keyframe &key = std::get<timeline::Keyframe>(document.lanes()[1].items()[0]);
    EXPECT_EQ(event.time(), key.time());
    EXPECT_EQ(-18000, key.time().ticks());
}
