// Copyright (c) 2026 Richard Thomson

#include <ResolvedAttributes.h>

#include <timelineParAnimator/TimelineJson.h>

#include <timeline/Layout.h>
#include <timeline/size_cast.h>
#include <timeline/Snapshot.h>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace timeline_par_animator;
using Json = nlohmann::json;

namespace
{

/// Mapping fixture and its expected materialized keyframes.
///
struct MappingCase
{
    std::string name;
    std::string mapping_file;
    std::string golden_file;
};

/// One recipe lane within a mapping fixture.
///
struct RealizationCase
{
    std::string name;
    std::string mapping_file;
    std::string golden_file;
    int recipe;
};

/// Expected fragment of a mapping's presentation text.
///
struct TextCase
{
    std::string name;
    std::string expected;
};

/// Import failure and the diagnostic that identifies its cause.
///
struct InvalidImportCase
{
    std::string name;
    std::string mapping_file;
    std::string expected_diagnostic;
};

/// Invalid recipe independently supplied to the mapping constructor.
///
struct InvalidRecipeCase
{
    std::string name;
    MappingRecipe recipe;
};

/// Invalid frame-addressed inputs independently supplied to a mapping.
///
struct InvalidInputCase
{
    std::string name;
    std::vector<MappingInput> inputs;
};

/// Expected value of one materialized overlapping pulse key.
///
struct PulseKeyCase
{
    std::string name;
    int item;
    double expected;
};

void PrintTo(const MappingCase &value, std::ostream *output)
{
    *output << value.name;
}

void PrintTo(const RealizationCase &value, std::ostream *output)
{
    *output << value.name;
}

void PrintTo(const TextCase &value, std::ostream *output)
{
    *output << value.name;
}

void PrintTo(const InvalidImportCase &value, std::ostream *output)
{
    *output << value.name;
}

void PrintTo(const InvalidRecipeCase &value, std::ostream *output)
{
    *output << value.name;
}

void PrintTo(const InvalidInputCase &value, std::ostream *output)
{
    *output << value.name;
}

void PrintTo(const PulseKeyCase &value, std::ostream *output)
{
    *output << value.name;
}

template <typename Case>
std::string case_name(const testing::TestParamInfo<Case> &information)
{
    return information.param.name;
}

std::filesystem::path fixture(std::string_view name)
{
    return std::filesystem::path("fixtures/beat-keys") / std::string(name);
}

Json load_json(const std::filesystem::path &path)
{
    std::ifstream input(path);
    if (!input)
    {
        throw std::runtime_error("Unable to open fixture: " + path.string());
    }
    Json result;
    input >> result;
    return result;
}

JsonImportResult import_clean_mapping(std::string_view name)
{
    JsonImportResult result = import_timeline_json(fixture(name));
    if (!result.succeeded() || !result.mapping || !result.diagnostics.empty())
    {
        throw std::runtime_error("Unable to import mapping fixture: " + std::string(name));
    }
    return result;
}

const timeline::Lane &generated_lane(const JsonImportResult &result, int recipe)
{
    const int source_lanes = result.mapping->source_document().lane_count();
    return result.document->lanes()[source_lanes + recipe];
}

std::map<timeline::Ticks, double> golden_keys(const std::filesystem::path &path, const std::string &target)
{
    const Json golden = load_json(path);
    std::map<timeline::Ticks, double> result;
    for (const Json &entry : golden.at("keyframes"))
    {
        if (entry.at("target").get<std::string>() == target)
        {
            result.emplace(entry.at("frame").get<timeline::Ticks>(), entry.at("value").get<double>());
        }
    }
    return result;
}

std::map<timeline::Ticks, double> lane_keys(const timeline::Lane &lane, const timeline::FrameGrid &grid)
{
    std::map<timeline::Ticks, double> result;
    for (const timeline::Item &item : lane.items())
    {
        const timeline::Keyframe &key = std::get<timeline::Keyframe>(item);
        result.emplace(*grid.frame_at_or_before(key.time()), key.value());
    }
    return result;
}

timeline::Document validation_source()
{
    return timeline::Document(timeline::FrameGrid(timeline::Timebase(120000), 2, 30, 1), 0, 0);
}

MappingRecipe valid_recipe()
{
    return MappingRecipe{"music.rms", "zoom", "replace"};
}

MappingOutput valid_output()
{
    return MappingOutput{"overlay", "music"};
}

BeatKeysMapping overlapping_note_mapping()
{
    const timeline::FrameGrid grid(timeline::Timebase(120000), 2, 10, 1);
    const timeline::Document source(grid, 0, 0);
    const MappingRecipe recipe{"music.note_pulse", "flash", "add", 2.0, 0.25, 0.2, std::nullopt};
    return BeatKeysMapping(
        source, {recipe}, {{"music.note_pulse", 0, 1.0}, {"music.note_pulse", 1, 1.0}}, valid_output(), "mapping.json");
}

std::string mapping_snapshot(const timeline::Document &document)
{
    const timeline::Viewport viewport(600, 240, document.content_start().value(), document.content_end().value());
    const timeline::Layout layout(document, viewport, timeline::LayoutMetrics(130, 24, 36, 4));
    return timeline::render_snapshot(layout.display_list());
}

/// Owns the mapping import selected by a parameterized fixture case.
///
class MappingFixtureTest : public testing::TestWithParam<MappingCase>
{
protected:
    void SetUp() override
    {
        m_result = import_clean_mapping(GetParam().mapping_file);
    }
    const MappingCase &definition() const
    {
        return GetParam();
    }
    const JsonImportResult &import_result() const
    {
        return m_result;
    }

private:
    JsonImportResult m_result;
};

/// Owns one imported recipe and its generated lane.
///
class MappingRealizationTest : public testing::TestWithParam<RealizationCase>
{
protected:
    void SetUp() override
    {
        m_result = import_clean_mapping(GetParam().mapping_file);
    }
    const RealizationCase &definition() const
    {
        return GetParam();
    }
    const JsonImportResult &import_result() const
    {
        return m_result;
    }
    const MappingRecipe &recipe() const
    {
        return m_result.mapping->recipes()[definition().recipe];
    }
    const timeline::Lane &lane() const
    {
        return generated_lane(m_result, definition().recipe);
    }

private:
    JsonImportResult m_result;
};

/// Owns the RMS mapping imported for fixed semantic and layout checks.
///
class RmsMappingTest : public testing::Test
{
protected:
    const BeatKeysMapping &mapping() const
    {
        return *m_result.mapping;
    }
    const timeline::Document &document() const
    {
        return *m_result.document;
    }
    const MappingRecipe &recipe() const
    {
        return mapping().recipes()[0];
    }

private:
    JsonImportResult m_result{import_clean_mapping("rms.beat-keys.json")};
};

/// Owns the RMS mapping used by one presentation-text parameter.
///
class MappingTextTest : public testing::TestWithParam<TextCase>
{
protected:
    void SetUp() override
    {
        m_result = import_clean_mapping("rms.beat-keys.json");
    }
    const TextCase &definition() const
    {
        return GetParam();
    }
    const BeatKeysMapping &mapping() const
    {
        return *m_result.mapping;
    }

private:
    JsonImportResult m_result;
};

/// Owns the failed mapping import selected by a parameterized fixture case.
///
class InvalidImportTest : public testing::TestWithParam<InvalidImportCase>
{
protected:
    void SetUp() override
    {
        m_result = import_timeline_json(fixture(GetParam().mapping_file));
    }
    const InvalidImportCase &definition() const
    {
        return GetParam();
    }
    const JsonImportResult &import_result() const
    {
        return m_result;
    }

private:
    JsonImportResult m_result;
};

/// Owns the failed fractional-frame mapping import.
///
class FractionalFrameMappingTest : public testing::Test
{
protected:
    const JsonImportResult &import_result() const
    {
        return m_result;
    }

private:
    JsonImportResult m_result{import_timeline_json(fixture("fractional-frame.beat-keys.json"))};
};

/// Exercises independently supplied invalid mapping recipes.
///
class InvalidRecipeTest : public testing::TestWithParam<InvalidRecipeCase>
{
};

/// Exercises independently supplied invalid frame-addressed inputs.
///
class InvalidInputTest : public testing::TestWithParam<InvalidInputCase>
{
};

/// Owns one materialized overlapping pulse mapping.
///
class OverlappingMappingTest : public testing::Test
{
protected:
    const timeline::Document &document() const
    {
        return m_document;
    }
    const timeline::Lane &lane() const
    {
        return m_document.lanes()[0];
    }

private:
    BeatKeysMapping m_mapping{overlapping_note_mapping()};
    timeline::Document m_document{m_mapping.materialize()};
};

/// Owns one materialized overlapping pulse mapping per expected key value.
///
class OverlappingDecayTest : public testing::TestWithParam<PulseKeyCase>
{
protected:
    void SetUp() override
    {
        m_mapping.emplace(overlapping_note_mapping());
        m_document.emplace(m_mapping->materialize());
    }
    const PulseKeyCase &definition() const
    {
        return GetParam();
    }
    const timeline::Lane &lane() const
    {
        return m_document->lanes()[0];
    }

private:
    std::optional<BeatKeysMapping> m_mapping;
    std::optional<timeline::Document> m_document;
};

/// Owns the offset mapping used for synchronization checks.
///
class SynchronizationMappingTest : public testing::Test
{
protected:
    const timeline::Document &document() const
    {
        return *m_result.document;
    }

private:
    JsonImportResult m_result{import_clean_mapping("offset-pulses.beat-keys.json")};
};

TEST_P(MappingFixtureTest, materializesOneLanePerRecipe)
{
    const int source_lanes = import_result().mapping->source_document().lane_count();

    const int generated_lanes = import_result().document->lane_count() - source_lanes;

    EXPECT_EQ(timeline::size_cast(import_result().mapping->recipes()), generated_lanes);
}

TEST_P(MappingFixtureTest, matchesGoldenKeyframeCount)
{
    const Json golden = load_json(fixture(definition().golden_file));

    const int expected = timeline::size_cast(golden.at("keyframes"));

    EXPECT_EQ(expected, import_result().document->keyframe_count());
}

TEST_P(MappingRealizationTest, usesKeyframeLaneKind)
{
    const std::string_view kind = import_result().document->strings().lookup(lane().kind());

    EXPECT_EQ("keyframes", kind);
}

TEST_P(MappingRealizationTest, usesRecipeTargetAsLabel)
{
    const std::string_view label = import_result().document->strings().lookup(lane().label());

    EXPECT_EQ(recipe().target, label);
}

TEST_P(MappingRealizationTest, matchesGoldenValues)
{
    const std::map<timeline::Ticks, double> expected = golden_keys(fixture(definition().golden_file), recipe().target);

    const std::map<timeline::Ticks, double> actual = lane_keys(lane(), *import_result().document->frame_grid());

    ASSERT_EQ(expected.size(), actual.size());
    for (const std::pair<const timeline::Ticks, double> &entry : expected)
    {
        const std::map<timeline::Ticks, double>::const_iterator found = actual.find(entry.first);
        ASSERT_NE(actual.end(), found);
        EXPECT_DOUBLE_EQ(entry.second, found->second);
    }
}

TEST_P(MappingRealizationTest, preservesOperationMetadata)
{
    std::vector<std::string_view> operations;
    for (const timeline::Item &item : lane().items())
    {
        const timeline::Keyframe &key = std::get<timeline::Keyframe>(item);
        operations.push_back(resolved_attributes(import_result(), key.attributes()).at("op"));
    }

    ASSERT_EQ(lane().item_count(), timeline::size_cast(operations));
    for (const std::string_view operation : operations)
    {
        EXPECT_EQ(recipe().operation, operation);
    }
}

TEST_P(MappingRealizationTest, preservesSourceMetadata)
{
    std::vector<std::string_view> sources;
    for (const timeline::Item &item : lane().items())
    {
        const timeline::Keyframe &key = std::get<timeline::Keyframe>(item);
        sources.push_back(resolved_attributes(import_result(), key.attributes()).at("source"));
    }

    ASSERT_EQ(lane().item_count(), timeline::size_cast(sources));
    for (const std::string_view source : sources)
    {
        EXPECT_EQ(recipe().source, source);
    }
}

TEST(BeatKeysMappingRowPulse, returnsUntransformedZero)
{
    const JsonImportResult result = import_clean_mapping("row-pulses.beat-keys.json");
    const timeline::Lane &lane = generated_lane(result, 0);

    const timeline::Keyframe &last = std::get<timeline::Keyframe>(lane.items().back());

    EXPECT_DOUBLE_EQ(0.0, last.value());
}

TEST(BeatKeysMappingNotePulse, extendsFrameGridForDecay)
{
    const JsonImportResult result = import_clean_mapping("note-pulses.beat-keys.json");

    const timeline::Ticks frame_count = result.document->frame_grid()->frame_count();

    EXPECT_EQ(7, frame_count);
}

TEST_F(RmsMappingTest, preservesScale)
{
    const double scale = recipe().scale;

    EXPECT_DOUBLE_EQ(2.0, scale);
}

TEST_F(RmsMappingTest, preservesClampUpperBound)
{
    const std::optional<std::pair<double, double>> &clamp = recipe().clamp;

    const double upper_bound = clamp.value_or(std::pair<double, double>{0.0, 0.0}).second;

    ASSERT_TRUE(clamp);
    EXPECT_DOUBLE_EQ(0.875, upper_bound);
}

TEST_F(RmsMappingTest, retainsSourceDocument)
{
    const int lane_count = mapping().source_document().lane_count();

    EXPECT_EQ(1, lane_count);
}

TEST_F(RmsMappingTest, rebuildsWithoutDisposableCacheLane)
{
    timeline::Document cache = mapping().materialize();
    const int expected = cache.lane_count();
    const timeline::Time start = cache.frame_grid()->offset();
    const timeline::Time end = cache.frame_grid()->end_time();
    timeline::DocumentBuilder builder(std::move(cache));
    builder.add_lane(timeline::Lane(
        builder.intern("cache-only"), builder.intern("Temporary"), builder.intern("events"), start, end));
    cache = std::move(builder).build();

    const timeline::Document rebuilt = mapping().materialize();

    EXPECT_EQ(expected, rebuilt.lane_count());
}

TEST_F(RmsMappingTest, rendersMappedTarget)
{
    const std::string snapshot = mapping_snapshot(document());

    const std::size_t position = snapshot.find("camera.zoom");

    EXPECT_NE(std::string::npos, position);
}

TEST_F(RmsMappingTest, rendersSourceLane)
{
    const std::string snapshot = mapping_snapshot(document());

    const std::size_t position = snapshot.find("RMS");

    EXPECT_NE(std::string::npos, position);
}

TEST_P(MappingTextTest, includesExpectedFragment)
{
    const std::string text = to_string(mapping());

    EXPECT_NE(std::string::npos, text.find(definition().expected));
}

TEST_P(InvalidImportTest, rejectsImport)
{
    const bool succeeded = import_result().succeeded();

    EXPECT_FALSE(succeeded);
}

TEST_P(InvalidImportTest, omitsMapping)
{
    const bool has_mapping = import_result().mapping.has_value();

    EXPECT_FALSE(has_mapping);
}

TEST_P(InvalidImportTest, reportsCause)
{
    const std::vector<std::string> &diagnostics = import_result().diagnostics;

    ASSERT_FALSE(diagnostics.empty());
    EXPECT_NE(std::string::npos, diagnostics.back().find(definition().expected_diagnostic));
}

TEST_F(FractionalFrameMappingTest, rejectsImport)
{
    const bool succeeded = import_result().succeeded();

    EXPECT_FALSE(succeeded);
}

TEST_F(FractionalFrameMappingTest, omitsMapping)
{
    const bool has_mapping = import_result().mapping.has_value();

    EXPECT_FALSE(has_mapping);
}

TEST_F(FractionalFrameMappingTest, reportsSourceFrame)
{
    const std::vector<std::string> &diagnostics = import_result().diagnostics;

    ASSERT_FALSE(diagnostics.empty());
    EXPECT_NE(std::string::npos, diagnostics.back().find("source frame"));
}

TEST_P(InvalidRecipeTest, rejectsRecipe)
{
    const timeline::Document source = validation_source();
    const MappingRecipe recipe = GetParam().recipe;

    const bool throws_invalid_argument = [&source, &recipe]()
    {
        try
        {
            const BeatKeysMapping mapping(source, {recipe}, {}, valid_output(), "mapping.json");
            (void) mapping;
        }
        catch (const std::invalid_argument &)
        {
            return true;
        }
        return false;
    }();

    EXPECT_TRUE(throws_invalid_argument);
}

TEST_P(InvalidInputTest, rejectsFrameAddressedInput)
{
    const timeline::Document source = validation_source();
    const std::vector<MappingInput> inputs = GetParam().inputs;

    const bool throws_invalid_argument = [&source, &inputs]()
    {
        try
        {
            const BeatKeysMapping mapping(source, {valid_recipe()}, inputs, valid_output(), "mapping.json");
            (void) mapping;
        }
        catch (const std::invalid_argument &)
        {
            return true;
        }
        return false;
    }();

    EXPECT_TRUE(throws_invalid_argument);
}

TEST_F(OverlappingMappingTest, materializesFourPulseKeys)
{
    ASSERT_EQ(1, document().lane_count());
    EXPECT_EQ(4, lane().item_count());
}

TEST_P(OverlappingDecayTest, materializesExpectedValue)
{
    const timeline::Keyframe &key = std::get<timeline::Keyframe>(lane().items()[definition().item]);

    EXPECT_DOUBLE_EQ(definition().expected, key.value());
}

TEST_F(SynchronizationMappingTest, appliesOffset)
{
    const timeline::Ticks offset = document().frame_grid()->offset().ticks();

    EXPECT_EQ(-18000, offset);
}

TEST_F(SynchronizationMappingTest, alignsSourceEventAndMappedKey)
{
    const timeline::Instant &event = std::get<timeline::Instant>(document().lanes()[0].items()[0]);
    const timeline::Keyframe &key = std::get<timeline::Keyframe>(document().lanes()[1].items()[0]);

    const bool aligned = event.time() == key.time();

    EXPECT_TRUE(aligned);
}

TEST_F(SynchronizationMappingTest, placesMappedKeyAtOffset)
{
    const timeline::Keyframe &key = std::get<timeline::Keyframe>(document().lanes()[1].items()[0]);

    const timeline::Ticks ticks = key.time().ticks();

    EXPECT_EQ(-18000, ticks);
}

INSTANTIATE_TEST_SUITE_P(Mappings, MappingFixtureTest,
    testing::Values(MappingCase{"Rms", "rms.beat-keys.json", "gold-write-rms-keyframes.json"},
        MappingCase{"Peak", "peak.beat-keys.json", "gold-write-peak-keyframes.json"},
        MappingCase{"Row", "row-pulses.beat-keys.json", "gold-write-row-pulses.json"},
        MappingCase{"NoteAndEffect", "note-pulses.beat-keys.json", "gold-write-note-pulses.json"}),
    case_name<MappingCase>);

INSTANTIATE_TEST_SUITE_P(Realizations, MappingRealizationTest,
    testing::Values(RealizationCase{"RmsZoom", "rms.beat-keys.json", "gold-write-rms-keyframes.json", 0},
        RealizationCase{"RmsBrightness", "rms.beat-keys.json", "gold-write-rms-keyframes.json", 1},
        RealizationCase{"RmsOpacity", "rms.beat-keys.json", "gold-write-rms-keyframes.json", 2},
        RealizationCase{"PeakShake", "peak.beat-keys.json", "gold-write-peak-keyframes.json", 0},
        RealizationCase{"RowFlash", "row-pulses.beat-keys.json", "gold-write-row-pulses.json", 0},
        RealizationCase{"NoteEnergy", "note-pulses.beat-keys.json", "gold-write-note-pulses.json", 0},
        RealizationCase{"EffectFlash", "note-pulses.beat-keys.json", "gold-write-note-pulses.json", 1}),
    case_name<RealizationCase>);

INSTANTIATE_TEST_SUITE_P(TextFragments, MappingTextTest,
    testing::Values(TextCase{"Source", "Music input:"}, TextCase{"Output", "Output: overlay / music"},
        TextCase{"RecipeCount", "Mapping recipes: 3"}, TextCase{"Recipe", "music.rms -> camera.zoom (replace)"},
        TextCase{"Clamp", "Clamp: 0 to 0.875"}),
    case_name<TextCase>);

INSTANTIATE_TEST_SUITE_P(ImportFailures, InvalidImportTest,
    testing::Values(InvalidImportCase{"UnknownBinding", "invalid-binding.beat-keys.json",
                        "unknown source reference: music.centroid"},
        InvalidImportCase{"MissingRelativeInput", "missing-input.beat-keys.json", "missing.music.json"}),
    case_name<InvalidImportCase>);

INSTANTIATE_TEST_SUITE_P(InvalidRecipes, InvalidRecipeTest,
    testing::Values(InvalidRecipeCase{"NegativeDecay", {"music.rms", "zoom", "replace", 1.0, 0.0, -1.0, std::nullopt}},
        InvalidRecipeCase{
            "ReversedClamp", {"music.rms", "zoom", "replace", 1.0, 0.0, 0.0, std::pair<double, double>{2.0, 1.0}}},
        InvalidRecipeCase{"InfiniteScale",
            {"music.rms", "zoom", "replace", std::numeric_limits<double>::infinity(), 0.0, 0.0, std::nullopt}}),
    case_name<InvalidRecipeCase>);

INSTANTIATE_TEST_SUITE_P(InvalidInputs, InvalidInputTest,
    testing::Values(InvalidInputCase{"NegativeFrame", {{"music.rms", -1, 0.5}}},
        InvalidInputCase{"DuplicateFeatureFrame", {{"music.rms", 0, 0.5}, {"music.rms", 0, 0.75}}}),
    case_name<InvalidInputCase>);

INSTANTIATE_TEST_SUITE_P(PulseKeys, OverlappingDecayTest,
    testing::Values(PulseKeyCase{"First", 0, 2.25}, PulseKeyCase{"Overlap", 1, 3.463061},
        PulseKeyCase{"Decay", 2, 1.463061}, PulseKeyCase{"Return", 3, 0.0}),
    case_name<PulseKeyCase>);

} // namespace
