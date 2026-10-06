// Copyright (c) 2026 Richard Thomson

#include <ResolvedAttributes.h>

#include <timelineParAnimator/TimelineJson.h>

#include <timeline/Layout.h>
#include <timeline/Query.h>
#include <timeline/size_cast.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <optional>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

using namespace timeline_par_animator;

namespace
{

/// One integer-valued lane and its expected midpoint values.
///
struct RoundedLaneCase
{
    std::string name;
    int lane;
    std::string parameter;
    int component;
    double fractional;
    double rounded;
};

/// One tick adjacent to a positive or negative halfway boundary.
///
struct RoundingTickCase
{
    std::string name;
    int lane;
    timeline::Ticks adjustment;
    double expected;
};

/// One integer-output extrapolation or interpolation policy.
///
struct PolicyCase
{
    std::string name;
    int lane;
    std::string parameter;
};

/// One fractional policy value and its rounded output.
///
struct PolicyRoundingCase
{
    std::string name;
    int lane;
    double fractional;
    double rounded;
};

/// One output sampled at a specific fixture frame.
///
struct FrameOutputCase
{
    std::string name;
    int lane;
    int frame;
    std::optional<double> expected;
};

/// Relative position of a source-computed value around a halfway boundary.
///
enum class HalfwaySide
{
    BELOW,
    ABOVE
};

/// One source arithmetic boundary and its rounded output.
///
struct ArithmeticBoundaryCase
{
    std::string name;
    int lane;
    std::string parameter;
    HalfwaySide side;
    double halfway;
    double rounded;
};

/// One held enum value sampled at a source frame.
///
struct EnumFrameCase
{
    std::string name;
    int frame;
    std::string value;
};

/// One independently diagnosed malformed integer-output track.
///
struct DiagnosticCase
{
    std::string name;
    int index;
    std::string message;
};

const std::vector<RoundedLaneCase> ROUNDED_LANE_CASES{
    {"Maxiter", 0, "maxiter", 0, 100.5, 101.0},
    {"Negative", 1, "negative", 0, -2.5, -3.0},
    {"Choice", 2, "choice", 0, -2.5, -3.0},
    {"TupleFirst", 3, "tuple", 0, 100.5, 101.0},
    {"TupleSecond", 4, "tuple", 1, -2.5, -3.0},
};

const std::vector<RoundingTickCase> ROUNDING_TICK_CASES{
    {"PositiveBeforeHalf", 0, -1, 100.0},
    {"PositiveAfterHalf", 0, 1, 101.0},
    {"NegativeBeforeHalf", 1, -1, -3.0},
    {"NegativeAfterHalf", 1, 1, -2.0},
};

const std::vector<PolicyCase> POLICY_CASES{
    {"Base", 0, "base"},
    {"Omit", 1, "omit"},
    {"Cycle", 2, "cycle"},
    {"PingPong", 3, "ping"},
    {"Hold", 4, "hold"},
    {"Step", 5, "step"},
};

const std::vector<PolicyRoundingCase> POLICY_ROUNDING_CASES{
    {"Cycle", 2, 101.5, 102.0},
    {"PingPong", 3, -1.5, -2.0},
};

const std::vector<FrameOutputCase> GAP_CASES{
    {"BeforeFirstKey", 1, 0, std::nullopt},
    {"AfterLastKey", 1, 6, std::nullopt},
};

const std::vector<FrameOutputCase> BASE_CASES{
    {"BeforeFirstKey", 0, 0, 678.0},
    {"AfterLastKey", 0, 6, 678.0},
};

const std::vector<FrameOutputCase> EXTRAPOLATED_CASES{
    {"CycleBeforeFirstKey", 2, 0, 103.0},
    {"CycleAfterLastKey", 2, 6, 100.0},
    {"PingPongBeforeFirstKey", 3, 0, -2.0},
    {"PingPongAfterLastKey", 3, 6, -1.0},
};

const std::vector<FrameOutputCase> HELD_CASES{
    {"HoldBetweenKeys", 4, 3, -3.0},
    {"HoldAfterLastKey", 4, 8, 0.0},
    {"StepBetweenKeys", 5, 3, 100.0},
    {"StepAfterLastKey", 5, 8, 103.0},
};

const std::vector<ArithmeticBoundaryCase> ARITHMETIC_BOUNDARY_CASES{
    {"Positive", 0, "choice", HalfwaySide::BELOW, 7.5, 7.0},
    {"Negative", 1, "negative", HalfwaySide::ABOVE, -7.5, -7.0},
};

const std::vector<EnumFrameCase> ENUM_FRAME_CASES{
    {"FrameOne", 0, "a"},
    {"FrameTwo", 1, "a"},
    {"FrameThree", 2, "a"},
    {"FrameFour", 3, "a"},
    {"FrameFive", 4, "a"},
    {"FrameSix", 5, "a"},
    {"FrameSeven", 6, "b"},
};

const std::vector<DiagnosticCase> DIAGNOSTIC_CASES{
    {"IntegralScalar", 0, "integral"},
    {"IntegerRange", 1, "int range"},
    {"CurvePolicy", 2, "curves"},
    {"IntegralComponent", 3, "integral"},
    {"TupleArity", 4, "arity"},
    {"CatalogBounds", 5, "bounds"},
};

void PrintTo(const RoundedLaneCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const RoundingTickCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const PolicyCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const PolicyRoundingCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const FrameOutputCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const ArithmeticBoundaryCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const EnumFrameCase &value, std::ostream *stream)
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
    std::ostringstream name_builder;
    name_builder << "frame-" << std::setfill('0') << std::setw(4) << frame + 1 << " {";
    const std::string name = name_builder.str();
    const std::string::size_type start = golden.find(name);
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

std::optional<double> golden_value(const std::string &entry, std::string_view parameter, int component)
{
    const std::string name = "\n    " + std::string(parameter) + "=";
    std::string::size_type start = entry.find(name);
    if (start == std::string::npos)
    {
        return std::nullopt;
    }
    start += name.size();
    for (int index = 0; index < component; ++index)
    {
        const std::string::size_type separator = entry.find('/', start);
        if (separator == std::string::npos)
        {
            throw std::runtime_error("golden tuple component is missing");
        }
        start = separator + 1;
    }
    return std::stod(entry.substr(start));
}

JsonImportResult import_clean_result(const std::filesystem::path &path)
{
    JsonImportResult result = import_timeline_json(path);
    if (!result.succeeded() || !result.diagnostics.empty())
    {
        throw std::runtime_error("test fixture failed to import cleanly");
    }
    return result;
}

timeline::Document import_clean_document(const std::filesystem::path &path)
{
    return *import_clean_result(path).document;
}

timeline::Document import_partial_document(const std::filesystem::path &path)
{
    const JsonImportResult result = import_timeline_json(path);
    if (!result.succeeded())
    {
        throw std::runtime_error("partial test fixture did not produce a document");
    }
    return *result.document;
}

timeline::FrameInspection inspect_at(const timeline::Document &document, int frame)
{
    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(document, frame);
    if (!inspection)
    {
        throw std::out_of_range("test frame is outside the document");
    }
    return *inspection;
}

std::vector<std::optional<double>> golden_values(
    const std::filesystem::path &path, std::string_view parameter, int component, int frame_count)
{
    const std::string golden = read_text(path);
    std::vector<std::optional<double>> result;
    for (int frame = 0; frame < frame_count; ++frame)
    {
        result.push_back(golden_value(golden_entry(golden, frame), parameter, component));
    }
    return result;
}

std::vector<std::optional<double>> lane_values(const timeline::Document &document, int lane)
{
    std::vector<std::optional<double>> result;
    for (int frame = 0; frame < document.frame_grid()->frame_count(); ++frame)
    {
        result.push_back(inspect_at(document, frame).lanes[lane].value);
    }
    return result;
}

std::vector<std::optional<double>> lane_output_values(const timeline::Document &document, int lane)
{
    std::vector<std::optional<double>> result;
    for (int frame = 0; frame < document.frame_grid()->frame_count(); ++frame)
    {
        result.push_back(inspect_at(document, frame).lanes[lane].output_value);
    }
    return result;
}

const std::string &diagnostic_at(const JsonImportResult &result, int index)
{
    if (index < 0 || timeline::size_cast(result.diagnostics) <= index)
    {
        throw std::out_of_range("test diagnostic index is out of range");
    }
    return result.diagnostics[index];
}

timeline::Document integer_document()
{
    return import_clean_document("fixtures/integer-output.json");
}

timeline::Document policy_document()
{
    return import_clean_document("fixtures/integer-output-policies.json");
}

timeline::Document boundary_document()
{
    return import_clean_document("fixtures/integer-output-boundary.json");
}

timeline::Document combine_with_music(const timeline::Document &document)
{
    const timeline::Document music = import_clean_document("fixtures/beat-keys/rms.beat-keys.json");
    return timeline::combine_documents(document, music);
}

timeline::HitResult animation_zero_hit(const timeline::Layout &layout)
{
    for (const timeline::Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<timeline::Polyline>(primitive))
        {
            const timeline::Polyline &line = std::get<timeline::Polyline>(primitive);
            if (layout.display_list().strings().lookup(line.id.lane_id) == "animation-0")
            {
                const std::optional<timeline::HitResult> result = layout.hit_test(line.points.front(), 0);
                if (result)
                {
                    return *result;
                }
            }
        }
    }
    throw std::runtime_error("integer-output curve is not hit-testable");
}

