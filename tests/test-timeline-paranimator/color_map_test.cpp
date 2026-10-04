// Copyright (c) 2026 Richard Thomson

#include <timelineParAnimator/TimelineJson.h>

#include <timeline/Layout.h>
#include <timeline/Query.h>
#include <timeline/Snapshot.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <random>
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

TEST(MaskedColorMap, matchesSourceMapsWithOwnedDefinitionsAndWorkingComparison)
{
    for (const std::string &fixture : {"effects", "variants"})
    {
        SCOPED_TRACE(fixture);
        const timeline::Document document = [&fixture]
        {
            JsonImportOptions options;
            options.frames_per_second_numerator = 30000;
            options.frames_per_second_denominator = 1001;
            const JsonImportResult imported =
                import_timeline_json("fixtures/color-map-masked-" + fixture + ".json", options);
            if (!imported.succeeded() || !imported.diagnostics.empty())
            {
                throw std::runtime_error("Masked palette import failed");
            }
            return *imported.document;
        }();
        const timeline::FrameGrid &grid = *document.frame_grid();
        EXPECT_EQ(4004, grid.frame_duration().ticks());
        EXPECT_EQ(fixture == "effects" ? 4 : 16, document.lane_count());
        for (int lane_index = 0; lane_index < document.lane_count(); ++lane_index)
        {
            const timeline::Lane &lane = document.lanes()[lane_index];
            if (lane.kind() == "palette")
            {
                const timeline::PaletteCurve &curve = std::get<timeline::PaletteCurve>(lane.items()[0]);
                const std::string output = curve.attributes().at("output");
                const std::string prefix = output.substr(0, output.find('%'));
                for (int frame = 0; frame < grid.frame_count(); ++frame)
                {
                    const timeline::Palette expected =
                        golden_palette("gold-" + prefix + "000" + std::to_string(frame + 1) + ".map");
                    const timeline::Palette actual = curve.sample(grid.frame_start(frame));
                    for (int index = 0; index < 256; ++index)
                    {
#ifndef _MSVC_STL_VERSION
                        // Positive sparkle goldens use MSVC's distribution; verify other STLs below.
                        if ((prefix == "masked-sparkle-" && index >= 2 && index <= 5) ||
                            (prefix == "masked-sparkle-max-" && index >= 254))
                        {
                            continue;
                        }
#endif
                        EXPECT_EQ(expected[index], actual[index]);
                    }
                    const timeline::FrameInspection inspected = *timeline::inspect_frame(document, frame);
                    ASSERT_TRUE(inspected.lanes[lane_index].items[0].palette);
                    EXPECT_EQ(actual, *inspected.lanes[lane_index].items[0].palette);
                }
                EXPECT_EQ(curve.sample(grid.frame_start(grid.frame_count() - 1)), curve.sample(grid.end_time()));
            }
            else
            {
                const timeline::Keyframe &key = std::get<timeline::Keyframe>(lane.items()[0]);
                EXPECT_EQ("amount", key.attributes().at("member"));
                EXPECT_NE(std::string::npos, key.attributes().at("signal").find("keys"));
                EXPECT_NE(std::string::npos, key.attributes().at("effect").find("kind"));
            }
        }
        const JsonImportResult imported = import_timeline_json("fixtures/color-map-masked-" + fixture + ".json");
        const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
        ASSERT_TRUE(imported.succeeded());
        ASSERT_TRUE(music.succeeded());
        const timeline::Document combined = timeline::combine_documents(*imported.document, *music.document);
        EXPECT_EQ(document.lane_count() + 4, combined.lane_count());
        const timeline::Layout layout(combined,
            timeline::Viewport(650, 1000, combined.frame_grid()->offset(), combined.frame_grid()->end_time()),
            timeline::LayoutMetrics(240, 20, 40, 4));
        const std::string snapshot = timeline::render_snapshot(layout.display_list());
        EXPECT_NE(std::string::npos, snapshot.find("swatch PALETTE"));
        EXPECT_NE(std::string::npos, snapshot.find("amount-key-0"));
        bool palette_hit = false;
        bool signal_hit = false;
        for (const timeline::Primitive &primitive : layout.display_list().primitives())
        {
            if (std::holds_alternative<timeline::Swatch>(primitive))
            {
                const timeline::Swatch &swatch = std::get<timeline::Swatch>(primitive);
                const std::optional<timeline::HitResult> hit = layout.hit_test(timeline::Point{swatch.x, swatch.y}, 0);
                ASSERT_TRUE(hit);
                EXPECT_EQ(swatch.id.item_id, hit->id.item_id);
                palette_hit = true;
            }
            else if (std::holds_alternative<timeline::Marker>(primitive))
            {
                const timeline::Marker &marker = std::get<timeline::Marker>(primitive);
                if (marker.id.lane_id.find("-amount") != std::string::npos)
                {
                    const std::optional<timeline::HitResult> hit =
                        layout.hit_test(timeline::Point{marker.x, marker.y}, 0);
                    ASSERT_TRUE(hit);
                    EXPECT_EQ(marker.id.item_id, hit->id.item_id);
                    signal_hit = true;
                }
            }
        }
        EXPECT_TRUE(palette_hit);
        EXPECT_TRUE(signal_hit);
    }
}

