// Copyright (c) 2026 Richard Thomson

#include <ResolvedAttributes.h>

#include <timelineParAnimator/TimelineJson.h>

#include <timeline/Layout.h>
#include <timeline/Query.h>
#include <timeline/size_cast.h>
#include <timeline/Snapshot.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <optional>
#include <ostream>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

using namespace timeline_par_animator;

namespace
{

/// One color-map fixture and its expected document shape.
///
struct FixtureCase
{
    std::string name;
    std::filesystem::path fixture;
    int lanes;
};

/// One basic color-map fixture and its golden-file stem.
///
struct ColorMapCase
{
    std::string name;
    std::filesystem::path fixture;
    std::string golden;
    int lanes;
};

/// One masked sparkle evaluator configuration.
///
struct RandomCase
{
    std::string name;
    int lane;
    unsigned int seed;
    int first;
    int last;
};

/// One independently diagnosed invalid animation.
///
struct DiagnosticCase
{
    std::string name;
    int index;
    std::string message;
};

const std::vector<FixtureCase> MASKED_FIXTURES{
    {"Effects", "fixtures/color-map-masked-effects.json", 4},
    {"Variants", "fixtures/color-map-masked-variants.json", 16},
};

const std::vector<RandomCase> RANDOM_CASES{
    {"PositiveSeed", 4, 1234, 2, 5},
    {"MaximumSeed", 6, 2147483647, 254, 255},
};

const std::vector<DiagnosticCase> MASKED_DIAGNOSTICS{
    {"RangesArray", 0, "ranges"},
    {"NonemptyRanges", 1, "nonempty"},
    {"RangeShape", 2, "range"},
    {"UnreadableMask", 3, "read"},
    {"FilenameString", 4, "string"},
    {"RangeStart", 5, "range"},
    {"RangeEnd", 6, "range"},
    {"AmountRange", 7, "range"},
    {"ColorShape", 8, "color"},
    {"ColorRange", 9, "range"},
    {"NegativeSeed", 10, "seed"},
    {"LargeSeed", 11, "seed"},
    {"FractionalSeed", 12, "seed"},
    {"SeedInteger", 13, "integer"},
    {"SignalRange", 14, "range"},
    {"KeyRange", 15, "range"},
    {"SecondEffect", 16, "effect 1"},
    {"UnknownField", 17, "field"},
    {"GeometricAmount", 18, "geometric"},
};

const std::vector<FixtureCase> INDEXED_FIXTURES{
    {"Effects", "fixtures/color-map-indexed-effects.json", 2},
    {"Variants", "fixtures/color-map-indexed-variants.json", 13},
};

const std::vector<DiagnosticCase> INDEXED_DIAGNOSTICS{
    {"FromRange", 0, "range"},
    {"ToRange", 1, "range"},
    {"StartRange", 2, "range"},
    {"EndRange", 3, "range"},
    {"IntegerOffset", 4, "integer"},
    {"UnknownField", 5, "field"},
    {"PaletteSize", 6, "256"},
    {"IndexRange", 7, "index"},
    {"SignalInteger", 8, "integer"},
    {"SignalField", 9, "field"},
    {"OffsetDefinition", 10, "offset"},
    {"GeometricOffset", 11, "geometric"},
    {"SignalRange", 12, "range"},
    {"SecondEffect", 13, "effect 1"},
};

const std::vector<FixtureCase> EFFECT_FIXTURES{
    {"Brightness", "fixtures/color-map-effects-brightness.json", 2},
    {"Adjustments", "fixtures/color-map-effects-adjustments.json", 5},
    {"Variants", "fixtures/color-map-effects-variants.json", 10},
    {"AmountHold", "fixtures/color-map-effects-amount-hold.json", 2},
    {"Order", "fixtures/color-map-effects-order.json", 6},
};

const std::vector<DiagnosticCase> EFFECT_DIAGNOSTICS{
    {"NonemptyEffects", 0, "nonempty"},
    {"EffectsArray", 1, "effects"},
    {"MissingAmount", 2, "amount"},
    {"TwoKeys", 3, "two"},
    {"IncreasingKeys", 4, "increasing"},
    {"FrameRange", 5, "frame"},
    {"GeometricAmount", 6, "geometric"},
    {"NumericAmount", 7, "numeric"},
    {"PositiveBrightness", 8, "positive"},
    {"UnknownCurve", 9, "curve"},
    {"UnknownField", 10, "field"},
    {"SourceDefinition", 11, "source"},
    {"SecondEffect", 12, "effect 1"},
    {"AmountRange", 13, "range"},
};

const std::vector<ColorMapCase> COLOR_MAP_FIXTURES{
    {"Gradient", "fixtures/color-map-gradient.json", "gradient", 1},
    {"File", "fixtures/color-map-file.json", "file", 1},
    {"Mixed", "fixtures/color-map-mixed.json", "mixed", 1},
    {"Names", "fixtures/color-map-names.json", "names", 1},
    {"Keyed", "fixtures/color-map-keyed.json", "keyed", 2},
    {"Hold", "fixtures/color-map-hold.json", "hold", 2},
    {"PartialKeys", "fixtures/color-map-partial-keys.json", "partial-keys", 2},
};

const std::vector<ColorMapCase> KEYED_COLOR_MAP_FIXTURES{
    {"Keyed", "fixtures/color-map-keyed.json", "keyed", 2},
    {"Hold", "fixtures/color-map-hold.json", "hold", 2},
    {"PartialKeys", "fixtures/color-map-partial-keys.json", "partial-keys", 2},
};

const std::vector<DiagnosticCase> COLOR_MAP_DIAGNOSTICS{
    {"AtFileDefinition", 0, "at-file"},
    {"EffectsDefinition", 1, "effects"},
    {"SourceDefinition", 2, "source"},
    {"Filename", 3, "filename"},
    {"UnreadableFile", 4, "read"},
    {"ShortPalette", 5, "256"},
    {"LongPalette", 6, "256"},
    {"RgbValue", 7, "RGB"},
    {"TwoStops", 8, "two stops"},
    {"IncreasingStops", 9, "increasing"},
    {"IndexValue", 10, "index"},
    {"IndexRange", 11, "range"},
    {"ColorRange", 12, "range"},
    {"ColorShape", 13, "color"},
    {"ThreeChannels", 14, "three"},
    {"GeometricCurve", 15, "geometric"},
    {"FrameRange", 16, "frame"},
    {"IncreasingKeys", 17, "increasing"},
    {"StringValue", 18, "string"},
    {"GradientShape", 19, "gradient"},
};

void PrintTo(const FixtureCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const ColorMapCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const RandomCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const DiagnosticCase &value, std::ostream *stream)
{
    *stream << value.name;
}

template <typename Case>
std::string case_name(const testing::TestParamInfo<Case> &info)
{
    return info.param.name;
}

JsonImportOptions source_options()
{
    JsonImportOptions result;
    result.frames_per_second_numerator = 30000;
    result.frames_per_second_denominator = 1001;
    return result;
}

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

timeline::Document import_clean_document(const std::filesystem::path &fixture)
{
    JsonImportResult result = import_timeline_json(fixture);
    if (!result.succeeded() || !result.diagnostics.empty())
    {
        throw std::runtime_error("color-map fixture import failed");
    }
    return std::move(*result.document);
}

timeline::Document import_source_document(const std::filesystem::path &fixture)
{
    JsonImportResult result = import_timeline_json(fixture, source_options());
    if (!result.succeeded() || !result.diagnostics.empty())
    {
        throw std::runtime_error("color-map source import failed");
    }
    return std::move(*result.document);
}

timeline::Document combine_with_music(const timeline::Document &document)
{
    const timeline::Document music = import_clean_document("fixtures/beat-keys/rms.beat-keys.json");
    return timeline::combine_documents(document, music);
}

std::vector<int> palette_lanes(const timeline::Document &document)
{
    std::vector<int> result;
    for (int index = 0; index < document.lane_count(); ++index)
    {
        if (document.strings().lookup(document.lanes()[index].kind()) == "palette")
        {
            result.push_back(index);
        }
    }
    return result;
}

std::vector<int> signal_lanes(const timeline::Document &document)
{
    std::vector<int> result;
    for (int index = 0; index < document.lane_count(); ++index)
    {
        if (document.strings().lookup(document.lanes()[index].kind()) != "palette")
        {
            result.push_back(index);
        }
    }
    return result;
}

const timeline::PaletteCurve &palette_curve(const timeline::Document &document, int lane)
{
    return std::get<timeline::PaletteCurve>(document.lanes()[lane].items()[0]);
}

const timeline::Keyframe &keyframe(const timeline::Document &document, int lane)
{
    return std::get<timeline::Keyframe>(document.lanes()[lane].items()[0]);
}

std::string output_prefix(const timeline::Document &document, const timeline::PaletteCurve &curve)
{
    const std::string_view output = resolved_attributes(document, curve.attributes()).at("output");
    return std::string(output.substr(0, output.find('%')));
}

std::string numbered_golden(std::string_view prefix, int frame)
{
    return std::string(prefix) + "000" + std::to_string(frame + 1) + ".map";
}

std::string basic_golden(const ColorMapCase &definition, int frame)
{
    if (definition.golden == "gradient")
    {
        return "gold-color-map-gradient.map";
    }
    return "gold-color-map-" + definition.golden + "-" + std::to_string(frame) + ".map";
}

std::optional<timeline::DisplayId> first_swatch_hit(const timeline::Layout &layout)
{
    for (const timeline::Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<timeline::Swatch>(primitive))
        {
            const timeline::Swatch &swatch = std::get<timeline::Swatch>(primitive);
            const std::optional<timeline::HitResult> hit = layout.hit_test({swatch.x, swatch.y}, 0);
            if (hit && hit->id.item_id == swatch.id.item_id)
            {
                return hit->id;
            }
        }
    }
    return std::nullopt;
}

