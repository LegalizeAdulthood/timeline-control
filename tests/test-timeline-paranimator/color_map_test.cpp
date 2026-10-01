// Copyright (c) 2026 Richard Thomson

#include <timeline/Layout.h>
#include <timeline/Query.h>
#include <timeline/Snapshot.h>
#include <timelineParAnimator/TimelineJson.h>

#include <gtest/gtest.h>

#include <array>
#include <fstream>
#include <stdexcept>

using namespace timeline_par_animator;

namespace
{

timeline::Palette golden_palette(const std::string &filename)
{
    std::ifstream input("fixtures/" + filename);
    if (!input)
    {
        throw std::runtime_error("Missing color-map golden: " + filename);
    }
    timeline::Palette palette;
    int red = 0;
    int green = 0;
    int blue = 0;
    while (input >> red >> green >> blue)
    {
        palette.emplace_back(red, green, blue);
    }
    if (timeline::size_cast(palette) != 256)
    {
        throw std::runtime_error("Invalid color-map golden");
    }
    return palette;
}

} // namespace

TEST(ColorMapImport, matches_source_evaluator_and_preserves_structured_owned_values)
{
    for (const std::string &fixture : {"gradient", "file", "mixed", "names", "keyed", "hold", "partial-keys"})
    {
        SCOPED_TRACE(fixture);
        const timeline::Document document = [&fixture]
        {
            JsonImportOptions options;
            options.frames_per_second_numerator = 30000;
            options.frames_per_second_denominator = 1001;
            const JsonImportResult imported = import_timeline_json("fixtures/color-map-" + fixture + ".json", options);
            if (!imported.succeeded() || !imported.diagnostics.empty())
            {
                throw std::runtime_error("Color-map import failed: " + imported.diagnostics.front());
            }
            return *imported.document;
        }();
        const bool keyed = fixture == "keyed" || fixture == "hold" || fixture == "partial-keys";
        ASSERT_EQ(keyed ? 2 : 1, document.lane_count());
        const timeline::FrameGrid &grid = *document.frame_grid();
        EXPECT_EQ(4004, grid.frame_duration().ticks());
        const timeline::PaletteCurve &curve = std::get<timeline::PaletteCurve>(document.lanes()[0].items().front());
        EXPECT_EQ("animation-0-palette", curve.id());
        EXPECT_EQ(256, curve.color_count());
        EXPECT_EQ("colors", curve.attributes().at("parameter"));
        EXPECT_NE(std::string::npos, curve.attributes().at("color-map").find("at-file"));
        for (int frame = 0; frame < grid.frame_count(); ++frame)
        {
            const std::string golden = fixture == "gradient"
                ? "gold-color-map-gradient.map"
                : "gold-color-map-" + fixture + "-" + std::to_string(frame) + ".map";
            const timeline::Palette expected = golden_palette(golden);
            EXPECT_EQ(expected, curve.sample(grid.frame_start(frame)));
            const timeline::FrameInspection inspected = *timeline::inspect_frame(document, frame);
            const timeline::InspectionItem &item = inspected.lanes[0].items[0];
            EXPECT_FALSE(item.value);
            ASSERT_TRUE(item.palette);
            EXPECT_EQ(expected, *item.palette);
        }
        const timeline::Palette last = curve.sample(grid.frame_start(grid.frame_count() - 1));
        EXPECT_EQ(last, curve.sample(grid.end_time()));
        const JsonImportResult animation = import_timeline_json("fixtures/color-map-" + fixture + ".json");
        const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
        ASSERT_TRUE(animation.succeeded());
        ASSERT_TRUE(music.succeeded());
        const timeline::Document combined = timeline::combine_documents(*animation.document, *music.document);
        EXPECT_EQ(document.lane_count() + 4, combined.lane_count());
        const timeline::Layout layout(combined, timeline::Viewport(550, 240, grid.offset(), grid.end_time()),
            timeline::LayoutMetrics(110, 20, 40, 4));
        EXPECT_NE(std::string::npos, timeline::render_snapshot(layout.display_list()).find("swatch PALETTE"));
        if (keyed)
        {
            EXPECT_EQ("animation-0-keys", document.lanes()[1].id());
            const timeline::Instant &key = std::get<timeline::Instant>(document.lanes()[1].items()[0]);
            EXPECT_EQ("input/warm.map", key.attributes().at("value"));
            EXPECT_NE(std::string::npos, key.attributes().at("color-map").find("keys"));
        }
    }
}