TEST(MaskedColorMap, resetsTheSourceRandomEngineAndDrawsRgbInOrder)
{
    const JsonImportResult imported = import_timeline_json("fixtures/color-map-masked-variants.json");
    ASSERT_TRUE(imported.succeeded());
    const timeline::Document &document = *imported.document;
    const timeline::Palette input = golden_palette("input/indexed.map");
    const timeline::FrameGrid &grid = *document.frame_grid();
    for (int lane_index : {4, 6})
    {
        const timeline::PaletteCurve &curve = std::get<timeline::PaletteCurve>(document.lanes()[lane_index].items()[0]);
        for (timeline::Ticks ticks : {timeline::Ticks{249}, timeline::Ticks{250}, timeline::Ticks{251},
                 grid.frame_start(1).ticks(), grid.frame_start(4).ticks()})
        {
            const timeline::Time time = timeline::Time::from_ticks(ticks);
            const double amount = *document.lanes()[lane_index + 1].evaluate_keyframes(time);
            const int rounded = static_cast<int>(std::lround(amount));
            std::mt19937 engine(static_cast<std::mt19937::result_type>(lane_index == 4 ? 1234 : 2147483647));
            std::uniform_int_distribution<int> distribution(-rounded, rounded);
            timeline::Palette expected = input;
            const int first = lane_index == 4 ? 2 : 254;
            const int last = lane_index == 4 ? 5 : 255;
            for (int index = first; index <= last; ++index)
            {
                const int red = std::clamp(input[index].red() + distribution(engine), 0, 255);
                const int green = std::clamp(input[index].green() + distribution(engine), 0, 255);
                const int blue = std::clamp(input[index].blue() + distribution(engine), 0, 255);
                expected[index] = timeline::RgbColor(red, green, blue);
            }
            EXPECT_EQ(expected, curve.sample(time));
            static_cast<void>(curve.sample(grid.frame_start(2)));
            EXPECT_EQ(expected, curve.sample(time));
            const timeline::Document copy = document;
            EXPECT_EQ(expected, std::get<timeline::PaletteCurve>(copy.lanes()[lane_index].items()[0]).sample(time));
        }
    }
}