std::optional<timeline::DisplayId> marker_hit(const timeline::Layout &layout, std::string_view lane_name)
{
    for (const timeline::Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<timeline::Marker>(primitive))
        {
            const timeline::Marker &marker = std::get<timeline::Marker>(primitive);
            if (layout.display_list().strings().lookup(marker.id.lane_id) == lane_name)
            {
                const std::optional<timeline::HitResult> hit = layout.hit_test({marker.x, marker.y}, 0);
                if (hit && hit->id.item_id == marker.id.item_id)
                {
                    return hit->id;
                }
            }
        }
    }
    return std::nullopt;
}

timeline::Palette sparkle_palette(const timeline::Document &document, const RandomCase &definition, timeline::Time time)
{
    const timeline::Palette input = golden_palette("input/indexed.map");
    const double amount = *document.lanes()[definition.lane + 1].evaluate_keyframes(time);
    const int rounded = static_cast<int>(std::lround(amount));
    std::mt19937 engine(static_cast<std::mt19937::result_type>(definition.seed));
    std::uniform_int_distribution<int> distribution(-rounded, rounded);
    timeline::Palette result = input;
    for (int index = definition.first; index <= definition.last; ++index)
    {
        const int red = std::clamp(input[index].red() + distribution(engine), 0, 255);
        const int green = std::clamp(input[index].green() + distribution(engine), 0, 255);
        const int blue = std::clamp(input[index].blue() + distribution(engine), 0, 255);
        result[index] = timeline::RgbColor(red, green, blue);
    }
    return result;
}

/// Exercises masked color-map fixtures independently.
///
class MaskedColorMapFixtureTest : public testing::TestWithParam<FixtureCase>
{
};

/// Exercises masked sparkle evaluator configurations independently.
///
class MaskedRandomTest : public testing::TestWithParam<RandomCase>
{
};

/// Exercises masked color-map diagnostics independently.
///
class MaskedDiagnosticTest : public testing::TestWithParam<DiagnosticCase>
{
};

/// Exercises indexed color-map fixtures independently.
///
class IndexedColorMapFixtureTest : public testing::TestWithParam<FixtureCase>
{
};

/// Exercises indexed color-map diagnostics independently.
///
class IndexedDiagnosticTest : public testing::TestWithParam<DiagnosticCase>
{
};

/// Exercises color-map effect fixtures independently.
///
class ColorMapEffectFixtureTest : public testing::TestWithParam<FixtureCase>
{
};

/// Exercises color-map effect diagnostics independently.
///
class ColorMapEffectDiagnosticTest : public testing::TestWithParam<DiagnosticCase>
{
};

/// Exercises basic color-map fixtures independently.
///
class ColorMapFixtureTest : public testing::TestWithParam<ColorMapCase>
{
};

/// Exercises keyed color-map metadata independently.
///
class KeyedColorMapFixtureTest : public testing::TestWithParam<ColorMapCase>
{
};

/// Exercises basic color-map diagnostics independently.
///
class ColorMapDiagnosticTest : public testing::TestWithParam<DiagnosticCase>
{
};

} // namespace

TEST_P(MaskedColorMapFixtureTest, importsExpectedDocumentShape)
{
    const FixtureCase &definition = GetParam();

    const timeline::Document document = import_source_document(definition.fixture);

    EXPECT_EQ(definition.lanes, document.lane_count());
    EXPECT_EQ(4004, document.frame_grid()->frame_duration().ticks());
}

TEST_P(MaskedColorMapFixtureTest, matchesSourceMaps)
{
    const FixtureCase &definition = GetParam();
    const timeline::Document document = import_source_document(definition.fixture);
    const timeline::FrameGrid &grid = *document.frame_grid();

    const std::vector<int> lanes = palette_lanes(document);

    ASSERT_FALSE(lanes.empty());
    for (int lane : lanes)
    {
        const timeline::PaletteCurve &curve = palette_curve(document, lane);
        const std::string prefix = output_prefix(document, curve);
        for (int frame = 0; frame < grid.frame_count(); ++frame)
        {
            const timeline::Palette expected = golden_palette(numbered_golden("gold-" + prefix, frame));
            const timeline::Palette actual = curve.sample(grid.frame_start(frame));
            for (int index = 0; index < 256; ++index)
            {
#ifndef _MSVC_STL_VERSION
                if ((prefix == "masked-sparkle-" && index >= 2 && index <= 5) ||
                    (prefix == "masked-sparkle-max-" && index >= 254))
                {
                    continue;
                }
#endif
                EXPECT_EQ(expected[index], actual[index]);
            }
        }
    }
}