TEST(ColorMapImport, samples_continuously_and_retains_boundary_hold_semantics)
{
    const JsonImportResult linear = import_timeline_json("fixtures/color-map-keyed.json");
    ASSERT_TRUE(linear.succeeded());
    const timeline::FrameGrid &grid = *linear.document->frame_grid();
    const timeline::Time half = grid.offset() + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2);
    const timeline::PaletteCurve &curve = std::get<timeline::PaletteCurve>(linear.document->lanes()[0].items()[0]);
    EXPECT_EQ(timeline::RgbColor(0, 64, 191), curve.sample(half)[0]);
    EXPECT_EQ(timeline::RgbColor(128, 0, 128), curve.sample(grid.frame_start(1))[255]);
    const JsonImportResult held = import_timeline_json("fixtures/color-map-hold.json");
    ASSERT_TRUE(held.succeeded());
    const timeline::PaletteCurve &step = std::get<timeline::PaletteCurve>(held.document->lanes()[0].items()[0]);
    EXPECT_EQ(golden_palette("gold-color-map-keyed-0.map"), step.sample(grid.frame_start(1)));
    EXPECT_EQ(golden_palette("gold-color-map-keyed-2.map"), step.sample(grid.frame_start(2)));
    const JsonImportResult partial = import_timeline_json("fixtures/color-map-partial-keys.json");
    ASSERT_TRUE(partial.succeeded());
    const timeline::PaletteCurve &clamped = std::get<timeline::PaletteCurve>(partial.document->lanes()[0].items()[0]);
    EXPECT_EQ(clamped.sample(partial.document->frame_grid()->frame_start(0)),
        clamped.sample(partial.document->frame_grid()->frame_start(1)));
    EXPECT_EQ(clamped.sample(partial.document->frame_grid()->frame_start(3)),
        clamped.sample(partial.document->frame_grid()->frame_start(4)));
}

TEST(ColorMapImport, preserves_layer_identity_and_independent_definitions)
{
    const JsonImportResult imported = import_timeline_json("fixtures/color-map-layers.json");
    ASSERT_TRUE(imported.succeeded());
    ASSERT_TRUE(imported.diagnostics.empty());
    ASSERT_EQ(3, imported.document->lane_count());
    EXPECT_EQ("animation-layer-0-0", imported.document->lanes()[0].id());
    EXPECT_EQ("animation-layer-1-0", imported.document->lanes()[2].id());
    const timeline::PaletteCurve &first = std::get<timeline::PaletteCurve>(imported.document->lanes()[0].items()[0]);
    const timeline::PaletteCurve &second = std::get<timeline::PaletteCurve>(imported.document->lanes()[2].items()[0]);
    EXPECT_EQ("palette", first.attributes().at("layer"));
    EXPECT_EQ("constant", second.attributes().at("layer"));
    EXPECT_NE(first.sample(imported.document->frame_grid()->frame_start(1)),
        second.sample(imported.document->frame_grid()->frame_start(1)));
}

TEST(ColorMapImport, diagnoses_invalid_definitions_without_partial_lanes)
{
    const JsonImportResult imported = import_timeline_json("fixtures/partial-color-map.json");
    ASSERT_TRUE(imported.succeeded());
    const std::array<std::string, 20> diagnostics{"at-file", "effects", "source", "filename", "read", "256", "256",
        "RGB", "two stops", "increasing", "index", "range", "range", "color", "three", "geometric", "frame",
        "increasing", "string", "gradient"};
    ASSERT_EQ(diagnostics.size(), imported.diagnostics.size());
    ASSERT_EQ(2, imported.document->lane_count());
    EXPECT_EQ("animation-20", imported.document->lanes()[0].id());
    for (int index = 0; index < timeline::size_cast(diagnostics); ++index)
    {
        SCOPED_TRACE(index);
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find("animation-" + std::to_string(index) + ":"));
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find(diagnostics[index]));
    }
    const JsonImportResult failed = import_timeline_json("fixtures/invalid-color-map.json");
    EXPECT_FALSE(failed.succeeded());
    EXPECT_FALSE(failed.diagnostics.empty());
}