TEST(MaskedColorMap, blendsOverlapsOnceAndPreservesPartialHoldAndEffectOrder)
{
    const JsonImportResult imported = import_timeline_json("fixtures/color-map-masked-variants.json");
    ASSERT_TRUE(imported.succeeded());
    const timeline::Document &document = *imported.document;
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Palette input = golden_palette("input/indexed.map");
    const timeline::Palette mask = golden_palette("input/warm.map");
    const timeline::PaletteCurve &overlap = std::get<timeline::PaletteCurve>(document.lanes()[0].items()[0]);
    const timeline::Time half = grid.offset() + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2);
    EXPECT_DOUBLE_EQ(0.125, *document.lanes()[1].evaluate_keyframes(half));
    const timeline::RgbColor &from = input[4];
    const timeline::RgbColor &to = mask[4];
    EXPECT_EQ(timeline::RgbColor(static_cast<int>(std::lround(from.red() + 0.125 * (to.red() - from.red()))),
                  static_cast<int>(std::lround(from.green() + 0.125 * (to.green() - from.green()))),
                  static_cast<int>(std::lround(from.blue() + 0.125 * (to.blue() - from.blue())))),
        overlap.sample(half)[4]);
    EXPECT_EQ(input[1], overlap.sample(half)[1]);
    EXPECT_EQ(input[8], overlap.sample(half)[8]);
    const timeline::Lane &held = document.lanes()[9];
    for (int frame = 0; frame < 5; ++frame)
    {
        EXPECT_DOUBLE_EQ(frame < 3 ? 0.25 : 0.75, *held.evaluate_keyframes(grid.frame_start(frame)));
    }
    const timeline::Keyframe &key = std::get<timeline::Keyframe>(held.items()[0]);
    EXPECT_EQ(timeline::KeyframeInterpolation::HOLD, key.interpolation());
    EXPECT_EQ("step", key.attributes().at("outgoing-curve"));
    const timeline::PaletteCurve &forward = std::get<timeline::PaletteCurve>(document.lanes()[10].items()[0]);
    const timeline::PaletteCurve &reverse = std::get<timeline::PaletteCurve>(document.lanes()[13].items()[0]);
    EXPECT_NE(forward.sample(grid.frame_start(2))[4], reverse.sample(grid.frame_start(2))[4]);
}

TEST(MaskedColorMap, diagnosesInvalidMasksAmountsColorsAndSeedsTransactionally)
{
    const JsonImportResult imported = import_timeline_json("fixtures/color-map-masked-partial.json");
    ASSERT_TRUE(imported.succeeded());
    const std::array<std::string, 19> needles{"ranges", "nonempty", "range", "read", "string", "range", "range",
        "range", "color", "range", "seed", "seed", "seed", "integer", "range", "range", "effect 1", "field",
        "geometric"};
    ASSERT_EQ(needles.size(), imported.diagnostics.size());
    ASSERT_EQ(4, imported.document->lane_count());
    EXPECT_EQ("animation-19", imported.document->lanes()[0].id());
    for (int index = 0; index < timeline::size_cast(needles); ++index)
    {
        SCOPED_TRACE(index);
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find("animation-" + std::to_string(index) + ":"));
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find(needles[index]));
    }
    const JsonImportResult failed = import_timeline_json("fixtures/color-map-masked-invalid.json");
    EXPECT_FALSE(failed.succeeded());
    EXPECT_FALSE(failed.diagnostics.empty());
}