TEST_P(MaskedColorMapFixtureTest, exposesSampledPalettesToInspection)
{
    const FixtureCase &definition = GetParam();
    const timeline::Document document = import_source_document(definition.fixture);
    const timeline::FrameGrid &grid = *document.frame_grid();

    const std::vector<int> lanes = palette_lanes(document);

    for (int lane : lanes)
    {
        const timeline::PaletteCurve &curve = palette_curve(document, lane);
        for (int frame = 0; frame < grid.frame_count(); ++frame)
        {
            const timeline::FrameInspection inspection = *timeline::inspect_frame(document, frame);
            ASSERT_TRUE(inspection.lanes[lane].items[0].palette);
            EXPECT_EQ(curve.sample(grid.frame_start(frame)), *inspection.lanes[lane].items[0].palette);
        }
    }
}

TEST_P(MaskedColorMapFixtureTest, clampsSamplingAtDocumentEnd)
{
    const FixtureCase &definition = GetParam();
    const timeline::Document document = import_source_document(definition.fixture);
    const timeline::FrameGrid &grid = *document.frame_grid();

    const std::vector<int> lanes = palette_lanes(document);

    for (int lane : lanes)
    {
        const timeline::PaletteCurve &curve = palette_curve(document, lane);
        EXPECT_EQ(curve.sample(grid.frame_start(grid.frame_count() - 1)), curve.sample(grid.end_time()));
    }
}

TEST_P(MaskedColorMapFixtureTest, preservesPaletteOutputDefinitions)
{
    const FixtureCase &definition = GetParam();
    const timeline::Document document = import_source_document(definition.fixture);

    const std::vector<int> lanes = palette_lanes(document);

    for (int lane : lanes)
    {
        const ResolvedAttributes attributes = resolved_attributes(document, palette_curve(document, lane).attributes());
        EXPECT_FALSE(attributes.at("output").empty());
    }
}

TEST_P(MaskedColorMapFixtureTest, preservesAmountDefinitions)
{
    const FixtureCase &definition = GetParam();
    const timeline::Document document = import_source_document(definition.fixture);

    const std::vector<int> lanes = signal_lanes(document);

    for (int lane : lanes)
    {
        const ResolvedAttributes attributes = resolved_attributes(document, keyframe(document, lane).attributes());
        EXPECT_EQ("amount", attributes.at("member"));
        EXPECT_NE(std::string::npos, attributes.at("signal").find("keys"));
        EXPECT_NE(std::string::npos, attributes.at("effect").find("kind"));
    }
}

TEST_P(MaskedColorMapFixtureTest, composesWithMusicDocument)
{
    const FixtureCase &definition = GetParam();
    const timeline::Document document = import_clean_document(definition.fixture);

    const timeline::Document combined = combine_with_music(document);

    EXPECT_EQ(definition.lanes + 4, combined.lane_count());
}

TEST_P(MaskedColorMapFixtureTest, rendersPaletteSwatches)
{
    const FixtureCase &definition = GetParam();
    const timeline::Document document = combine_with_music(import_clean_document(definition.fixture));
    const timeline::Layout layout(document,
        timeline::Viewport(650, 1000, document.frame_grid()->offset(), document.frame_grid()->end_time()),
        timeline::LayoutMetrics(240, 20, 40, 4));

    const std::string snapshot = timeline::render_snapshot(layout.display_list());

    EXPECT_NE(std::string::npos, snapshot.find("swatch PALETTE"));
}

TEST_P(MaskedColorMapFixtureTest, rendersAmountKeys)
{
    const FixtureCase &definition = GetParam();
    const timeline::Document document = combine_with_music(import_clean_document(definition.fixture));
    const timeline::Layout layout(document,
        timeline::Viewport(650, 1000, document.frame_grid()->offset(), document.frame_grid()->end_time()),
        timeline::LayoutMetrics(240, 20, 40, 4));

    const std::string snapshot = timeline::render_snapshot(layout.display_list());

    EXPECT_NE(std::string::npos, snapshot.find("amount-key-0"));
}

TEST_P(MaskedColorMapFixtureTest, hitTestsPaletteIdentity)
{
    const FixtureCase &definition = GetParam();
    const timeline::Document document = combine_with_music(import_clean_document(definition.fixture));
    const timeline::Layout layout(document,
        timeline::Viewport(650, 1000, document.frame_grid()->offset(), document.frame_grid()->end_time()),
        timeline::LayoutMetrics(240, 20, 40, 4));

    const std::optional<timeline::DisplayId> hit = first_swatch_hit(layout);

    EXPECT_TRUE(hit);
}

TEST_P(MaskedColorMapFixtureTest, hitTestsAmountIdentity)
{
    const FixtureCase &definition = GetParam();
    const timeline::Document document = combine_with_music(import_clean_document(definition.fixture));
    const timeline::Layout layout(document,
        timeline::Viewport(650, 1000, document.frame_grid()->offset(), document.frame_grid()->end_time()),
        timeline::LayoutMetrics(240, 20, 40, 4));
    std::optional<timeline::DisplayId> hit;
    for (const timeline::Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<timeline::Marker>(primitive))
        {
            const timeline::Marker &marker = std::get<timeline::Marker>(primitive);
            const std::string_view lane = layout.display_list().strings().lookup(marker.id.lane_id);
            if (lane.find("-amount") != std::string::npos)
            {
                const std::optional<timeline::HitResult> result = layout.hit_test({marker.x, marker.y}, 0);
                if (result && result->id.item_id == marker.id.item_id)
                {
                    hit = result->id;
                    break;
                }
            }
        }
    }

    EXPECT_TRUE(hit);
}

INSTANTIATE_TEST_SUITE_P(
    MaskedFixtures, MaskedColorMapFixtureTest, testing::ValuesIn(MASKED_FIXTURES), case_name<FixtureCase>);

TEST_P(MaskedRandomTest, drawsRgbInSourceOrder)
{
    const RandomCase &definition = GetParam();
    const timeline::Document document = import_clean_document("fixtures/color-map-masked-variants.json");
    const timeline::FrameGrid &grid = *document.frame_grid();
    const std::vector<timeline::Ticks> ticks{249, 250, 251, grid.frame_start(1).ticks(), grid.frame_start(4).ticks()};

    for (timeline::Ticks value : ticks)
    {
        const timeline::Time time = timeline::Time::from_ticks(value);
        EXPECT_EQ(sparkle_palette(document, definition, time), palette_curve(document, definition.lane).sample(time));
    }
}

TEST_P(MaskedRandomTest, resetsSourceEngineForEverySample)
{
    const RandomCase &definition = GetParam();
    const timeline::Document document = import_clean_document("fixtures/color-map-masked-variants.json");
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Time time = timeline::Time::from_ticks(250);
    const timeline::Palette expected = sparkle_palette(document, definition, time);
    const timeline::PaletteCurve &curve = palette_curve(document, definition.lane);

    static_cast<void>(curve.sample(grid.frame_start(2)));
    const timeline::Palette actual = curve.sample(time);

    EXPECT_EQ(expected, actual);
}

TEST_P(MaskedRandomTest, ownsEvaluatorAfterDocumentCopy)
{
    const RandomCase &definition = GetParam();
    const timeline::Document document = import_clean_document("fixtures/color-map-masked-variants.json");
    const timeline::Time time = timeline::Time::from_ticks(250);
    const timeline::Palette expected = sparkle_palette(document, definition, time);

    const timeline::Document copy = document;
    const timeline::Palette actual = palette_curve(copy, definition.lane).sample(time);

    EXPECT_EQ(expected, actual);
}