/// Owns the base integer-output document for fixed test cases.
///
class IntegerDocumentTest : public testing::Test
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
    timeline::Document m_document{integer_document()};
};

/// Owns the base integer-output document for a parameterized test case.
///
template <typename Case>
class ParameterizedIntegerDocumentTest : public testing::TestWithParam<Case>
{
protected:
    void SetUp() override
    {
        m_document.emplace(integer_document());
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

/// Exercises the continuous double lane in the base integer document.
///
class IntegerOutputDoubleTest : public IntegerDocumentTest
{
};

/// Exercises the categorical lane in the base integer document.
///
class IntegerOutputEnumTest : public IntegerDocumentTest
{
};

/// Owns an independently copied integer-output document.
///
class IntegerOutputCopyTest : public IntegerDocumentTest
{
protected:
    IntegerOutputCopyTest() :
        m_imported(import_clean_result("fixtures/integer-output.json")),
        m_copied_document(*m_imported.document)
    {
    }
    void release_imported_document()
    {
        m_imported.document.reset();
    }
    const timeline::Document &copied_document() const
    {
        return m_copied_document;
    }

private:
    JsonImportResult m_imported;
    timeline::Document m_copied_document;
};

/// Owns the base integer-output document composed with music.
///
class IntegerOutputCompositionTest : public IntegerDocumentTest
{
protected:
    const timeline::Document &combined_document() const
    {
        return m_combined_document;
    }

private:
    timeline::Document m_combined_document{combine_with_music(document())};
};

/// Owns the standard layout for the music-composed integer document.
///
class IntegerOutputLayoutTest : public IntegerOutputCompositionTest
{
protected:
    IntegerOutputLayoutTest() :
        m_layout(combined_document(),
            timeline::Viewport(
                600, 400, combined_document().frame_grid()->offset(), combined_document().frame_grid()->end_time()),
            timeline::LayoutMetrics(100, 20, 40, 4))
    {
    }
    const timeline::Layout &layout() const
    {
        return m_layout;
    }

private:
    timeline::Layout m_layout;
};

/// Exercises one integer-valued output lane.
///
class RoundedIntegerLaneTest : public ParameterizedIntegerDocumentTest<RoundedLaneCase>
{
};

/// Exercises one tick adjacent to a halfway rounding boundary.
///
class IntegerRoundingTickTest : public ParameterizedIntegerDocumentTest<RoundingTickCase>
{
};

/// Exercises one extrapolation or interpolation policy.
///
class IntegerPolicyTest : public testing::TestWithParam<PolicyCase>
{
};

/// Exercises rounding of one fractional policy value.
///
class IntegerPolicyRoundingTest : public testing::TestWithParam<PolicyRoundingCase>
{
};

/// Exercises an omitted output outside its authored key range.
///
class IntegerGapTest : public testing::TestWithParam<FrameOutputCase>
{
};

/// Exercises source-value extrapolation outside its authored key range.
///
class IntegerBaseTest : public testing::TestWithParam<FrameOutputCase>
{
};

/// Exercises repeated and reflected output beyond the authored key range.
///
class IntegerExtrapolationTest : public testing::TestWithParam<FrameOutputCase>
{
};

/// Exercises hold and step output values.
///
class IntegerHeldValueTest : public testing::TestWithParam<FrameOutputCase>
{
};

/// Exercises source arithmetic near a halfway rounding boundary.
///
class IntegerArithmeticBoundaryTest : public testing::TestWithParam<ArithmeticBoundaryCase>
{
};

/// Exercises one frame of a held categorical value.
///
class IntegerEnumFrameTest : public ParameterizedIntegerDocumentTest<EnumFrameCase>
{
};

/// Exercises one malformed integer-output track and its diagnostic.
///
class IntegerOutputDiagnosticTest : public testing::TestWithParam<DiagnosticCase>
{
};

} // namespace

TEST_P(RoundedIntegerLaneTest, retainsFractionalCurveValue)
{
    const RoundedLaneCase &test_case = definition();

    const std::optional<double> value = inspect_at(document(), 1).lanes[test_case.lane].value;

    ASSERT_TRUE(value);
    EXPECT_DOUBLE_EQ(test_case.fractional, *value);
}

TEST_P(RoundedIntegerLaneTest, declaresOutputRounding)
{
    const RoundedLaneCase &test_case = definition();
    const timeline::FrameInspection inspection = inspect_at(document(), 1);

    const int count =
        resolved_attributes(document(), inspection.lanes[test_case.lane].items[0].attributes).count("output-rounding");

    EXPECT_EQ(1, count);
}

TEST_P(RoundedIntegerLaneTest, namesOutputRoundingPolicy)
{
    const RoundedLaneCase &test_case = definition();
    const timeline::FrameInspection inspection = inspect_at(document(), 1);

    const std::string_view policy =
        resolved_attributes(document(), inspection.lanes[test_case.lane].items[0].attributes).at("output-rounding");

    EXPECT_EQ("nearest-half-away-from-zero", policy);
}

TEST_P(RoundedIntegerLaneTest, roundsOutputHalfAwayFromZero)
{
    const RoundedLaneCase &test_case = definition();

    const std::optional<double> value = inspect_at(document(), 1).lanes[test_case.lane].output_value;

    ASSERT_TRUE(value);
    EXPECT_DOUBLE_EQ(test_case.rounded, *value);
}

TEST_P(RoundedIntegerLaneTest, matchesSourceGolden)
{
    const RoundedLaneCase &test_case = definition();

    const std::vector<std::optional<double>> actual = lane_output_values(document(), test_case.lane);
    const std::vector<std::optional<double>> expected = golden_values(
        "fixtures/gold-integer-output.par", test_case.parameter, test_case.component, frame_grid().frame_count());

    EXPECT_EQ(expected, actual);
}

INSTANTIATE_TEST_SUITE_P(
    RoundedLanes, RoundedIntegerLaneTest, testing::ValuesIn(ROUNDED_LANE_CASES), case_name<RoundedLaneCase>);

TEST_P(IntegerRoundingTickTest, roundsAtExpectedSideOfHalfway)
{
    const RoundingTickCase &test_case = definition();
    const timeline::Time half = frame_grid().frame_start(1);
    const timeline::Time time = timeline::Time::from_ticks(half.ticks() + test_case.adjustment);

    const std::optional<double> value = document().lanes()[test_case.lane].evaluate_keyframe_output(time);

    ASSERT_TRUE(value);
    EXPECT_DOUBLE_EQ(test_case.expected, *value);
}

INSTANTIATE_TEST_SUITE_P(
    AdjacentTicks, IntegerRoundingTickTest, testing::ValuesIn(ROUNDING_TICK_CASES), case_name<RoundingTickCase>);

TEST_F(IntegerOutputDoubleTest, retainsFractionalCurveValue)
{
    const std::optional<double> value = inspect_at(document(), 1).lanes[5].value;

    ASSERT_TRUE(value);
    EXPECT_DOUBLE_EQ(1.5, *value);
}

TEST_F(IntegerOutputDoubleTest, omitsRoundedOutput)
{
    const std::vector<std::optional<double>> outputs = lane_output_values(document(), 5);

    for (const std::optional<double> &value : outputs)
    {
        EXPECT_FALSE(value);
    }
}

TEST_F(IntegerOutputDoubleTest, matchesSourceGolden)
{
    const std::vector<std::optional<double>> actual = lane_values(document(), 5);
    const std::vector<std::optional<double>> expected =
        golden_values("fixtures/gold-integer-output.par", "bailout", 0, frame_grid().frame_count());

    EXPECT_EQ(expected, actual);
}

TEST_F(IntegerOutputEnumTest, omitsNumericValue)
{
    const std::vector<std::optional<double>> values = lane_values(document(), 6);

    for (const std::optional<double> &value : values)
    {
        EXPECT_FALSE(value);
    }
}

TEST_F(IntegerOutputEnumTest, omitsRoundedOutput)
{
    const std::vector<std::optional<double>> outputs = lane_output_values(document(), 6);

    for (const std::optional<double> &value : outputs)
    {
        EXPECT_FALSE(value);
    }
}

TEST_P(IntegerEnumFrameTest, preservesHeldSourceValue)
{
    const EnumFrameCase &test_case = definition();
    const timeline::FrameInspection inspection = inspect_at(document(), test_case.frame);

    const std::string_view value = resolved_attributes(document(), inspection.lanes[6].items[0].attributes).at("value");

    EXPECT_EQ(test_case.value, value);
}

INSTANTIATE_TEST_SUITE_P(
    EnumFrames, IntegerEnumFrameTest, testing::ValuesIn(ENUM_FRAME_CASES), case_name<EnumFrameCase>);

TEST_F(IntegerOutputCopyTest, survivesImportResultRelease)
{
    const std::optional<double> expected = inspect_at(document(), 1).lanes[0].output_value;

    release_imported_document();
    const std::optional<double> actual = inspect_at(copied_document(), 1).lanes[0].output_value;

    ASSERT_TRUE(expected);
    ASSERT_TRUE(actual);
    EXPECT_DOUBLE_EQ(*expected, *actual);
}

TEST_F(IntegerOutputCompositionTest, combinesWithMusicDocument)
{
    EXPECT_EQ(11, combined_document().lane_count());
}

TEST_F(IntegerOutputCompositionTest, preservesRoundedOutput)
{
    const std::optional<double> value = inspect_at(combined_document(), 1).lanes[0].output_value;

    ASSERT_TRUE(value);
    EXPECT_DOUBLE_EQ(101, *value);
}

TEST_F(IntegerOutputLayoutTest, hitTestsLaneIdentity)
{
    const timeline::HitResult hit = animation_zero_hit(layout());
    const std::string_view identity = layout().display_list().strings().lookup(hit.id.lane_id);

    EXPECT_EQ("animation-0", identity);
}

TEST_F(IntegerOutputLayoutTest, hitTestsKeyIdentity)
{
    const timeline::HitResult hit = animation_zero_hit(layout());
    const std::string_view identity = layout().display_list().strings().lookup(hit.id.item_id);

    EXPECT_EQ("animation-0-key-0", identity);
}

TEST(IntegerOutput, rejectsFractionalIntegerKeys)
{
    const std::filesystem::path fixture("fixtures/integer-output-invalid.json");

    const JsonImportResult result = import_timeline_json(fixture);

    EXPECT_FALSE(result.succeeded());
}

TEST_P(IntegerPolicyTest, matchesSourceGolden)
{
    const PolicyCase &definition = GetParam();
    const timeline::Document document = policy_document();
    const int frame_count = document.frame_grid()->frame_count();

    const std::vector<std::optional<double>> actual = lane_output_values(document, definition.lane);
    const std::vector<std::optional<double>> expected =
        golden_values("fixtures/gold-integer-output-policies.par", definition.parameter, 0, frame_count);

    EXPECT_EQ(expected, actual);
}

INSTANTIATE_TEST_SUITE_P(Policies, IntegerPolicyTest, testing::ValuesIn(POLICY_CASES), case_name<PolicyCase>);

TEST_P(IntegerPolicyRoundingTest, retainsFractionalValue)
{
    const PolicyRoundingCase &definition = GetParam();
    const timeline::Document document = policy_document();

    const std::optional<double> value = inspect_at(document, 3).lanes[definition.lane].value;

    ASSERT_TRUE(value);
    EXPECT_DOUBLE_EQ(definition.fractional, *value);
}

TEST_P(IntegerPolicyRoundingTest, roundsOutput)
{
    const PolicyRoundingCase &definition = GetParam();
    const timeline::Document document = policy_document();

    const std::optional<double> value = inspect_at(document, 3).lanes[definition.lane].output_value;

    ASSERT_TRUE(value);
    EXPECT_DOUBLE_EQ(definition.rounded, *value);
}

INSTANTIATE_TEST_SUITE_P(
    PolicyRounding, IntegerPolicyRoundingTest, testing::ValuesIn(POLICY_ROUNDING_CASES), case_name<PolicyRoundingCase>);

TEST_P(IntegerGapTest, omitsOutputOutsideKeyRange)
{
    const FrameOutputCase &definition = GetParam();
    const timeline::Document document = policy_document();

    const std::optional<double> value = inspect_at(document, definition.frame).lanes[definition.lane].output_value;

    EXPECT_EQ(definition.expected, value);
}

INSTANTIATE_TEST_SUITE_P(Gaps, IntegerGapTest, testing::ValuesIn(GAP_CASES), case_name<FrameOutputCase>);

TEST_P(IntegerBaseTest, restoresSourceOutputOutsideKeyRange)
{
    const FrameOutputCase &definition = GetParam();
    const timeline::Document document = policy_document();

    const std::optional<double> value = inspect_at(document, definition.frame).lanes[definition.lane].output_value;

    EXPECT_EQ(definition.expected, value);
}

INSTANTIATE_TEST_SUITE_P(BaseValues, IntegerBaseTest, testing::ValuesIn(BASE_CASES), case_name<FrameOutputCase>);

TEST_P(IntegerExtrapolationTest, producesExpectedOutputOutsideKeyRange)
{
    const FrameOutputCase &definition = GetParam();
    const timeline::Document document = policy_document();

    const std::optional<double> value = inspect_at(document, definition.frame).lanes[definition.lane].output_value;

    EXPECT_EQ(definition.expected, value);
}

INSTANTIATE_TEST_SUITE_P(
    ExtrapolatedValues, IntegerExtrapolationTest, testing::ValuesIn(EXTRAPOLATED_CASES), case_name<FrameOutputCase>);

TEST_P(IntegerHeldValueTest, preservesHeldOutput)
{
    const FrameOutputCase &definition = GetParam();
    const timeline::Document document = policy_document();

    const std::optional<double> value = inspect_at(document, definition.frame).lanes[definition.lane].output_value;

    EXPECT_EQ(definition.expected, value);
}

INSTANTIATE_TEST_SUITE_P(HeldValues, IntegerHeldValueTest, testing::ValuesIn(HELD_CASES), case_name<FrameOutputCase>);

TEST(IntegerOutputDiagnostics, reportsEveryMalformedTrack)
{
    const JsonImportResult result = import_timeline_json("fixtures/integer-output-partial.json");

    const int count = timeline::size_cast(result.diagnostics);

    EXPECT_EQ(6, count);
}

TEST(IntegerOutputDiagnostics, retainsOnlyValidTrack)
{
    const timeline::Document document = import_partial_document("fixtures/integer-output-partial.json");

    const int count = document.lane_count();

    EXPECT_EQ(1, count);
}

TEST(IntegerOutputDiagnostics, retainsExpectedValidTrack)
{
    const timeline::Document document = import_partial_document("fixtures/integer-output-partial.json");
    if (document.lane_count() != 1)
    {
        throw std::runtime_error("partial fixture has an unexpected lane count");
    }

    const std::string_view identity = document.strings().lookup(document.lanes()[0].id());

    EXPECT_EQ("animation-6", identity);
}

TEST(IntegerOutputDiagnostics, leavesValidDoubleOutputUnrounded)
{
    const timeline::Document document = import_partial_document("fixtures/integer-output-partial.json");

    const std::optional<double> output = inspect_at(document, 1).lanes[0].output_value;

    EXPECT_FALSE(output);
}

TEST_P(IntegerOutputDiagnosticTest, identifiesMalformedForm)
{
    const DiagnosticCase &definition = GetParam();
    const JsonImportResult result = import_timeline_json("fixtures/integer-output-partial.json");

    const std::string &diagnostic = diagnostic_at(result, definition.index);

    EXPECT_NE(std::string::npos, diagnostic.find("animation-" + std::to_string(definition.index) + ":"));
    EXPECT_NE(std::string::npos, diagnostic.find(definition.message));
}

INSTANTIATE_TEST_SUITE_P(
    MalformedTracks, IntegerOutputDiagnosticTest, testing::ValuesIn(DIAGNOSTIC_CASES), case_name<DiagnosticCase>);

TEST_P(IntegerArithmeticBoundaryTest, matchesSourceGolden)
{
    const ArithmeticBoundaryCase &definition = GetParam();
    const timeline::Document document = boundary_document();
    const int frame_count = document.frame_grid()->frame_count();

    const std::vector<std::optional<double>> actual = lane_output_values(document, definition.lane);
    const std::vector<std::optional<double>> expected =
        golden_values("fixtures/gold-integer-output-boundary.par", definition.parameter, 0, frame_count);

    EXPECT_EQ(expected, actual);
}

TEST_P(IntegerArithmeticBoundaryTest, preservesSourceSideOfHalfway)
{
    const ArithmeticBoundaryCase &definition = GetParam();
    const timeline::Document document = boundary_document();

    const std::optional<double> value = inspect_at(document, 15).lanes[definition.lane].value;

    ASSERT_TRUE(value);
    if (definition.side == HalfwaySide::BELOW)
    {
        EXPECT_LT(*value, definition.halfway);
    }
    else
    {
        EXPECT_GT(*value, definition.halfway);
    }
}

TEST_P(IntegerArithmeticBoundaryTest, roundsSourceComputedValue)
{
    const ArithmeticBoundaryCase &definition = GetParam();
    const timeline::Document document = boundary_document();

    const std::optional<double> value = inspect_at(document, 15).lanes[definition.lane].output_value;

    ASSERT_TRUE(value);
    EXPECT_DOUBLE_EQ(definition.rounded, *value);
}

INSTANTIATE_TEST_SUITE_P(Boundaries, IntegerArithmeticBoundaryTest, testing::ValuesIn(ARITHMETIC_BOUNDARY_CASES),
    case_name<ArithmeticBoundaryCase>);