TEST(IndexedColorMap, matchesSourceMapsAndPreservesOwnedOffsetDefinitions)
{
    for (const std::string &fixture : {"effects", "variants"})
    {
        SCOPED_TRACE(fixture);
        const timeline::Document document = [&fixture]
        {
            JsonImportOptions options;
            options.frames_per_second_numerator = 30000;
            options.frames_per_second_denominator = 1001;
            const JsonImportResult imported =
                import_timeline_json("fixtures/color-map-indexed-" + fixture + ".json", options);
            if (!imported.succeeded() || !imported.diagnostics.empty())
            {
                throw std::runtime_error("Indexed palette import failed");
            }
            return *imported.document;
        }();
        EXPECT_EQ(fixture == "effects" ? 2 : 13, document.lane_count());
        const timeline::FrameGrid &grid = *document.frame_grid();
        EXPECT_EQ(4004, grid.frame_duration().ticks());
        for (int lane_index = 0; lane_index < document.lane_count(); ++lane_index)
        {
            const timeline::Lane &lane = document.lanes()[lane_index];
            if (lane.kind() == "palette")
            {
                const timeline::PaletteCurve &curve = std::get<timeline::PaletteCurve>(lane.items()[0]);
                const std::string output = curve.attributes().at("output");
                const std::string prefix = output.substr(0, output.find('%'));
                for (int frame = 0; frame < grid.frame_count(); ++frame)
                {
                    const timeline::Palette expected =
                        golden_palette("gold-" + prefix + "000" + std::to_string(frame + 1) + ".map");
                    EXPECT_EQ(expected, curve.sample(grid.frame_start(frame)));
                    const timeline::FrameInspection inspected = *timeline::inspect_frame(document, frame);
                    const std::optional<timeline::Palette> sampled = inspected.lanes[lane_index].items[0].palette;
                    ASSERT_TRUE(sampled);
                    EXPECT_EQ(expected, *sampled);
                }
                EXPECT_EQ(curve.sample(grid.frame_start(4)), curve.sample(grid.end_time()));
            }
            else
            {
                const timeline::Keyframe &key = std::get<timeline::Keyframe>(lane.items()[0]);
                const std::string member = key.attributes().at("member");
                EXPECT_TRUE(member == "offset" || member == "amount");
                EXPECT_NE(std::string::npos, key.attributes().at("signal").find("keys"));
                EXPECT_NE(std::string::npos, key.attributes().at("color-map").find("effects"));
                if (member == "offset")
                {
                    EXPECT_EQ("ping-pong", key.attributes().at("effect-kind"));
                    EXPECT_NE(std::string::npos, lane.id().find("-offset"));
                }
            }
        }
    }
}

TEST(IndexedColorMap, roundsOffsetsAwayFromZeroAndRetainsUnroundedSignals)
{
    const JsonImportResult imported = import_timeline_json("fixtures/color-map-indexed-effects.json");
    ASSERT_TRUE(imported.succeeded());
    const timeline::Document &document = *imported.document;
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Time half = grid.offset() + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2);
    const timeline::PaletteCurve &curve = std::get<timeline::PaletteCurve>(document.lanes()[0].items()[0]);
    EXPECT_DOUBLE_EQ(0.5, *document.lanes()[1].evaluate_keyframes(half));
    EXPECT_EQ(timeline::RgbColor(5, 250, 5), curve.sample(half + timeline::Duration::from_ticks(-1))[2]);
    EXPECT_EQ(timeline::RgbColor(2, 253, 2), curve.sample(half)[2]);
    EXPECT_EQ(timeline::RgbColor(1, 254, 1), curve.sample(half)[1]);
    EXPECT_EQ(timeline::RgbColor(6, 249, 6), curve.sample(half)[6]);

    const JsonImportResult variants = import_timeline_json("fixtures/color-map-indexed-variants.json");
    ASSERT_TRUE(variants.succeeded());
    const timeline::Lane &held = variants.document->lanes()[9];
    const timeline::PaletteCurve &held_palette =
        std::get<timeline::PaletteCurve>(variants.document->lanes()[8].items()[0]);
    for (int frame = 0; frame < 5; ++frame)
    {
        const timeline::Time time = variants.document->frame_grid()->frame_start(frame);
        EXPECT_DOUBLE_EQ(frame < 3 ? -0.5 : 6.5, *held.evaluate_keyframes(time));
        EXPECT_EQ(timeline::RgbColor(5, 250, 5), held_palette.sample(time)[2]);
    }
    const timeline::Keyframe &first = std::get<timeline::Keyframe>(held.items()[0]);
    EXPECT_EQ(timeline::KeyframeInterpolation::HOLD, first.interpolation());
    EXPECT_EQ("geometric", first.attributes().at("curve"));
    EXPECT_EQ("step", first.attributes().at("outgoing-curve"));
}