INSTANTIATE_TEST_SUITE_P(RandomEffects, MaskedRandomTest, testing::ValuesIn(RANDOM_CASES), case_name<RandomCase>);

TEST(MaskedColorMap, samplesOverlapAmountContinuously)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-masked-variants.json");
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Time half = grid.offset() + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2);

    const double amount = *document.lanes()[1].evaluate_keyframes(half);

    EXPECT_DOUBLE_EQ(0.125, amount);
}

TEST(MaskedColorMap, blendsOverlapOnce)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-masked-variants.json");
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Time half = grid.offset() + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2);
    const timeline::Palette input = golden_palette("input/indexed.map");
    const timeline::Palette mask = golden_palette("input/warm.map");
    const timeline::RgbColor &from = input[4];
    const timeline::RgbColor &to = mask[4];
    const timeline::RgbColor expected(static_cast<int>(std::lround(from.red() + 0.125 * (to.red() - from.red()))),
        static_cast<int>(std::lround(from.green() + 0.125 * (to.green() - from.green()))),
        static_cast<int>(std::lround(from.blue() + 0.125 * (to.blue() - from.blue()))));

    const timeline::RgbColor actual = palette_curve(document, 0).sample(half)[4];

    EXPECT_EQ(expected, actual);
}

TEST(MaskedColorMap, preservesColorsOutsideOverlappingRanges)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-masked-variants.json");
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Time half = grid.offset() + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2);
    const timeline::Palette input = golden_palette("input/indexed.map");

    const timeline::Palette actual = palette_curve(document, 0).sample(half);

    EXPECT_EQ(input[1], actual[1]);
    EXPECT_EQ(input[8], actual[8]);
}

TEST(MaskedColorMap, holdsPartialAmountAtKeyBoundaries)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-masked-variants.json");
    const timeline::FrameGrid &grid = *document.frame_grid();

    for (int frame = 0; frame < 5; ++frame)
    {
        EXPECT_DOUBLE_EQ(frame < 3 ? 0.25 : 0.75, *document.lanes()[9].evaluate_keyframes(grid.frame_start(frame)));
    }
}

TEST(MaskedColorMap, importsPartialAmountAsHold)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-masked-variants.json");

    const timeline::Keyframe &key = keyframe(document, 9);

    EXPECT_EQ(timeline::KeyframeInterpolation::HOLD, key.interpolation());
}

TEST(MaskedColorMap, recordsPartialAmountOutgoingStep)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-masked-variants.json");

    const ResolvedAttributes attributes = resolved_attributes(document, keyframe(document, 9).attributes());

    EXPECT_EQ("step", attributes.at("outgoing-curve"));
}

TEST(MaskedColorMap, preservesEffectOrder)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-masked-variants.json");
    const timeline::Time time = document.frame_grid()->frame_start(2);

    const timeline::RgbColor forward = palette_curve(document, 10).sample(time)[4];
    const timeline::RgbColor reverse = palette_curve(document, 13).sample(time)[4];

    EXPECT_NE(forward, reverse);
}

TEST(MaskedColorMap, reportsEveryPartialDiagnostic)
{
    const JsonImportResult result = import_timeline_json("fixtures/color-map-masked-partial.json");

    ASSERT_TRUE(result.succeeded());
    EXPECT_EQ(19, timeline::size_cast(result.diagnostics));
}

TEST(MaskedColorMap, retainsOnlyValidAnimationAfterFailures)
{
    const JsonImportResult result = import_timeline_json("fixtures/color-map-masked-partial.json");

    ASSERT_TRUE(result.document);
    EXPECT_EQ(4, result.document->lane_count());
    EXPECT_EQ("animation-19", result.document->strings().lookup(result.document->lanes()[0].id()));
}

TEST_P(MaskedDiagnosticTest, identifiesFailureClass)
{
    const DiagnosticCase &definition = GetParam();

    const JsonImportResult result = import_timeline_json("fixtures/color-map-masked-partial.json");

    ASSERT_GT(timeline::size_cast(result.diagnostics), definition.index);
    EXPECT_NE(std::string::npos,
        result.diagnostics[definition.index].find("animation-" + std::to_string(definition.index) + ":"));
    EXPECT_NE(std::string::npos, result.diagnostics[definition.index].find(definition.message));
}

INSTANTIATE_TEST_SUITE_P(
    MaskedFailures, MaskedDiagnosticTest, testing::ValuesIn(MASKED_DIAGNOSTICS), case_name<DiagnosticCase>);

TEST(MaskedColorMap, rejectsInvalidDocument)
{
    const JsonImportResult result = import_timeline_json("fixtures/color-map-masked-invalid.json");

    EXPECT_FALSE(result.succeeded());
    EXPECT_FALSE(result.diagnostics.empty());
}

TEST_P(IndexedColorMapFixtureTest, importsExpectedDocumentShape)
{
    const FixtureCase &definition = GetParam();

    const timeline::Document document = import_source_document(definition.fixture);

    EXPECT_EQ(definition.lanes, document.lane_count());
    EXPECT_EQ(4004, document.frame_grid()->frame_duration().ticks());
}

TEST_P(IndexedColorMapFixtureTest, matchesSourceMaps)
{
    const FixtureCase &definition = GetParam();
    const timeline::Document document = import_source_document(definition.fixture);
    const timeline::FrameGrid &grid = *document.frame_grid();

    const std::vector<int> lanes = palette_lanes(document);

    for (int lane : lanes)
    {
        const timeline::PaletteCurve &curve = palette_curve(document, lane);
        const std::string prefix = output_prefix(document, curve);
        for (int frame = 0; frame < grid.frame_count(); ++frame)
        {
            EXPECT_EQ(golden_palette(numbered_golden("gold-" + prefix, frame)), curve.sample(grid.frame_start(frame)));
        }
    }
}

TEST_P(IndexedColorMapFixtureTest, exposesSampledPalettesToInspection)
{
    const FixtureCase &definition = GetParam();
    const timeline::Document document = import_source_document(definition.fixture);
    const timeline::FrameGrid &grid = *document.frame_grid();

    const std::vector<int> lanes = palette_lanes(document);

    for (int lane : lanes)
    {
        for (int frame = 0; frame < grid.frame_count(); ++frame)
        {
            const timeline::Palette expected = palette_curve(document, lane).sample(grid.frame_start(frame));
            const timeline::FrameInspection inspection = *timeline::inspect_frame(document, frame);
            ASSERT_TRUE(inspection.lanes[lane].items[0].palette);
            EXPECT_EQ(expected, *inspection.lanes[lane].items[0].palette);
        }
    }
}

TEST_P(IndexedColorMapFixtureTest, clampsSamplingAtDocumentEnd)
{
    const FixtureCase &definition = GetParam();
    const timeline::Document document = import_source_document(definition.fixture);
    const timeline::FrameGrid &grid = *document.frame_grid();

    const std::vector<int> lanes = palette_lanes(document);

    for (int lane : lanes)
    {
        const timeline::PaletteCurve &curve = palette_curve(document, lane);
        EXPECT_EQ(curve.sample(grid.frame_start(4)), curve.sample(grid.end_time()));
    }
}

