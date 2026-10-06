// Copyright (c) 2026 Richard Thomson

#include <ResolvedAttributes.h>

#include <timelineParAnimator/TimelineJson.h>

#include <timeline/Layout.h>
#include <timeline/Query.h>
#include <timeline/size_cast.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

using namespace timeline_par_animator;

namespace
{

/// One non-clamping extrapolation policy.
///
struct PolicyCase
{
    std::string name;
    std::string policy;
};

/// One exact extrapolated value sampled at a rational frame position.
///
struct ValueCase
{
    std::string name;
    std::string policy;
    timeline::Ticks frame_numerator;
    timeline::Ticks frame_denominator;
    timeline::Ticks tick_adjustment;
    std::optional<double> expected;
};

/// One extrapolation policy sampled at an extreme timeline tick.
///
struct ExtremeCase
{
    std::string name;
    std::string policy;
    timeline::Ticks ticks;
};

/// One scalar type represented by the extrapolation variants fixture.
///
struct VariantCase
{
    std::string name;
    int lane;
    std::string parameter;
};

/// One independently diagnosed malformed extrapolation track.
///
struct DiagnosticCase
{
    std::string name;
    int index;
    std::string message;
};

const std::vector<PolicyCase> POLICY_CASES{
    {"Base", "base"},
    {"Omit", "omit"},
    {"Cycle", "cycle"},
    {"PingPong", "ping-pong"},
};

const std::vector<ValueCase> VALUE_CASES{
    {"CycleBeforeDistantPeriod", "cycle", -7, 1, 0, 200.0},
    {"CycleBeforeFirstPeriod", "cycle", -1, 1, 0, 200.0},
    {"CycleAtAuthoredMidpoint", "cycle", 2, 1, 0, 200.0},
    {"CycleAfterOnePeriod", "cycle", 5, 1, 0, 200.0},
    {"CycleAfterThreePeriods", "cycle", 11, 1, 0, 200.0},
    {"CycleAtRepeatedKey", "cycle", 7, 2, 0, 300.0},
    {"CycleWithinRepeatedRamp", "cycle", 9, 2, 0, 150.0},
    {"CycleWithinAuthoredRamp", "cycle", 3, 2, 0, 150.0},
    {"PingPongWithinReflectedRamp", "ping-pong", 7, 2, 0, 250.0},
    {"PingPongBeforeFirstPeriod", "ping-pong", -4, 1, 0, 200.0},
    {"BaseAtFirstKey", "base", 1, 1, 0, 100.0},
    {"BaseWithinAuthoredRamp", "base", 2, 1, 0, 200.0},
    {"BaseBeforeFirstKey", "base", 1, 1, -1, 678.0},
    {"BaseAfterLastKey", "base", 2, 1, 1, 678.0},
    {"OmitBeforeFirstKey", "omit", 1, 1, -1, std::nullopt},
    {"OmitAfterLastKey", "omit", 2, 1, 1, std::nullopt},
};

const std::vector<ExtremeCase> EXTREME_CASES{
    {"CycleMinimumTick", "cycle", std::numeric_limits<timeline::Ticks>::min()},
    {"PingPongMaximumTick", "ping-pong", std::numeric_limits<timeline::Ticks>::max()},
};

const std::vector<VariantCase> VARIANT_CASES{
    {"Integer", 0, "maxiter"},
    {"Double", 1, "bailout"},
    {"IntegerOrEnum", 2, "choice"},
};

const std::vector<DiagnosticCase> DIAGNOSTIC_CASES{
    {"IncreasingKeys", 0, "strictly increasing"},
    {"NumericScalar", 1, "numeric scalar"},
    {"TwoKeys", 2, "two keys"},
    {"SourceParameter", 3, "source parameter"},
    {"IntegralValue", 4, "integral"},
    {"JsonInteger", 5, "JSON integer"},
};

void PrintTo(const PolicyCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const ValueCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const ExtremeCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const VariantCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const DiagnosticCase &value, std::ostream *stream)
{
    *stream << value.name;
}

template <typename Case>
std::string case_name(const testing::TestParamInfo<Case> &information)
{
    return information.param.name;
}

std::filesystem::path policy_fixture(std::string_view policy)
{
    return std::filesystem::path("fixtures/maxiter-" + std::string(policy) + ".json");
}

std::filesystem::path policy_golden(std::string_view policy)
{
    return std::filesystem::path("fixtures/gold-maxiter-" + std::string(policy) + ".par");
}

std::string read_text(const std::filesystem::path &path)
{
    std::ifstream input(path);
    if (!input)
    {
        throw std::runtime_error("cannot read test fixture");
    }
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::string golden_entry(const std::string &golden, int frame)
{
    const std::string heading = "frame-000" + std::to_string(frame + 1) + " {";
    const std::string::size_type start = golden.find(heading);
    if (start == std::string::npos)
    {
        throw std::runtime_error("golden frame entry is missing");
    }
    const std::string::size_type end = golden.find('}', start);
    if (end == std::string::npos)
    {
        throw std::runtime_error("golden frame entry is incomplete");
    }
    return golden.substr(start, end - start);
}

std::optional<double> golden_value(const std::string &golden, int frame, std::string_view parameter)
{
    const std::string entry = golden_entry(golden, frame);
    const std::string assignment = std::string(parameter) + "=";
    const std::string::size_type start = entry.find(assignment);
    if (start == std::string::npos)
    {
        return std::nullopt;
    }
    return std::stod(entry.substr(start + assignment.size()));
}

JsonImportResult import_clean_result(const std::filesystem::path &path, const JsonImportOptions &options)
{
    JsonImportResult result = import_timeline_json(path, options);
    if (!result.succeeded() || !result.diagnostics.empty())
    {
        throw std::runtime_error("test fixture failed to import cleanly");
    }
    return result;
}

JsonImportResult import_clean_result(const std::filesystem::path &path)
{
    return import_clean_result(path, JsonImportOptions{});
}

timeline::Document import_clean_document(const std::filesystem::path &path, const JsonImportOptions &options)
{
    return *import_clean_result(path, options).document;
}

timeline::Document import_clean_document(const std::filesystem::path &path)
{
    return import_clean_document(path, JsonImportOptions{});
}

const std::string &diagnostic_at(const JsonImportResult &result, int index)
{
    if (index < 0 || timeline::size_cast(result.diagnostics) <= index)
    {
        throw std::out_of_range("test diagnostic index is out of range");
    }
    return result.diagnostics[index];
}

timeline::Document policy_document(std::string_view policy)
{
    return import_clean_document(policy_fixture(policy));
}

std::vector<std::optional<double>> lane_values(
    const timeline::Document &document, int lane_index, const timeline::FrameGrid &grid)
{
    std::vector<std::optional<double>> result;
    for (int frame = 0; frame < grid.frame_count(); ++frame)
    {
        result.push_back(document.lanes()[lane_index].evaluate_keyframes(grid.frame_start(frame)));
    }
    return result;
}

std::vector<std::optional<double>> inspection_values(
    const timeline::Document &document, int lane_index, const timeline::FrameGrid &grid)
{
    std::vector<std::optional<double>> result;
    for (int frame = 0; frame < grid.frame_count(); ++frame)
    {
        result.push_back(timeline::inspect_frame(document, frame)->lanes[lane_index].value);
    }
    return result;
}

std::vector<std::optional<double>> golden_values(
    const std::filesystem::path &path, std::string_view parameter, int frame_count)
{
    const std::string golden = read_text(path);
    std::vector<std::optional<double>> result;
    for (int frame = 0; frame < frame_count; ++frame)
    {
        result.push_back(golden_value(golden, frame, parameter));
    }
    return result;
}

const timeline::Keyframe &first_key(const timeline::Document &document, int lane_index)
{
    return std::get<timeline::Keyframe>(document.lanes()[lane_index].items()[0]);
}

timeline::Document combine_with_music(const timeline::Document &document)
{
    const timeline::Document music = import_clean_document("fixtures/beat-keys/rms.beat-keys.json");
    return timeline::combine_documents(document, music);
}

const timeline::Polyline &curve_line(const timeline::Layout &layout, timeline::StringId lane_id)
{
    for (const timeline::Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<timeline::Polyline>(primitive))
        {
            const timeline::Polyline &line = std::get<timeline::Polyline>(primitive);
            if (line.id.lane_id == lane_id)
            {
                return line;
            }
        }
    }
    throw std::runtime_error("extrapolation curve line is missing");
}

/// Owns the policy document selected by a parameterized fixture case.
///
template <typename Case>
class PolicyDocumentTest : public testing::TestWithParam<Case>
{
protected:
    void SetUp() override
    {
        m_document.emplace(policy_document(this->GetParam().policy));
    }
    const Case &definition() const
    {
        return this->GetParam();
    }
    const timeline::Document &document() const
    {
        return *m_document;
    }
    const timeline::FrameGrid &frame_grid() const
    {
        return *document().frame_grid();
    }

private:
    std::optional<timeline::Document> m_document;
};

/// Exercises behavior shared by each non-clamping extrapolation policy.
///
class ExtrapolationPolicyTest : public PolicyDocumentTest<PolicyCase>
{
};

/// Owns a policy document composed with the music document.
///
class ExtrapolationPolicyCompositionTest : public PolicyDocumentTest<PolicyCase>
{
protected:
    void SetUp() override
    {
        PolicyDocumentTest<PolicyCase>::SetUp();
        m_combined_document.emplace(combine_with_music(document()));
    }
    const timeline::Document &combined_document() const
    {
        return *m_combined_document;
    }

private:
    std::optional<timeline::Document> m_combined_document;
};

/// Exercises one extrapolation policy at one exact rational frame position.
///
class ExtrapolationValueTest : public PolicyDocumentTest<ValueCase>
{
};

/// Exercises overflow-safe extrapolation at timeline tick limits.
///
class ExtrapolationExtremeTest : public PolicyDocumentTest<ExtremeCase>
{
};

/// Owns the variant document used for parameterized source comparison.
///
class ExtrapolationVariantTest : public testing::TestWithParam<VariantCase>
{
protected:
    void SetUp() override
    {
        m_document.emplace(import_clean_document("fixtures/extrapolation-variants.json"));
    }
    const VariantCase &definition() const
    {
        return GetParam();
    }
    const timeline::Document &document() const
    {
        return *m_document;
    }
    const timeline::FrameGrid &frame_grid() const
    {
        return *document().frame_grid();
    }

private:
    std::optional<timeline::Document> m_document;
};

/// Owns the fixed extrapolation-variants document.
///
class ExtrapolationVariantsTest : public testing::Test
{
protected:
    const timeline::Document &document() const
    {
        return m_document;
    }
    const timeline::FrameGrid &frame_grid() const
    {
        return *m_document.frame_grid();
    }

private:
    timeline::Document m_document{import_clean_document("fixtures/extrapolation-variants.json")};
};

/// Owns one partial extrapolation import and its retained document.
///
class ExtrapolationPartialImportTest : public testing::Test
{
protected:
    const JsonImportResult &import_result() const
    {
        return m_result;
    }
    const timeline::Document &document() const
    {
        return *m_result.document;
    }

private:
    JsonImportResult m_result{import_timeline_json("fixtures/extrapolation-partial.json")};
};

/// Owns the partial import used for one parameterized diagnostic check.
///
class ExtrapolationDiagnosticTest : public testing::TestWithParam<DiagnosticCase>
{
protected:
    void SetUp() override
    {
        m_result = import_timeline_json("fixtures/extrapolation-partial.json");
    }
    const DiagnosticCase &definition() const
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

} // namespace

TEST_P(ExtrapolationPolicyTest, matchesSourceOutput)
{
    const PolicyCase &test_case = definition();
    JsonImportOptions options;
    options.frames_per_second_numerator = 30000;
    options.frames_per_second_denominator = 1001;
    const timeline::Document document = import_clean_document(policy_fixture(test_case.policy), options);
    const timeline::FrameGrid &grid = *document.frame_grid();

    const std::vector<std::optional<double>> actual = lane_values(document, 0, grid);
    const std::vector<std::optional<double>> expected =
        golden_values(policy_golden(test_case.policy), "maxiter", grid.frame_count());

    ASSERT_EQ(timeline::size_cast(expected), timeline::size_cast(actual));
    for (int frame = 0; frame < timeline::size_cast(expected); ++frame)
    {
        ASSERT_EQ(expected[frame].has_value(), actual[frame].has_value()) << frame;
        if (expected[frame])
        {
            EXPECT_DOUBLE_EQ(*expected[frame], *actual[frame]) << frame;
        }
    }
}

TEST_P(ExtrapolationPolicyTest, ownsPolicyMetadataAfterImportResultRelease)
{
    const PolicyCase &test_case = definition();
    JsonImportResult imported = import_clean_result(policy_fixture(test_case.policy));
    const timeline::Document document = *imported.document;

    imported.document.reset();
    const ResolvedAttributes attributes = resolved_attributes(document, first_key(document, 0).attributes());

    EXPECT_EQ(test_case.policy, attributes.at("extrapolate"));
}

TEST_P(ExtrapolationPolicyTest, ownsTrackDefinitionAfterImportResultRelease)
{
    const PolicyCase &test_case = definition();
    JsonImportResult imported = import_clean_result(policy_fixture(test_case.policy));
    const timeline::Document document = *imported.document;

    imported.document.reset();
    const ResolvedAttributes attributes = resolved_attributes(document, first_key(document, 0).attributes());

    EXPECT_NE(std::string_view::npos, attributes.at("track-definition").find("keys"));
}

TEST_P(ExtrapolationPolicyTest, placesFirstKeyAtSourceFrame)
{
    const PolicyCase &test_case = definition();
    JsonImportOptions options;
    options.frames_per_second_numerator = 30000;
    options.frames_per_second_denominator = 1001;

    const timeline::Document document = import_clean_document(policy_fixture(test_case.policy), options);
    const timeline::Time time = first_key(document, 0).time();

    EXPECT_EQ(4004, time.ticks());
}

TEST_P(ExtrapolationPolicyTest, matchesFrameInspection)
{
    const std::vector<std::optional<double>> evaluated = lane_values(document(), 0, frame_grid());
    const std::vector<std::optional<double>> inspected = inspection_values(document(), 0, frame_grid());

    EXPECT_EQ(evaluated, inspected);
}

TEST_P(ExtrapolationPolicyCompositionTest, combinesWithMusicDocument)
{
    EXPECT_EQ(5, combined_document().lane_count());
}

TEST_P(ExtrapolationPolicyCompositionTest, preservesInitialValueWhenCombined)
{
    const std::optional<double> expected = document().lanes()[0].evaluate_keyframes(timeline::Time{});

    const std::optional<double> actual = combined_document().lanes()[0].evaluate_keyframes(timeline::Time{});

    EXPECT_EQ(expected, actual);
}

TEST_P(ExtrapolationPolicyCompositionTest, rendersCurveWithinLaneBand)
{
    const timeline::Layout layout(combined_document(),
        timeline::Viewport(600, 300, frame_grid().offset(), frame_grid().end_time()),
        timeline::LayoutMetrics(100, 20, 40, 4));

    const timeline::Polyline &line = curve_line(layout, combined_document().lanes()[0].id());

    for (const timeline::Point &point : line.points)
    {
        EXPECT_GE(point.y, 24);
        EXPECT_LT(point.y, 56);
    }
}

TEST_P(ExtrapolationPolicyCompositionTest, hitTestsCurveIdentity)
{
    const timeline::Layout layout(combined_document(),
        timeline::Viewport(600, 300, frame_grid().offset(), frame_grid().end_time()),
        timeline::LayoutMetrics(100, 20, 40, 4));
    const timeline::Polyline &line = curve_line(layout, combined_document().lanes()[0].id());

    const std::optional<timeline::HitResult> result = layout.hit_test(line.points.front(), 0);

    ASSERT_TRUE(result);
    EXPECT_EQ(combined_document().lanes()[0].id(), result->id.lane_id);
}

INSTANTIATE_TEST_SUITE_P(Policies, ExtrapolationPolicyTest, testing::ValuesIn(POLICY_CASES), case_name<PolicyCase>);
INSTANTIATE_TEST_SUITE_P(
    Policies, ExtrapolationPolicyCompositionTest, testing::ValuesIn(POLICY_CASES), case_name<PolicyCase>);

TEST_P(ExtrapolationValueTest, evaluatesExpectedValue)
{
    const ValueCase &test_case = definition();
    const timeline::Ticks width = frame_grid().frame_duration().ticks();
    const timeline::Ticks ticks =
        width * test_case.frame_numerator / test_case.frame_denominator + test_case.tick_adjustment;

    const std::optional<double> actual = document().lanes()[0].evaluate_keyframes(timeline::Time::from_ticks(ticks));

    ASSERT_EQ(test_case.expected.has_value(), actual.has_value());
    if (test_case.expected)
    {
        EXPECT_DOUBLE_EQ(*test_case.expected, *actual);
    }
}

INSTANTIATE_TEST_SUITE_P(Values, ExtrapolationValueTest, testing::ValuesIn(VALUE_CASES), case_name<ValueCase>);

TEST_P(ExtrapolationExtremeTest, evaluatesWithoutTickOverflow)
{
    const ExtremeCase &test_case = definition();

    const std::optional<double> value =
        document().lanes()[0].evaluate_keyframes(timeline::Time::from_ticks(test_case.ticks));

    EXPECT_TRUE(value);
}

INSTANTIATE_TEST_SUITE_P(
    ExtremeTicks, ExtrapolationExtremeTest, testing::ValuesIn(EXTREME_CASES), case_name<ExtremeCase>);

TEST_P(ExtrapolationVariantTest, matchesSourceOutput)
{
    const VariantCase &test_case = definition();

    const std::vector<std::optional<double>> actual = lane_values(document(), test_case.lane, frame_grid());
    const std::vector<std::optional<double>> expected =
        golden_values("fixtures/gold-extrapolation-variants.par", test_case.parameter, frame_grid().frame_count());

    ASSERT_EQ(timeline::size_cast(expected), timeline::size_cast(actual));
    for (int frame = 0; frame < timeline::size_cast(expected); ++frame)
    {
        ASSERT_TRUE(expected[frame]) << frame;
        ASSERT_TRUE(actual[frame]) << frame;
        EXPECT_DOUBLE_EQ(*expected[frame], *actual[frame]) << frame;
    }
}

INSTANTIATE_TEST_SUITE_P(
    ScalarTypes, ExtrapolationVariantTest, testing::ValuesIn(VARIANT_CASES), case_name<VariantCase>);

TEST_F(ExtrapolationVariantsTest, importsEveryScalarType)
{
    EXPECT_EQ(3, document().lane_count());
}

TEST_F(ExtrapolationVariantsTest, preservesDoubleSourceValue)
{
    const ResolvedAttributes attributes = resolved_attributes(document(), first_key(document(), 1).attributes());

    EXPECT_EQ("1.25", attributes.at("source-value"));
}

TEST_F(ExtrapolationVariantsTest, preservesDoubleSourceEntry)
{
    const ResolvedAttributes attributes = resolved_attributes(document(), first_key(document(), 1).attributes());

    EXPECT_EQ("Bailout_Demo", attributes.at("source-entry"));
}

TEST_F(ExtrapolationVariantsTest, preservesDoubleCatalogDefinition)
{
    const ResolvedAttributes attributes = resolved_attributes(document(), first_key(document(), 1).attributes());

    EXPECT_NE(std::string_view::npos, attributes.at("catalog-definition").find("double"));
}

TEST_F(ExtrapolationVariantsTest, interpolatesDoubleBetweenKeys)
{
    const timeline::Ticks width = frame_grid().frame_duration().ticks();
    const timeline::Time half = timeline::Time::from_ticks(width * 3 / 2);

    const std::optional<double> value = document().lanes()[1].evaluate_keyframes(half);

    ASSERT_TRUE(value);
    EXPECT_DOUBLE_EQ(2, *value);
}

TEST_F(ExtrapolationVariantsTest, holdsIntegerBetweenKeys)
{
    const timeline::Ticks width = frame_grid().frame_duration().ticks();
    const timeline::Time half = timeline::Time::from_ticks(width * 3 / 2);

    const std::optional<double> value = document().lanes()[0].evaluate_keyframes(half);

    ASSERT_TRUE(value);
    EXPECT_DOUBLE_EQ(100, *value);
}

TEST_F(ExtrapolationPartialImportTest, reportsEveryMalformedTrack)
{
    const int count = timeline::size_cast(import_result().diagnostics);

    EXPECT_EQ(6, count);
}

TEST_F(ExtrapolationPartialImportTest, retainsOnlyValidTrackAfterMalformedTracks)
{
    EXPECT_EQ(1, document().lane_count());
}

TEST_F(ExtrapolationPartialImportTest, retainsExpectedValidTrackAfterMalformedTracks)
{
    ASSERT_EQ(1, document().lane_count());

    const std::string_view identity = document().strings().lookup(document().lanes()[0].id());

    EXPECT_EQ("animation-6", identity);
}

TEST_P(ExtrapolationDiagnosticTest, identifiesMalformedForm)
{
    const DiagnosticCase &test_case = definition();

    const std::string &diagnostic = diagnostic_at(import_result(), test_case.index);

    EXPECT_NE(std::string::npos, diagnostic.find("animation-" + std::to_string(test_case.index) + ":"));
    EXPECT_NE(std::string::npos, diagnostic.find(test_case.message));
}

INSTANTIATE_TEST_SUITE_P(
    MalformedForms, ExtrapolationDiagnosticTest, testing::ValuesIn(DIAGNOSTIC_CASES), case_name<DiagnosticCase>);

TEST(ExtrapolationDiagnostics, rejectsUnsupportedPolicy)
{
    const std::filesystem::path fixture("fixtures/extrapolation-invalid.json");

    const JsonImportResult result = import_timeline_json(fixture);

    EXPECT_FALSE(result.succeeded());
}

TEST(ExtrapolationDiagnostics, reportsSingleUnsupportedPolicyDiagnostic)
{
    const JsonImportResult result = import_timeline_json("fixtures/extrapolation-invalid.json");

    const int count = timeline::size_cast(result.diagnostics);

    EXPECT_EQ(1, count);
}

TEST(ExtrapolationDiagnostics, identifiesUnsupportedPolicy)
{
    const JsonImportResult result = import_timeline_json("fixtures/extrapolation-invalid.json");

    const std::string &diagnostic = diagnostic_at(result, 0);

    EXPECT_NE(std::string::npos, diagnostic.find("unknown extrapolate"));
}