TEST(IndexedColorMap, rendersComparesAndHitsPaletteAndOffsetLanes)
{
    const JsonImportResult imported = import_timeline_json("fixtures/color-map-indexed-effects.json");
    const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
    ASSERT_TRUE(imported.succeeded());
    ASSERT_TRUE(music.succeeded());
    const timeline::Document combined = timeline::combine_documents(*imported.document, *music.document);
    ASSERT_EQ(6, combined.lane_count());
    const timeline::Layout layout(combined,
        timeline::Viewport(600, 400, combined.frame_grid()->offset(), combined.frame_grid()->end_time()),
        timeline::LayoutMetrics(140, 20, 40, 4));
    const std::string snapshot = timeline::render_snapshot(layout.display_list());
    EXPECT_NE(std::string::npos, snapshot.find("swatch PALETTE"));
    EXPECT_NE(std::string::npos, snapshot.find("animation-0-effect-1-offset-key-0"));
    bool palette_hit = false;
    bool offset_hit = false;
    for (const timeline::Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<timeline::Swatch>(primitive))
        {
            const timeline::Swatch &swatch = std::get<timeline::Swatch>(primitive);
            const std::optional<timeline::HitResult> hit = layout.hit_test(timeline::Point{swatch.x, swatch.y}, 0);
            ASSERT_TRUE(hit);
            EXPECT_EQ(swatch.id.item_id, hit->id.item_id);
            palette_hit = true;
        }
        else if (std::holds_alternative<timeline::Marker>(primitive))
        {
            const timeline::Marker &marker = std::get<timeline::Marker>(primitive);
            if (marker.id.lane_id == "animation-0-effect-1-offset")
            {
                const std::optional<timeline::HitResult> hit = layout.hit_test(timeline::Point{marker.x, marker.y}, 0);
                ASSERT_TRUE(hit);
                EXPECT_EQ(marker.id.item_id, hit->id.item_id);
                offset_hit = true;
            }
        }
    }
    EXPECT_TRUE(palette_hit);
    EXPECT_TRUE(offset_hit);
}

TEST(IndexedColorMap, rejectsInvalidEffectsTransactionallyWithIndexedDiagnostics)
{
    const JsonImportResult imported = import_timeline_json("fixtures/color-map-indexed-partial.json");
    ASSERT_TRUE(imported.succeeded());
    const std::array<std::string, 14> needles{"range", "range", "range", "range", "integer", "field", "256", "index",
        "integer", "field", "offset", "geometric", "range", "effect 1"};
    ASSERT_EQ(needles.size(), imported.diagnostics.size());
    ASSERT_EQ(2, imported.document->lane_count());
    EXPECT_EQ("animation-14", imported.document->lanes()[0].id());
    for (int index = 0; index < timeline::size_cast(needles); ++index)
    {
        SCOPED_TRACE(index);
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find("animation-" + std::to_string(index) + ":"));
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find(needles[index]));
    }
    const JsonImportResult failed = import_timeline_json("fixtures/color-map-indexed-invalid.json");
    EXPECT_FALSE(failed.succeeded());
    EXPECT_FALSE(failed.diagnostics.empty());
}