TEST_P(IndexedColorMapFixtureTest, preservesSignalDefinitions)
{
    const FixtureCase &definition = GetParam();
    const timeline::Document document = import_source_document(definition.fixture);

    const std::vector<int> lanes = signal_lanes(document);

    for (int lane : lanes)
    {
        const ResolvedAttributes attributes = resolved_attributes(document, keyframe(document, lane).attributes());
        const std::string_view member = attributes.at("member");
        EXPECT_TRUE(member == "offset" || member == "amount");
        EXPECT_NE(std::string::npos, attributes.at("signal").find("keys"));
        EXPECT_NE(std::string::npos, attributes.at("color-map").find("effects"));
    }
}

TEST_P(IndexedColorMapFixtureTest, preservesOffsetDefinitions)
{
    const FixtureCase &definition = GetParam();
    const timeline::Document document = import_source_document(definition.fixture);

    const std::vector<int> lanes = signal_lanes(document);
    int offsets = 0;
    for (int lane : lanes)
    {
        const ResolvedAttributes attributes = resolved_attributes(document, keyframe(document, lane).attributes());
        if (attributes.at("member") == "offset")
        {
            ++offsets;
            EXPECT_EQ("ping-pong", attributes.at("effect-kind"));
            EXPECT_NE(std::string::npos, document.strings().lookup(document.lanes()[lane].id()).find("-offset"));
        }
    }
    EXPECT_GT(offsets, 0);
}

INSTANTIATE_TEST_SUITE_P(
    IndexedFixtures, IndexedColorMapFixtureTest, testing::ValuesIn(INDEXED_FIXTURES), case_name<FixtureCase>);

TEST(IndexedColorMap, retainsUnroundedOffsetSignal)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-indexed-effects.json");
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Time half = grid.offset() + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2);

    const double value = *document.lanes()[1].evaluate_keyframes(half);

    EXPECT_DOUBLE_EQ(0.5, value);
}

TEST(IndexedColorMap, roundsOffsetsAwayFromZero)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-indexed-effects.json");
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Time half = grid.offset() + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2);
    const timeline::PaletteCurve &curve = palette_curve(document, 0);

    const timeline::RgbColor before = curve.sample(half + timeline::Duration::from_ticks(-1))[2];
    const timeline::RgbColor at = curve.sample(half)[2];

    EXPECT_EQ(timeline::RgbColor(5, 250, 5), before);
    EXPECT_EQ(timeline::RgbColor(2, 253, 2), at);
    EXPECT_EQ(timeline::RgbColor(1, 254, 1), curve.sample(half)[1]);
    EXPECT_EQ(timeline::RgbColor(6, 249, 6), curve.sample(half)[6]);
}

TEST(IndexedColorMap, holdsFractionalOffsetsAtKeyBoundaries)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-indexed-variants.json");
    const timeline::FrameGrid &grid = *document.frame_grid();

    for (int frame = 0; frame < 5; ++frame)
    {
        EXPECT_DOUBLE_EQ(frame < 3 ? -0.5 : 6.5, *document.lanes()[9].evaluate_keyframes(grid.frame_start(frame)));
    }
}

TEST(IndexedColorMap, appliesHeldRoundedOffset)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-indexed-variants.json");
    const timeline::FrameGrid &grid = *document.frame_grid();

    for (int frame = 0; frame < 5; ++frame)
    {
        EXPECT_EQ(timeline::RgbColor(5, 250, 5), palette_curve(document, 8).sample(grid.frame_start(frame))[2]);
    }
}

TEST(IndexedColorMap, importsOffsetAsHold)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-indexed-variants.json");

    EXPECT_EQ(timeline::KeyframeInterpolation::HOLD, keyframe(document, 9).interpolation());
}

TEST(IndexedColorMap, preservesAuthoredOffsetCurve)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-indexed-variants.json");

    const ResolvedAttributes attributes = resolved_attributes(document, keyframe(document, 9).attributes());

    EXPECT_EQ("geometric", attributes.at("curve"));
}

TEST(IndexedColorMap, recordsOffsetOutgoingStep)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-indexed-variants.json");

    const ResolvedAttributes attributes = resolved_attributes(document, keyframe(document, 9).attributes());

    EXPECT_EQ("step", attributes.at("outgoing-curve"));
}

TEST(IndexedColorMap, composesWithMusicDocument)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-indexed-effects.json");

    const timeline::Document combined = combine_with_music(document);

    EXPECT_EQ(6, combined.lane_count());
}

TEST(IndexedColorMap, rendersPaletteSwatch)
{
    const timeline::Document document =
        combine_with_music(import_clean_document("fixtures/color-map-indexed-effects.json"));
    const timeline::Layout layout(document,
        timeline::Viewport(600, 400, document.frame_grid()->offset(), document.frame_grid()->end_time()),
        timeline::LayoutMetrics(140, 20, 40, 4));

    const std::string snapshot = timeline::render_snapshot(layout.display_list());

    EXPECT_NE(std::string::npos, snapshot.find("swatch PALETTE"));
}

TEST(IndexedColorMap, rendersOffsetKey)
{
    const timeline::Document document =
        combine_with_music(import_clean_document("fixtures/color-map-indexed-effects.json"));
    const timeline::Layout layout(document,
        timeline::Viewport(600, 400, document.frame_grid()->offset(), document.frame_grid()->end_time()),
        timeline::LayoutMetrics(140, 20, 40, 4));

    const std::string snapshot = timeline::render_snapshot(layout.display_list());

    EXPECT_NE(std::string::npos, snapshot.find("animation-0-effect-1-offset-key-0"));
}

TEST(IndexedColorMap, hitTestsPaletteIdentity)
{
    const timeline::Document document =
        combine_with_music(import_clean_document("fixtures/color-map-indexed-effects.json"));
    const timeline::Layout layout(document,
        timeline::Viewport(600, 400, document.frame_grid()->offset(), document.frame_grid()->end_time()),
        timeline::LayoutMetrics(140, 20, 40, 4));

    const std::optional<timeline::DisplayId> hit = first_swatch_hit(layout);

    EXPECT_TRUE(hit);
}

TEST(IndexedColorMap, hitTestsOffsetIdentity)
{
    const timeline::Document document =
        combine_with_music(import_clean_document("fixtures/color-map-indexed-effects.json"));
    const timeline::Layout layout(document,
        timeline::Viewport(600, 400, document.frame_grid()->offset(), document.frame_grid()->end_time()),
        timeline::LayoutMetrics(140, 20, 40, 4));

    const std::optional<timeline::DisplayId> hit = marker_hit(layout, "animation-0-effect-1-offset");

    EXPECT_TRUE(hit);
}

TEST(IndexedColorMap, reportsEveryPartialDiagnostic)
{
    const JsonImportResult result = import_timeline_json("fixtures/color-map-indexed-partial.json");

    ASSERT_TRUE(result.succeeded());
    EXPECT_EQ(14, timeline::size_cast(result.diagnostics));
}

TEST(IndexedColorMap, retainsOnlyValidAnimationAfterFailures)
{
    const JsonImportResult result = import_timeline_json("fixtures/color-map-indexed-partial.json");

    ASSERT_TRUE(result.document);
    EXPECT_EQ(2, result.document->lane_count());
    EXPECT_EQ("animation-14", result.document->strings().lookup(result.document->lanes()[0].id()));
}

TEST_P(IndexedDiagnosticTest, identifiesFailureClass)
{
    const DiagnosticCase &definition = GetParam();

    const JsonImportResult result = import_timeline_json("fixtures/color-map-indexed-partial.json");

    ASSERT_GT(timeline::size_cast(result.diagnostics), definition.index);
    EXPECT_NE(std::string::npos,
        result.diagnostics[definition.index].find("animation-" + std::to_string(definition.index) + ":"));
    EXPECT_NE(std::string::npos, result.diagnostics[definition.index].find(definition.message));
}

INSTANTIATE_TEST_SUITE_P(
    IndexedFailures, IndexedDiagnosticTest, testing::ValuesIn(INDEXED_DIAGNOSTICS), case_name<DiagnosticCase>);

TEST(IndexedColorMap, rejectsInvalidDocument)
{
    const JsonImportResult result = import_timeline_json("fixtures/color-map-indexed-invalid.json");

    EXPECT_FALSE(result.succeeded());
    EXPECT_FALSE(result.diagnostics.empty());
}

TEST_P(ColorMapEffectFixtureTest, importsExpectedDocumentShape)
{
    const FixtureCase &definition = GetParam();

    const timeline::Document document = import_source_document(definition.fixture);

    EXPECT_EQ(definition.lanes, document.lane_count());
    EXPECT_EQ(4004, document.frame_grid()->frame_duration().ticks());
}

TEST_P(ColorMapEffectFixtureTest, matchesSourceMaps)
{
    const FixtureCase &definition = GetParam();
    const timeline::Document document = import_source_document(definition.fixture);
    const timeline::FrameGrid &grid = *document.frame_grid();

    const std::vector<int> lanes = palette_lanes(document);

    for (int lane : lanes)
    {
        const timeline::PaletteCurve &curve = palette_curve(document, lane);
        const std::string prefix = output_prefix(document, curve);
        for (int frame = 0; frame < grid.frame_count(); ++frame)
        {
            EXPECT_EQ(golden_palette(numbered_golden("gold-effects-" + prefix, frame)),
                curve.sample(grid.frame_start(frame)));
        }
    }
}

TEST_P(ColorMapEffectFixtureTest, preservesExpectedPaletteCount)
{
    const FixtureCase &definition = GetParam();
    const timeline::Document document = import_source_document(definition.fixture);
    const int expected = definition.name == "Variants" ? 5 : definition.name == "Order" ? 2 : 1;

    const std::vector<int> lanes = palette_lanes(document);

    EXPECT_EQ(expected, timeline::size_cast(lanes));
}

TEST_P(ColorMapEffectFixtureTest, clampsSamplingAtDocumentEnd)
{
    const FixtureCase &definition = GetParam();
    const timeline::Document document = import_source_document(definition.fixture);
    const timeline::FrameGrid &grid = *document.frame_grid();

    const std::vector<int> lanes = palette_lanes(document);

    for (int lane : lanes)
    {
        const timeline::PaletteCurve &curve = palette_curve(document, lane);
        EXPECT_EQ(curve.sample(grid.frame_start(grid.frame_count() - 1)), curve.sample(grid.end_time()));
    }
}

TEST_P(ColorMapEffectFixtureTest, preservesAmountDefinitions)
{
    const FixtureCase &definition = GetParam();
    const timeline::Document document = import_source_document(definition.fixture);

    const std::vector<int> lanes = signal_lanes(document);

    for (int lane : lanes)
    {
        ASSERT_EQ(2, document.lanes()[lane].item_count());
        const ResolvedAttributes attributes = resolved_attributes(document, keyframe(document, lane).attributes());
        EXPECT_NE(std::string::npos, attributes.at("color-map").find("effects"));
        EXPECT_NE(std::string::npos, attributes.at("signal").find("keys"));
        EXPECT_EQ("amount", attributes.at("member"));
        EXPECT_NE(std::string::npos, document.strings().lookup(document.lanes()[lane].id()).find("-effect-"));
    }
}

TEST_P(ColorMapEffectFixtureTest, exposesPaletteAndAmountInspection)
{
    const FixtureCase &definition = GetParam();
    const timeline::Document document = import_source_document(definition.fixture);

    const timeline::FrameInspection inspection = *timeline::inspect_frame(document, 1);

    ASSERT_TRUE(inspection.lanes[0].items[0].palette);
    EXPECT_FALSE(inspection.lanes[0].items[0].value);
    EXPECT_TRUE(inspection.lanes[1].value);
}

TEST_P(ColorMapEffectFixtureTest, composesWithMusicDocument)
{
    const FixtureCase &definition = GetParam();
    const timeline::Document document = import_clean_document(definition.fixture);

    const timeline::Document combined = combine_with_music(document);

    EXPECT_EQ(definition.lanes + 4, combined.lane_count());
}

TEST_P(ColorMapEffectFixtureTest, rendersPaletteSwatches)
{
    const FixtureCase &definition = GetParam();
    const timeline::Document document = combine_with_music(import_clean_document(definition.fixture));
    const timeline::Layout layout(document,
        timeline::Viewport(600, 600, document.frame_grid()->offset(), document.frame_grid()->end_time()),
        timeline::LayoutMetrics(140, 20, 40, 4));

    const std::string snapshot = timeline::render_snapshot(layout.display_list());

    EXPECT_NE(std::string::npos, snapshot.find("swatch PALETTE"));
}

TEST_P(ColorMapEffectFixtureTest, rendersAmountKeys)
{
    const FixtureCase &definition = GetParam();
    const timeline::Document document = combine_with_music(import_clean_document(definition.fixture));
    const timeline::Layout layout(document,
        timeline::Viewport(600, 600, document.frame_grid()->offset(), document.frame_grid()->end_time()),
        timeline::LayoutMetrics(140, 20, 40, 4));

    const std::string snapshot = timeline::render_snapshot(layout.display_list());

    EXPECT_NE(std::string::npos, snapshot.find("animation-0-effect-0-amount-key-0"));
}

INSTANTIATE_TEST_SUITE_P(
    EffectFixtures, ColorMapEffectFixtureTest, testing::ValuesIn(EFFECT_FIXTURES), case_name<FixtureCase>);

TEST(ColorMapEffects, samplesBrightnessContinuously)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-effects-brightness.json");
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Time half = grid.offset() + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2);

    const timeline::RgbColor color = palette_curve(document, 0).sample(half)[0];
    const double amount = *document.lanes()[1].evaluate_keyframes(half);

    EXPECT_EQ(timeline::RgbColor(125, 150, 250), color);
    EXPECT_DOUBLE_EQ(1.25, amount);
}

TEST(ColorMapEffects, samplesBrightnessAtDestinationKey)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-effects-brightness.json");

    const timeline::RgbColor color = palette_curve(document, 0).sample(document.frame_grid()->frame_start(1))[0];

    EXPECT_EQ(timeline::RgbColor(150, 180, 255), color);
}

TEST(ColorMapEffects, holdsAmountAtKeyBoundaries)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-effects-amount-hold.json");

    for (int frame = 0; frame < 5; ++frame)
    {
        EXPECT_DOUBLE_EQ(
            frame < 3 ? 0.5 : 2, *document.lanes()[1].evaluate_keyframes(document.frame_grid()->frame_start(frame)));
    }
}