TEST(ColorMapEffects, matchesSourceMapsAndExposesOwnedAmountSignals)
{
    for (const std::string &fixture : {"brightness", "adjustments", "variants", "amount-hold", "order"})
    {
        SCOPED_TRACE(fixture);
        JsonImportOptions options;
        options.frames_per_second_numerator = 30000;
        options.frames_per_second_denominator = 1001;
        const JsonImportResult imported =
            import_timeline_json("fixtures/color-map-effects-" + fixture + ".json", options);
        ASSERT_TRUE(imported.succeeded());
        ASSERT_TRUE(imported.diagnostics.empty());
        const timeline::Document document = *imported.document;
        const timeline::FrameGrid &grid = *document.frame_grid();
        EXPECT_EQ(4004, grid.frame_duration().ticks());
        const int count = fixture == "adjustments" ? 5 : fixture == "variants" ? 10 : fixture == "order" ? 6 : 2;
        ASSERT_EQ(count, document.lane_count());
        int palettes = 0;
        for (const timeline::Lane &lane : document.lanes())
        {
            if (lane.kind() == "palette")
            {
                ++palettes;
                const timeline::PaletteCurve &curve = std::get<timeline::PaletteCurve>(lane.items()[0]);
                const std::string output = curve.attributes().at("output");
                const std::string prefix = output.substr(0, output.find('%'));
                for (int frame = 0; frame < grid.frame_count(); ++frame)
                {
                    const std::string number = "000" + std::to_string(frame + 1);
                    EXPECT_EQ(golden_palette("gold-effects-" + prefix + number + ".map"),
                        curve.sample(grid.frame_start(frame)));
                }
                EXPECT_EQ(curve.sample(grid.frame_start(grid.frame_count() - 1)), curve.sample(grid.end_time()));
            }
            else
            {
                ASSERT_EQ(2, lane.item_count());
                const timeline::Keyframe &key = std::get<timeline::Keyframe>(lane.items()[0]);
                EXPECT_NE(std::string::npos, key.attributes().at("color-map").find("effects"));
                EXPECT_NE(std::string::npos, key.attributes().at("signal").find("keys"));
                EXPECT_EQ("amount", key.attributes().at("member"));
                EXPECT_NE(std::string::npos, lane.id().find("-effect-"));
            }
        }
        EXPECT_EQ(fixture == "variants" ? 5 : fixture == "order" ? 2 : 1, palettes);
        const timeline::FrameInspection inspected = *timeline::inspect_frame(document, 1);
        ASSERT_TRUE(inspected.lanes[0].items[0].palette);
        EXPECT_FALSE(inspected.lanes[0].items[0].value);
        ASSERT_TRUE(inspected.lanes[1].value);
        const JsonImportResult animation = import_timeline_json("fixtures/color-map-effects-" + fixture + ".json");
        const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
        ASSERT_TRUE(animation.succeeded());
        ASSERT_TRUE(music.succeeded());
        const timeline::Document combined = timeline::combine_documents(*animation.document, *music.document);
        EXPECT_EQ(count + 4, combined.lane_count());
        const timeline::Layout layout(combined,
            timeline::Viewport(600, 600, combined.frame_grid()->offset(), combined.frame_grid()->end_time()),
            timeline::LayoutMetrics(140, 20, 40, 4));
        const std::string snapshot = timeline::render_snapshot(layout.display_list());
        EXPECT_NE(std::string::npos, snapshot.find("swatch PALETTE"));
        EXPECT_NE(std::string::npos, snapshot.find("animation-0-effect-0-amount-key-0"));
    }
}

TEST(ColorMapEffects, samplesNestedAmountsContinuouslyWithDestinationKeyInterpolation)
{
    const timeline::Document document = []
    {
        const JsonImportResult imported = import_timeline_json("fixtures/color-map-effects-brightness.json");
        if (!imported.succeeded())
        {
            throw std::runtime_error("brightness import failed");
        }
        return *imported.document;
    }();
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Time half = grid.offset() + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2);
    const timeline::PaletteCurve &curve = std::get<timeline::PaletteCurve>(document.lanes()[0].items()[0]);
    EXPECT_EQ(timeline::RgbColor(125, 150, 250), curve.sample(half)[0]);
    EXPECT_DOUBLE_EQ(1.25, *document.lanes()[1].evaluate_keyframes(half));
    EXPECT_EQ(timeline::RgbColor(150, 180, 255), curve.sample(grid.frame_start(1))[0]);
    const JsonImportResult held = import_timeline_json("fixtures/color-map-effects-amount-hold.json");
    ASSERT_TRUE(held.succeeded());
    const timeline::Lane &signal = held.document->lanes()[1];
    for (int frame = 0; frame < 5; ++frame)
    {
        EXPECT_DOUBLE_EQ(
            frame < 3 ? 0.5 : 2, *signal.evaluate_keyframes(held.document->frame_grid()->frame_start(frame)));
    }
    const timeline::Keyframe &first = std::get<timeline::Keyframe>(signal.items()[0]);
    EXPECT_EQ(timeline::KeyframeInterpolation::HOLD, first.interpolation());
    EXPECT_EQ("linear", first.attributes().at("curve"));
    EXPECT_EQ("step", first.attributes().at("outgoing-curve"));
    const JsonImportResult variants = import_timeline_json("fixtures/color-map-effects-variants.json");
    ASSERT_TRUE(variants.succeeded());
    const timeline::Keyframe &ignored = std::get<timeline::Keyframe>(variants.document->lanes()[1].items()[0]);
    EXPECT_EQ("geometric", ignored.attributes().at("curve"));
    EXPECT_EQ(timeline::KeyframeInterpolation::LINEAR, ignored.interpolation());
}