TEST(ColorMapEffects, importsAmountAsHold)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-effects-amount-hold.json");

    EXPECT_EQ(timeline::KeyframeInterpolation::HOLD, keyframe(document, 1).interpolation());
}

TEST(ColorMapEffects, preservesAuthoredAmountCurve)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-effects-amount-hold.json");

    const ResolvedAttributes attributes = resolved_attributes(document, keyframe(document, 1).attributes());

    EXPECT_EQ("linear", attributes.at("curve"));
}

TEST(ColorMapEffects, recordsAmountOutgoingStep)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-effects-amount-hold.json");

    const ResolvedAttributes attributes = resolved_attributes(document, keyframe(document, 1).attributes());

    EXPECT_EQ("step", attributes.at("outgoing-curve"));
}

TEST(ColorMapEffects, ignoresGeometricDestinationCurve)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-effects-variants.json");

    EXPECT_EQ(timeline::KeyframeInterpolation::LINEAR, keyframe(document, 1).interpolation());
}

TEST(ColorMapEffects, preservesIgnoredDestinationCurveDefinition)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-effects-variants.json");

    const ResolvedAttributes attributes = resolved_attributes(document, keyframe(document, 1).attributes());

    EXPECT_EQ("geometric", attributes.at("curve"));
}

TEST(ColorMapEffects, preservesEffectOrder)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-effects-order.json");

    const timeline::RgbColor first = palette_curve(document, 0).sample(timeline::Time{})[64];
    const timeline::RgbColor second = palette_curve(document, 3).sample(timeline::Time{})[64];

    EXPECT_NE(first, second);
}

TEST(ColorMapEffects, hitTestsPaletteIdentity)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-effects-order.json");
    const timeline::Layout layout(document,
        timeline::Viewport(600, 300, timeline::Time{}, document.frame_grid()->end_time()),
        timeline::LayoutMetrics(140, 20, 40, 4));

    const std::optional<timeline::DisplayId> hit = first_swatch_hit(layout);

    EXPECT_TRUE(hit);
}

TEST(ColorMapEffects, hitTestsAmountIdentity)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-effects-order.json");
    const timeline::Layout layout(document,
        timeline::Viewport(600, 300, timeline::Time{}, document.frame_grid()->end_time()),
        timeline::LayoutMetrics(140, 20, 40, 4));

    const std::optional<timeline::DisplayId> hit = marker_hit(layout, "animation-0-effect-0-amount");

    EXPECT_TRUE(hit);
}

TEST(ColorMapEffects, reportsEveryPartialDiagnostic)
{
    const JsonImportResult result = import_timeline_json("fixtures/color-map-effects-partial.json");

    ASSERT_TRUE(result.succeeded());
    EXPECT_EQ(14, timeline::size_cast(result.diagnostics));
}

TEST(ColorMapEffects, retainsOnlyValidAnimationAfterFailures)
{
    const JsonImportResult result = import_timeline_json("fixtures/color-map-effects-partial.json");

    ASSERT_TRUE(result.document);
    EXPECT_EQ(2, result.document->lane_count());
    EXPECT_EQ("animation-14", result.document->strings().lookup(result.document->lanes()[0].id()));
}

TEST_P(ColorMapEffectDiagnosticTest, identifiesFailureClass)
{
    const DiagnosticCase &definition = GetParam();

    const JsonImportResult result = import_timeline_json("fixtures/color-map-effects-partial.json");

    ASSERT_GT(timeline::size_cast(result.diagnostics), definition.index);
    EXPECT_NE(std::string::npos,
        result.diagnostics[definition.index].find("animation-" + std::to_string(definition.index) + ":"));
    EXPECT_NE(std::string::npos, result.diagnostics[definition.index].find(definition.message));
}

INSTANTIATE_TEST_SUITE_P(
    EffectFailures, ColorMapEffectDiagnosticTest, testing::ValuesIn(EFFECT_DIAGNOSTICS), case_name<DiagnosticCase>);

TEST(ColorMapEffects, rejectsInvalidDocument)
{
    const JsonImportResult result = import_timeline_json("fixtures/color-map-effects-invalid.json");

    EXPECT_FALSE(result.succeeded());
    EXPECT_FALSE(result.diagnostics.empty());
}

TEST_P(ColorMapFixtureTest, importsExpectedDocumentShape)
{
    const ColorMapCase &definition = GetParam();

    const timeline::Document document = import_source_document(definition.fixture);

    EXPECT_EQ(definition.lanes, document.lane_count());
    EXPECT_EQ(4004, document.frame_grid()->frame_duration().ticks());
}

TEST_P(ColorMapFixtureTest, preservesPaletteIdentity)
{
    const ColorMapCase &definition = GetParam();
    const timeline::Document document = import_source_document(definition.fixture);

    const timeline::PaletteCurve &curve = palette_curve(document, 0);

    EXPECT_EQ("animation-0-palette", document.strings().lookup(curve.id()));
    EXPECT_EQ(256, curve.color_count());
}

TEST_P(ColorMapFixtureTest, preservesPaletteDefinition)
{
    const ColorMapCase &definition = GetParam();
    const timeline::Document document = import_source_document(definition.fixture);

    const ResolvedAttributes attributes = resolved_attributes(document, palette_curve(document, 0).attributes());

    EXPECT_EQ("colors", attributes.at("parameter"));
    EXPECT_NE(std::string::npos, attributes.at("color-map").find("at-file"));
}

TEST_P(ColorMapFixtureTest, matchesSourceMaps)
{
    const ColorMapCase &definition = GetParam();
    const timeline::Document document = import_source_document(definition.fixture);
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::PaletteCurve &curve = palette_curve(document, 0);

    for (int frame = 0; frame < grid.frame_count(); ++frame)
    {
        EXPECT_EQ(golden_palette(basic_golden(definition, frame)), curve.sample(grid.frame_start(frame)));
    }
}

TEST_P(ColorMapFixtureTest, exposesPaletteOnlyInspection)
{
    const ColorMapCase &definition = GetParam();
    const timeline::Document document = import_source_document(definition.fixture);
    const timeline::FrameGrid &grid = *document.frame_grid();

    for (int frame = 0; frame < grid.frame_count(); ++frame)
    {
        const timeline::FrameInspection inspection = *timeline::inspect_frame(document, frame);
        const timeline::InspectionItem &item = inspection.lanes[0].items[0];
        EXPECT_FALSE(item.value);
        ASSERT_TRUE(item.palette);
        EXPECT_EQ(golden_palette(basic_golden(definition, frame)), *item.palette);
    }
}

TEST_P(ColorMapFixtureTest, clampsSamplingAtDocumentEnd)
{
    const ColorMapCase &definition = GetParam();
    const timeline::Document document = import_source_document(definition.fixture);
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::PaletteCurve &curve = palette_curve(document, 0);

    const timeline::Palette last = curve.sample(grid.frame_start(grid.frame_count() - 1));

    EXPECT_EQ(last, curve.sample(grid.end_time()));
}

TEST_P(ColorMapFixtureTest, composesWithMusicDocument)
{
    const ColorMapCase &definition = GetParam();
    const timeline::Document document = import_clean_document(definition.fixture);

    const timeline::Document combined = combine_with_music(document);

    EXPECT_EQ(definition.lanes + 4, combined.lane_count());
}

TEST_P(ColorMapFixtureTest, rendersPaletteSwatch)
{
    const ColorMapCase &definition = GetParam();
    const timeline::Document document = combine_with_music(import_clean_document(definition.fixture));
    const timeline::Layout layout(document,
        timeline::Viewport(550, 240, document.frame_grid()->offset(), document.frame_grid()->end_time()),
        timeline::LayoutMetrics(110, 20, 40, 4));

    const std::string snapshot = timeline::render_snapshot(layout.display_list());

    EXPECT_NE(std::string::npos, snapshot.find("swatch PALETTE"));
}

INSTANTIATE_TEST_SUITE_P(
    ColorMapFixtures, ColorMapFixtureTest, testing::ValuesIn(COLOR_MAP_FIXTURES), case_name<ColorMapCase>);

TEST_P(KeyedColorMapFixtureTest, preservesKeyLaneIdentity)
{
    const ColorMapCase &definition = GetParam();
    const timeline::Document document = import_source_document(definition.fixture);

    EXPECT_EQ("animation-0-keys", document.strings().lookup(document.lanes()[1].id()));
}

TEST_P(KeyedColorMapFixtureTest, preservesKeyValue)
{
    const ColorMapCase &definition = GetParam();
    const timeline::Document document = import_source_document(definition.fixture);
    const timeline::Instant &key = std::get<timeline::Instant>(document.lanes()[1].items()[0]);

    const ResolvedAttributes attributes = resolved_attributes(document, key.attributes());

    EXPECT_EQ("input/warm.map", attributes.at("value"));
}

TEST_P(KeyedColorMapFixtureTest, preservesKeyDefinition)
{
    const ColorMapCase &definition = GetParam();
    const timeline::Document document = import_source_document(definition.fixture);
    const timeline::Instant &key = std::get<timeline::Instant>(document.lanes()[1].items()[0]);

    const ResolvedAttributes attributes = resolved_attributes(document, key.attributes());

    EXPECT_NE(std::string::npos, attributes.at("color-map").find("keys"));
}

INSTANTIATE_TEST_SUITE_P(KeyedColorMapFixtures, KeyedColorMapFixtureTest, testing::ValuesIn(KEYED_COLOR_MAP_FIXTURES),
    case_name<ColorMapCase>);

TEST(ColorMapImport, samplesContinuouslyBetweenKeys)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-keyed.json");
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Time half = grid.offset() + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2);

    const timeline::RgbColor color = palette_curve(document, 0).sample(half)[0];

    EXPECT_EQ(timeline::RgbColor(0, 64, 191), color);
}

TEST(ColorMapImport, samplesDestinationKeyExactly)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-keyed.json");

    const timeline::RgbColor color = palette_curve(document, 0).sample(document.frame_grid()->frame_start(1))[255];

    EXPECT_EQ(timeline::RgbColor(128, 0, 128), color);
}

TEST(ColorMapImport, holdsPaletteUntilBoundary)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-hold.json");

    const timeline::Palette actual = palette_curve(document, 0).sample(document.frame_grid()->frame_start(1));

    EXPECT_EQ(golden_palette("gold-color-map-keyed-0.map"), actual);
}

TEST(ColorMapImport, switchesHeldPaletteAtBoundary)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-hold.json");

    const timeline::Palette actual = palette_curve(document, 0).sample(document.frame_grid()->frame_start(2));

    EXPECT_EQ(golden_palette("gold-color-map-keyed-2.map"), actual);
}

TEST(ColorMapImport, clampsBeforeFirstPartialKey)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-partial-keys.json");
    const timeline::PaletteCurve &curve = palette_curve(document, 0);

    const timeline::Palette first = curve.sample(document.frame_grid()->frame_start(0));
    const timeline::Palette before = curve.sample(document.frame_grid()->frame_start(1));

    EXPECT_EQ(first, before);
}

TEST(ColorMapImport, clampsAfterLastPartialKey)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-partial-keys.json");
    const timeline::PaletteCurve &curve = palette_curve(document, 0);

    const timeline::Palette last = curve.sample(document.frame_grid()->frame_start(3));
    const timeline::Palette after = curve.sample(document.frame_grid()->frame_start(4));

    EXPECT_EQ(last, after);
}

TEST(ColorMapImport, importsLayerDocumentShape)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-layers.json");

    EXPECT_EQ(3, document.lane_count());
}

TEST(ColorMapImport, preservesLayerIdentities)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-layers.json");

    EXPECT_EQ("animation-layer-0-0", document.strings().lookup(document.lanes()[0].id()));
    EXPECT_EQ("animation-layer-1-0", document.strings().lookup(document.lanes()[2].id()));
}

TEST(ColorMapImport, preservesLayerDefinitions)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-layers.json");

    const ResolvedAttributes first = resolved_attributes(document, palette_curve(document, 0).attributes());
    const ResolvedAttributes second = resolved_attributes(document, palette_curve(document, 2).attributes());

    EXPECT_EQ("palette", first.at("layer"));
    EXPECT_EQ("constant", second.at("layer"));
}

TEST(ColorMapImport, evaluatesLayerDefinitionsIndependently)
{
    const timeline::Document document = import_clean_document("fixtures/color-map-layers.json");
    const timeline::Time time = document.frame_grid()->frame_start(1);

    const timeline::Palette first = palette_curve(document, 0).sample(time);
    const timeline::Palette second = palette_curve(document, 2).sample(time);

    EXPECT_NE(first, second);
}

TEST(ColorMapImport, reportsEveryPartialDiagnostic)
{
    const JsonImportResult result = import_timeline_json("fixtures/partial-color-map.json");

    ASSERT_TRUE(result.succeeded());
    EXPECT_EQ(20, timeline::size_cast(result.diagnostics));
}

TEST(ColorMapImport, retainsOnlyValidAnimationAfterFailures)
{
    const JsonImportResult result = import_timeline_json("fixtures/partial-color-map.json");

    ASSERT_TRUE(result.document);
    EXPECT_EQ(2, result.document->lane_count());
    EXPECT_EQ("animation-20", result.document->strings().lookup(result.document->lanes()[0].id()));
}

TEST_P(ColorMapDiagnosticTest, identifiesFailureClass)
{
    const DiagnosticCase &definition = GetParam();

    const JsonImportResult result = import_timeline_json("fixtures/partial-color-map.json");

    ASSERT_GT(timeline::size_cast(result.diagnostics), definition.index);
    EXPECT_NE(std::string::npos,
        result.diagnostics[definition.index].find("animation-" + std::to_string(definition.index) + ":"));
    EXPECT_NE(std::string::npos, result.diagnostics[definition.index].find(definition.message));
}

INSTANTIATE_TEST_SUITE_P(
    ColorMapFailures, ColorMapDiagnosticTest, testing::ValuesIn(COLOR_MAP_DIAGNOSTICS), case_name<DiagnosticCase>);

TEST(ColorMapImport, rejectsInvalidDocument)
{
    const JsonImportResult result = import_timeline_json("fixtures/invalid-color-map.json");

    EXPECT_FALSE(result.succeeded());
    EXPECT_FALSE(result.diagnostics.empty());
}