TEST(ColorMapEffects, preservesOrderAndDistinctPaletteAndSignalHits)
{
    const JsonImportResult imported = import_timeline_json("fixtures/color-map-effects-order.json");
    ASSERT_TRUE(imported.succeeded());
    const timeline::Document &document = *imported.document;
    ASSERT_EQ(6, document.lane_count());
    const timeline::PaletteCurve &first = std::get<timeline::PaletteCurve>(document.lanes()[0].items()[0]);
    const timeline::PaletteCurve &second = std::get<timeline::PaletteCurve>(document.lanes()[3].items()[0]);
    EXPECT_NE(first.sample(timeline::Time{})[64], second.sample(timeline::Time{})[64]);
    const timeline::Layout layout(document,
        timeline::Viewport(600, 300, timeline::Time{}, document.frame_grid()->end_time()),
        timeline::LayoutMetrics(140, 20, 40, 4));
    bool palette_hit = false;
    bool signal_hit = false;
    for (const timeline::Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<timeline::Swatch>(primitive))
        {
            const timeline::Swatch &swatch = std::get<timeline::Swatch>(primitive);
            const std::optional<timeline::HitResult> hit = layout.hit_test(timeline::Point{swatch.x, swatch.y}, 0);
            ASSERT_TRUE(hit);
            EXPECT_EQ(swatch.id.item_id, hit->id.item_id);
            palette_hit = true;
        }
        else if (std::holds_alternative<timeline::Marker>(primitive))
        {
            const timeline::Marker &marker = std::get<timeline::Marker>(primitive);
            if (marker.id.lane_id == "animation-0-effect-0-amount")
            {
                const std::optional<timeline::HitResult> hit = layout.hit_test(timeline::Point{marker.x, marker.y}, 0);
                ASSERT_TRUE(hit);
                EXPECT_EQ(marker.id.item_id, hit->id.item_id);
                signal_hit = true;
            }
        }
    }
    EXPECT_TRUE(palette_hit);
    EXPECT_TRUE(signal_hit);
}

TEST(ColorMapEffects, diagnosesInvalidEffectsWithoutPartialPaletteOrSignalLanes)
{
    const JsonImportResult imported = import_timeline_json("fixtures/color-map-effects-partial.json");
    ASSERT_TRUE(imported.succeeded());
    const std::array<std::string, 14> needles{"nonempty", "effects", "amount", "two", "increasing", "frame",
        "geometric", "numeric", "positive", "curve", "field", "source", "effect 1", "range"};
    ASSERT_EQ(needles.size(), imported.diagnostics.size());
    ASSERT_EQ(2, imported.document->lane_count());
    EXPECT_EQ("animation-14", imported.document->lanes()[0].id());
    for (int index = 0; index < timeline::size_cast(needles); ++index)
    {
        SCOPED_TRACE(index);
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find("animation-" + std::to_string(index) + ":"));
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find(needles[index]));
    }
    const JsonImportResult failed = import_timeline_json("fixtures/color-map-effects-invalid.json");
    EXPECT_FALSE(failed.succeeded());
    EXPECT_FALSE(failed.diagnostics.empty());
}

TEST(ColorMapImport, matchesSourceEvaluatorAndPreservesStructuredOwnedValues)
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

TEST(ColorMapImport, samplesContinuouslyAndRetainsBoundaryHoldSemantics)
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

TEST(ColorMapImport, preservesLayerIdentityAndIndependentDefinitions)
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

TEST(ColorMapImport, diagnosesInvalidDefinitionsWithoutPartialLanes)
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
