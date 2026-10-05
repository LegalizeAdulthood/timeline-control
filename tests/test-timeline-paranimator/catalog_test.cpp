// Copyright (c) 2026 Richard Thomson

#include <ResolvedAttributes.h>

#include <timelineParAnimator/TimelineJson.h>

#include <timeline/Layout.h>
#include <timeline/Query.h>
#include <timeline/size_cast.h>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

using namespace timeline_par_animator;

namespace
{

using Json = nlohmann::json;

/// Named JSON input for one catalog validation behavior.
///
struct NamedJsonCase
{
    std::string name;
    Json value;
};

/// Named string input for one catalog validation behavior.
///
struct NamedStringCase
{
    std::string name;
    std::string value;
};

/// Catalog metadata and animated value that must be rejected together.
///
struct InvalidValueCase
{
    std::string name;
    Json metadata;
    Json value;
};

/// Expected evaluation and metadata for one imported component lane.
///
struct ComponentCase
{
    std::string name;
    int lane;
    double value;
    int component;
    int arity;
};

/// Expected diagnostic text for one invalid source track.
///
struct DiagnosticCase
{
    std::string name;
    int index;
    std::string message;
};

/// Numeric target resolved against the ParAnimator reference output.
///
struct NumericTargetCase
{
    std::string name;
    std::string parameter;
    std::vector<int> lanes;
    std::vector<double> frame_one;
};

/// Discrete target resolved against the ParAnimator reference output.
///
struct DiscreteTargetCase
{
    std::string name;
    std::string parameter;
    int lane;
    std::string frame_one;
};

/// Expected reference values for one catalog parameter over all frames.
///
struct ReferenceCase
{
    std::string name;
    std::string parameter;
    int first_lane;
    bool output;
};

const std::vector<InvalidValueCase> INVALID_VALUE_CASES{
    {"DoubleArray", {{"type", "double"}, {"default-curve", "linear"}}, Json::array({1})},
    {"FractionalInteger", {{"type", "integer"}, {"default-curve", "linear"}}, "1.5"},
    {"ComplexArity", {{"type", "complex"}, {"default-curve", "linear"}}, "1/2/3"},
    {"ComplexArray", {{"type", "complex"}, {"default-curve", "linear"}}, Json::array({1, 2})},
    {"UnknownEnum", {{"type", "enum"}, {"default-curve", "hold"}, {"values", {"a", "b"}}}, "unknown"},
    {"NumericEnum", {{"type", "enum"}, {"default-curve", "hold"}, {"values", {"1", "2"}}}, 1},
    {"YesNoString", {{"type", "yes-no"}, {"default-curve", "hold"}}, "yes"},
    {"NumericString", {{"type", "string"}, {"default-curve", "hold"}}, 1},
    {"UnknownFunction", {{"type", "function-list"}, {"default-curve", "hold"}, {"values", "id-functions"}},
        Json::array({"bad"})},
    {"CornersArity", {{"type", "corners"}}, "1/2/3"},
    {"CenterMagnificationArity", {{"type", "center-mag"}}, "1/2"},
};

const std::vector<NamedStringCase> UNSUPPORTED_TYPE_CASES{
    {"Miim", "miim"},
    {"Potential", "potential"},
    {"NumericTupleOrEnum", "numeric-tuple-or-enum"},
    {"ColorMap", "color-map"},
};

const std::vector<NamedJsonCase> MALFORMED_METADATA_CASES{
    {"Null", nullptr},
    {"Array", Json::array()},
    {"EmptyObject", Json::object()},
    {"UnknownType", {{"type", "unknown"}, {"description", "Unused"}}},
    {"EmptyDescription", {{"type", "double"}, {"description", ""}}},
    {"NumericDescription", {{"type", "double"}, {"description", 1}}},
    {"UnknownFormat", {{"type", "double"}, {"description", "Unused"}, {"format", "unknown"}}},
    {"UnknownDefaultCurve", {{"type", "double"}, {"description", "Unused"}, {"default-curve", "unknown"}}},
    {"UnknownExtrapolation", {{"type", "double"}, {"description", "Unused"}, {"extrapolate", "unknown"}}},
    {"StringMinimum", {{"type", "double"}, {"description", "Unused"}, {"min", "zero"}}},
    {"NullMaximum", {{"type", "double"}, {"description", "Unused"}, {"max", nullptr}}},
    {"NumericNormalize", {{"type", "double"}, {"description", "Unused"}, {"normalize", 1}}},
    {"ZeroArity", {{"type", "numeric-tuple"}, {"description", "Unused"}, {"arity", 0}}},
    {"FractionalArity", {{"type", "numeric-tuple"}, {"description", "Unused"}, {"arity", 1.5}}},
    {"FixedPointArity", {{"type", "point2"}, {"description", "Unused"}, {"arity", 3}}},
    {"MissingEnumValues", {{"type", "enum"}, {"description", "Unused"}}},
    {"EmptyEnumValues", {{"type", "enum"}, {"description", "Unused"}, {"values", Json::array()}}},
    {"NumericEnumValue", {{"type", "enum"}, {"description", "Unused"}, {"values", {1}}}},
    {"UnexpectedDoubleValues", {{"type", "double"}, {"description", "Unused"}, {"values", {"a"}}}},
    {"ArrayFunctionValues", {{"type", "function-list"}, {"description", "Unused"}, {"values", {"sin"}}}},
    {"UnknownFunctionValues", {{"type", "function-list"}, {"description", "Unused"}, {"values", "unknown"}}},
};

const std::vector<NamedJsonCase> NESTED_METADATA_CASES{
    {"FractalTypesArray", Json::parse(R"({"fractal-types":[]})")},
    {"FormulaEntriesArray", Json::parse(R"({"formula-entries":[]})")},
    {"NumericFractalEntry", Json::parse(R"({"fractal-types":{"unused":1}})")},
    {"NumericSlots", Json::parse(R"({"fractal-types":{"unused":{"params":{"slots":1}}}})")},
    {"IncompleteSlot", Json::parse(R"({"fractal-types":{"unused":{"params":{"slots":[{"index":0,"name":"slot"}]}}}})")},
    {"MissingGroupSlots",
        Json::parse(
            R"({"fractal-types":{"unused":{"params":{"groups":{"c":{"type":"complex","description":"C"}}}}}})")},
    {"MissingFunctionDefault",
        Json::parse(
            R"({"fractal-types":{"unused":{"functions":{"fn5":{"type":"enum","values":"id-functions","description":"Function"}}}}})")},
    {"InvalidKnobVariable",
        Json::parse(
            R"({"formula-entries":{"unused":{"params":{"knobs":{"k":{"type":"real","description":"Knob","variable":"p5.real"}}}}}})")},
    {"InvalidFormulaFunction",
        Json::parse(
            R"({"formula-entries":{"unused":{"functions":{"fn1":{"type":"double","description":"Function","values":"id-functions"}}}}})")},
};

const std::vector<NamedStringCase> DUPLICATE_SECTION_CASES{
    {"Parameters", "parameters"},
    {"FractalTypes", "fractal-types"},
    {"FormulaEntries", "formula-entries"},
};

const std::vector<NamedJsonCase> CATALOG_ROOT_CASES{
    {"Null", nullptr},
    {"Array", Json::array()},
    {"EmptyObject", Json::object()},
    {"NumericParameters", {{"parameters", 1}}},
};

const std::vector<NamedStringCase> SOURCE_TYPE_CASES{
    {"CenterMagnification", "center-mag"},
    {"ColorMap", "color-map"},
    {"Corners", "corners"},
    {"Complex", "complex"},
    {"Double", "double"},
    {"YesNo", "yes-no"},
    {"Integer", "integer"},
    {"IntegerTuple", "integer-tuple"},
    {"Miim", "miim"},
    {"NumericTuple", "numeric-tuple"},
    {"PointTwo", "point2"},
    {"PointThree", "point3"},
    {"Potential", "potential"},
    {"String", "string"},
    {"VectorTwo", "vector2"},
    {"VectorThree", "vector3"},
    {"Enum", "enum"},
    {"Inside", "inside"},
    {"Outside", "outside"},
    {"IntegerOrEnum", "integer-or-enum"},
    {"NumericTupleOrEnum", "numeric-tuple-or-enum"},
    {"FunctionList", "function-list"},
};

const std::vector<NamedStringCase> CATEGORICAL_TYPE_CASES{
    {"String", "string"},
    {"Enum", "enum"},
    {"FunctionList", "function-list"},
};

const std::vector<ComponentCase> COMPONENT_CASES{
    {"PositionX", 0, 2, 0, 2},
    {"PositionY", 1, 4, 1, 2},
    {"DirectionX", 2, 1, 0, 3},
    {"DirectionY", 3, 2, 1, 3},
    {"DirectionZ", 4, 3, 2, 3},
    {"TupleX", 5, 2, 0, 2},
    {"TupleY", 6, 4, 1, 2},
    {"Scalar", 7, 4.75, 0, 1},
    {"Maxiter", 8, 100.75, 0, 1},
};

const std::vector<DiagnosticCase> SOURCE_DIAGNOSTIC_CASES{
    {"PositionArity", 0, "arity"},
    {"DirectionArity", 1, "arity"},
    {"ScalarArray", 2, "scalar"},
    {"TupleArray", 3, "array"},
    {"ScalarUpperBound", 4, "bounds"},
    {"PositionLowerBound", 5, "bounds"},
    {"MissingDefaultCurve", 6, "default-curve"},
    {"ConstantDirectionPath", 7, "path"},
    {"LineDirectionPath", 8, "path"},
    {"GeometricScalarCurve", 9, "curve"},
    {"GeometricPositionCurve", 10, "curve"},
    {"IntegerTupleArray", 11, "array"},
    {"MissingIntegerDefaultCurve", 12, "default-curve"},
    {"ConstantPositionPath", 13, "path"},
    {"LinePositionPath", 14, "path"},
};

const std::vector<NumericTargetCase> NUMERIC_TARGET_CASES{
    {"Maxiter", "maxiter", {0}, {101}},
    {"Params", "params", {1, 2, 3}, {1.5, 2.5, 1}},
    {"Complex", "complex", {6, 7}, {3, 5}},
    {"CenterMagnification", "center-mag", {8, 9, 10}, {1, 2, 2}},
    {"Corners", "corners", {11, 12, 13, 14}, {-2.5, -0.5, -1.5, 2.5}},
};

const std::vector<DiscreteTargetCase> DISCRETE_TARGET_CASES{
    {"Function", "function", 4, "sin/sin"},
    {"Label", "label", 5, "1"},
    {"ShowOrbit", "showorbit", 15, "no"},
    {"Functions", "functions", 16, "sin/cos"},
};

const std::vector<ReferenceCase> REFERENCE_CASES{
    {"Position", "position", 0, false},
    {"Direction", "direction", 2, false},
    {"Tuple", "tuple", 5, false},
    {"Scalar", "scalar", 7, false},
    {"Maxiter", "maxiter", 8, true},
};

void PrintTo(const NamedJsonCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const NamedStringCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const InvalidValueCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const ComponentCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const DiagnosticCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const NumericTargetCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const DiscreteTargetCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const ReferenceCase &value, std::ostream *stream)
{
    *stream << value.name;
}

template <typename Case>
std::string case_name(const testing::TestParamInfo<Case> &info)
{
    return info.param.name;
}

Json read_json(const std::filesystem::path &path)
{
    std::ifstream input(path);
    if (!input)
    {
        throw std::runtime_error("unable to open JSON fixture");
    }
    Json result;
    input >> result;
    return result;
}

timeline::Document import_clean_document(const std::filesystem::path &path)
{
    JsonImportResult result = import_timeline_json(path);
    if (!result.succeeded() || !result.diagnostics.empty())
    {
        throw std::runtime_error("catalog fixture import failed");
    }
    return std::move(*result.document);
}

timeline::Document combine_with_music(const timeline::Document &document)
{
    const timeline::Document music = import_clean_document("fixtures/beat-keys/rms.beat-keys.json");
    return timeline::combine_documents(document, music);
}

std::vector<std::string> read_parameter_values(const std::filesystem::path &path, std::string_view parameter)
{
    std::ifstream input(path);
    if (!input)
    {
        throw std::runtime_error("unable to open reference output");
    }
    const std::string prefix = std::string(parameter) + "=";
    std::vector<std::string> result;
    for (std::string line; std::getline(input, line);)
    {
        const std::size_t start = line.find_first_not_of(' ');
        if (start != std::string::npos && line.compare(start, prefix.size(), prefix) == 0)
        {
            result.push_back(line.substr(start + prefix.size()));
        }
    }
    return result;
}

std::vector<double> parse_components(std::string value)
{
    std::replace(value.begin(), value.end(), '/', ' ');
    std::istringstream input(value);
    std::vector<double> result;
    for (double component = 0; input >> component;)
    {
        result.push_back(component);
    }
    return result;
}

Json extra_catalog()
{
    return Json::parse(R"({
        "parameters": {
            "unused": {"type":"point2","description":"Unused alias","normalize":false},
            "choice": {"type":"enum","description":"Choice","values":["a","b"]},
            "function": {"type":"function-list","description":"Functions","values":"id-functions"}
        },
        "fractal-types": {"unused": {
            "params": {
                "slots": [{"index":0,"name":"slot","type":"double","description":"Slot"}],
                "groups": {"c":{"type":"complex","description":"C","slots":[0,1]}}
            },
            "functions": {"fn1":{"type":"enum","values":"id-functions","description":"Function"}}
        }},
        "formula-entries": {"unused": {"params":{"knobs":{
            "k":{"type":"real","description":"Knob","variable":"p1.real"},
            "c":{"type":"complex","description":"Complex knob","variable":"p2"}
        }}}}
    })");
}

Json source_type_entry(const NamedStringCase &definition)
{
    Json result{{"type", definition.value}, {"description", "Unused"}, {"format", "raw"},
        {"default-curve", "geometric"}, {"extrapolate", "base"}, {"normalize", true}, {"min", 10}, {"max", -10}};
    if (definition.value == "enum" || definition.value == "inside" || definition.value == "outside" ||
        definition.value == "integer-or-enum" || definition.value == "numeric-tuple-or-enum")
    {
        result["values"] = Json::array({"choice"});
    }
    else if (definition.value == "function-list")
    {
        result["values"] = "id-functions";
    }
    return result;
}

Json categorical_entry(const NamedStringCase &definition)
{
    Json result{{"type", definition.value}, {"description", "Discrete target"}};
    if (definition.value == "enum")
    {
        result["values"] = {"a", "b"};
    }
    else if (definition.value == "function-list")
    {
        result["values"] = "id-functions";
    }
    return result;
}

/// Imports modified catalogs through isolated files without altering shared fixtures.
///
class CatalogLoading : public testing::Test
{
protected:
    void SetUp() override
    {
        m_directory = std::filesystem::temp_directory_path() /
            ("timeline-catalog-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(m_directory);
        m_config = read_json("fixtures/catalog-audit.json");
        m_config["source"]["file"] = std::filesystem::absolute("fixtures/input/catalog-audit-source.par").string();
        m_catalog = read_json("fixtures/input/catalog-audit.json");
    }

    void TearDown() override
    {
        std::filesystem::remove_all(m_directory);
    }

    JsonImportResult import_catalogs(const std::vector<Json> &catalogs)
    {
        m_config["parameter-catalogs"] = Json::array();
        int index = 0;
        for (const Json &catalog : catalogs)
        {
            const std::string name = "catalog-" + std::to_string(index++) + ".json";
            std::ofstream(m_directory / name) << catalog.dump();
            m_config["parameter-catalogs"].push_back(name);
        }
        const std::filesystem::path config = m_directory / "animation.json";
        std::ofstream(config) << m_config.dump();
        return import_timeline_json(config);
    }

    JsonImportResult import_distinct_catalogs()
    {
        return import_catalogs({m_catalog, extra_catalog()});
    }

    std::filesystem::path m_directory;
    Json m_config;
    Json m_catalog;
};

/// Imports formula targets through both supported name forms.
///
class FormulaTargetResolution : public CatalogLoading
{
protected:
    void SetUp() override
    {
        CatalogLoading::SetUp();
        m_catalog = read_json("fixtures/input/target-resolution-catalog.json");
        m_config["source"] = {
            {"file", std::filesystem::absolute("fixtures/input/source.par").string()}, {"name", "Function_Demo"}};
        m_config["tracks"] = Json::parse(R"([
            {"parameter":"Larry.c","keys":[{"frame":0,"value":"1/2"},{"frame":4,"value":"3/4"}]},
            {"parameter":"Larry[\"amount\"]","keys":[{"frame":0,"value":1},{"frame":4,"value":5}]},
            {"parameter":"Larry.fn2","keys":[{"frame":0,"value":"cos"},{"frame":4,"value":"sin"}]},
            {"parameter":"Larry[\"fn2\"]","keys":[{"frame":0,"value":"cos"},{"frame":4,"value":"sin"}]}
        ])");
    }

    timeline::Document import_formula_document()
    {
        JsonImportResult result = import_catalogs({m_catalog});
        if (!result.succeeded() || !result.diagnostics.empty())
        {
            throw std::runtime_error("formula target import failed");
        }
        return std::move(*result.document);
    }
};

/// Exercises invalid animated values independently.
///
class InvalidCatalogValueTest : public CatalogLoading, public testing::WithParamInterface<InvalidValueCase>
{
};

/// Exercises named string catalog cases independently.
///
class UnsupportedCatalogTypeTest : public CatalogLoading, public testing::WithParamInterface<NamedStringCase>
{
};

/// Exercises malformed unused parameter metadata independently.
///
class MalformedCatalogMetadataTest : public CatalogLoading, public testing::WithParamInterface<NamedJsonCase>
{
};

/// Exercises malformed nested catalog metadata independently.
///
class NestedCatalogMetadataTest : public CatalogLoading, public testing::WithParamInterface<NamedJsonCase>
{
};

/// Exercises duplicate names in catalog sections independently.
///
class DuplicateCatalogSectionTest : public CatalogLoading, public testing::WithParamInterface<NamedStringCase>
{
};

/// Exercises malformed catalog roots independently.
///
class CatalogRootTest : public CatalogLoading, public testing::WithParamInterface<NamedJsonCase>
{
};

/// Exercises accepted unused source types independently.
///
class CatalogSourceTypeTest : public CatalogLoading, public testing::WithParamInterface<NamedStringCase>
{
};

/// Exercises required categorical defaults independently.
///
class CategoricalDefaultTest : public CatalogLoading, public testing::WithParamInterface<NamedStringCase>
{
};

/// Exercises one imported component lane at a time.
///
class CatalogComponentTest : public testing::TestWithParam<ComponentCase>
{
};

/// Exercises one invalid source track diagnostic at a time.
///
class CatalogSourceDiagnosticTest : public testing::TestWithParam<DiagnosticCase>
{
};

/// Compares one numeric target with the reference output at a time.
///
class NumericTargetComparisonTest : public testing::TestWithParam<NumericTargetCase>
{
};

/// Compares one discrete target with the reference output at a time.
///
class DiscreteTargetComparisonTest : public testing::TestWithParam<DiscreteTargetCase>
{
};

/// Compares one catalog parameter with the reference output at a time.
///
class CatalogReferenceTest : public testing::TestWithParam<ReferenceCase>
{
};

} // namespace

TEST_F(CatalogLoading, diagnosesUnknownTarget)
{
    m_config["tracks"][1]["parameter"] = "undeclared";

    const JsonImportResult result = import_catalogs({m_catalog});

    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(1, timeline::size_cast(result.diagnostics));
    EXPECT_NE(std::string::npos, result.diagnostics[0].find("animation-1"));
    EXPECT_NE(std::string::npos, result.diagnostics[0].find("Unknown animated parameter"));
}

TEST_F(CatalogLoading, retainsValidTracksAfterUnknownTarget)
{
    m_config["tracks"][1]["parameter"] = "undeclared";

    const JsonImportResult result = import_catalogs({m_catalog});

    ASSERT_TRUE(result.document);
    EXPECT_EQ(6, result.document->lane_count());
}

TEST_F(FormulaTargetResolution, importsExpectedDocumentShape)
{
    const timeline::Document document = import_formula_document();

    EXPECT_EQ(5, document.lane_count());
}

TEST_F(FormulaTargetResolution, resolvesFormulaGroupByName)
{
    const timeline::Document document = import_formula_document();
    const timeline::FrameInspection inspection = *timeline::inspect_frame(document, 1);

    const ResolvedAttributes attributes = resolved_attributes(document, inspection.lanes[0].items[0].attributes);

    EXPECT_DOUBLE_EQ(1.5, *inspection.lanes[0].value);
    EXPECT_EQ("params", attributes.at("output-parameter"));
    EXPECT_EQ("[0,1]", attributes.at("slots"));
}

TEST_F(FormulaTargetResolution, resolvesFormulaKnobCatalogByBracketName)
{
    const timeline::Document document = import_formula_document();
    const timeline::FrameInspection inspection = *timeline::inspect_frame(document, 1);
    const ResolvedAttributes attributes = resolved_attributes(document, inspection.lanes[2].items[0].attributes);

    const Json catalog = Json::parse(attributes.at("catalog-definition"));

    EXPECT_EQ("double", catalog.at("type"));
}

TEST_F(FormulaTargetResolution, preservesFormulaKnobSourceDefinition)
{
    const timeline::Document document = import_formula_document();
    const timeline::FrameInspection inspection = *timeline::inspect_frame(document, 1);
    const ResolvedAttributes attributes = resolved_attributes(document, inspection.lanes[2].items[0].attributes);

    const Json source = Json::parse(attributes.at("catalog-source-definition"));

    EXPECT_EQ("real", source.at("type"));
}

TEST_F(FormulaTargetResolution, resolvesFormulaFunctionByDotName)
{
    const timeline::Document document = import_formula_document();
    const timeline::FrameInspection inspection = *timeline::inspect_frame(document, 1);

    const std::string_view value = resolved_attributes(document, inspection.lanes[3].items[0].attributes).at("value");

    EXPECT_EQ("sin/cos", value);
}

TEST_F(FormulaTargetResolution, resolvesFormulaFunctionByBracketName)
{
    const timeline::Document document = import_formula_document();
    const timeline::FrameInspection inspection = *timeline::inspect_frame(document, 1);

    const std::string_view value = resolved_attributes(document, inspection.lanes[4].items[0].attributes).at("value");

    EXPECT_EQ("sin/cos", value);
}

TEST(TargetResolution, importsExpectedDocumentShape)
{
    const timeline::Document document = import_clean_document("fixtures/target-resolution.json");

    EXPECT_EQ(17, document.lane_count());
}

TEST(TargetResolution, preservesGroupOutputMetadata)
{
    const timeline::Document document = import_clean_document("fixtures/target-resolution.json");
    const timeline::FrameInspection inspection = *timeline::inspect_frame(document, 1);

    const ResolvedAttributes attributes = resolved_attributes(document, inspection.lanes[1].items[0].attributes);

    EXPECT_EQ("params", attributes.at("output-parameter"));
    EXPECT_EQ("[0,1]", attributes.at("slots"));
}

TEST(TargetResolution, ownsCatalogDefinition)
{
    const timeline::Document document = import_clean_document("fixtures/target-resolution.json");
    const timeline::FrameInspection inspection = *timeline::inspect_frame(document, 1);

    const Json definition =
        Json::parse(resolved_attributes(document, inspection.lanes[1].items[0].attributes).at("catalog-definition"));

    EXPECT_EQ("complex", definition.at("type"));
}

TEST(TargetResolution, ownsTrackDefinition)
{
    const timeline::Document document = import_clean_document("fixtures/target-resolution.json");
    const timeline::FrameInspection inspection = *timeline::inspect_frame(document, 1);

    const Json definition =
        Json::parse(resolved_attributes(document, inspection.lanes[1].items[0].attributes).at("track-definition"));

    EXPECT_EQ("params.c", definition.at("parameter"));
}

TEST_F(CatalogLoading, requiresDefaultCurveEvenWithExplicitKeyCurve)
{
    m_catalog["parameters"]["scalar"].erase("default-curve");
    m_config["tracks"][3]["keys"][1]["curve"] = "linear";

    const JsonImportResult result = import_catalogs({m_catalog});

    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(1, timeline::size_cast(result.diagnostics));
    EXPECT_NE(std::string::npos, result.diagnostics[0].find("default-curve"));
}

TEST_F(CatalogLoading, requiresSourceValueForAnimatedParameter)
{
    m_catalog["parameters"]["absent"] = m_catalog["parameters"]["maxiter"];
    m_config["tracks"][4]["parameter"] = "absent";

    const JsonImportResult result = import_catalogs({m_catalog});

    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(1, timeline::size_cast(result.diagnostics));
    EXPECT_NE(std::string::npos, result.diagnostics[0].find("source parameter"));
}

TEST_P(CategoricalDefaultTest, requiresDefaultCurve)
{
    const NamedStringCase &definition = GetParam();
    m_catalog["parameters"]["scalar"] = categorical_entry(definition);

    const JsonImportResult result = import_catalogs({m_catalog});

    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(1, timeline::size_cast(result.diagnostics));
    EXPECT_NE(std::string::npos, result.diagnostics[0].find("default-curve"));
}

INSTANTIATE_TEST_SUITE_P(
    CategoricalTypes, CategoricalDefaultTest, testing::ValuesIn(CATEGORICAL_TYPE_CASES), case_name<NamedStringCase>);

TEST_F(CatalogLoading, requiresOrdinaryFunctionSource)
{
    m_catalog["parameters"]["function"] = {
        {"type", "function-list"}, {"description", "Functions"}, {"values", "id-functions"}, {"default-curve", "hold"}};
    m_config["tracks"][3]["parameter"] = "function";

    const JsonImportResult result = import_catalogs({m_catalog});

    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(1, timeline::size_cast(result.diagnostics));
    EXPECT_NE(std::string::npos, result.diagnostics[0].find("source parameter"));
}

TEST_F(CatalogLoading, suppliesCenterMagnificationOptionalFields)
{
    m_catalog["parameters"]["scalar"] = {{"type", "center-mag"}, {"description", "Center and magnification"}};
    m_config["tracks"][3]["keys"][0]["value"] = "0/0/1";
    m_config["tracks"][3]["keys"][1]["value"] = "4/8/16/0/90/45";

    const JsonImportResult result = import_catalogs({m_catalog});
    const timeline::FrameInspection inspection = *timeline::inspect_frame(*result.document, 1);

    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.diagnostics.empty());
    EXPECT_EQ(14, result.document->lane_count());
    EXPECT_NEAR(2, *inspection.lanes[9].value, 1e-12);
    EXPECT_DOUBLE_EQ(1, *inspection.lanes[10].value);
    EXPECT_DOUBLE_EQ(22.5, *inspection.lanes[11].value);
}

TEST_F(CatalogLoading, preservesAuthoredCenterMagnificationValue)
{
    m_catalog["parameters"]["scalar"] = {{"type", "center-mag"}, {"description", "Center and magnification"}};
    m_config["tracks"][3]["keys"][0]["value"] = "0/0/1";
    m_config["tracks"][3]["keys"][1]["value"] = "4/8/16/0/90/45";

    const JsonImportResult result = import_catalogs({m_catalog});
    const timeline::FrameInspection inspection = *timeline::inspect_frame(*result.document, 1);

    ASSERT_TRUE(result.succeeded());
    EXPECT_EQ("0/0/1", resolved_attributes(result, inspection.lanes[9].items[0].attributes).at("value"));
}

TEST(TargetResolution, composesWithMusicDocument)
{
    const timeline::Document document = import_clean_document("fixtures/target-resolution.json");

    const timeline::Document comparison = combine_with_music(document);

    EXPECT_EQ(21, comparison.lane_count());
}

TEST_P(NumericTargetComparisonTest, evaluatesAtFrameOne)
{
    const NumericTargetCase &definition = GetParam();
    const timeline::Document document = import_clean_document("fixtures/target-resolution.json");

    const timeline::FrameInspection inspection = *timeline::inspect_frame(document, 1);

    ASSERT_EQ(timeline::size_cast(definition.lanes), timeline::size_cast(definition.frame_one));
    for (int component = 0; component < timeline::size_cast(definition.frame_one); ++component)
    {
        EXPECT_DOUBLE_EQ(definition.frame_one[component], *inspection.lanes[definition.lanes[component]].value);
    }
}

TEST_P(NumericTargetComparisonTest, matchesParAnimatorAfterComposition)
{
    const NumericTargetCase &definition = GetParam();
    const timeline::Document document = import_clean_document("fixtures/target-resolution.json");
    const timeline::Document comparison = combine_with_music(document);
    const std::vector<std::string> expected =
        read_parameter_values("fixtures/gold-target-resolution.par", definition.parameter);

    ASSERT_EQ(5, timeline::size_cast(expected));
    for (int frame = 0; frame < timeline::size_cast(expected); ++frame)
    {
        const std::vector<double> components = parse_components(expected[frame]);
        const timeline::FrameInspection inspection = *timeline::inspect_frame(comparison, frame);
        ASSERT_EQ(timeline::size_cast(definition.lanes), timeline::size_cast(components));
        for (int component = 0; component < timeline::size_cast(components); ++component)
        {
            EXPECT_NEAR(components[component], *inspection.lanes[definition.lanes[component]].value, 1e-10);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
    NumericTargets, NumericTargetComparisonTest, testing::ValuesIn(NUMERIC_TARGET_CASES), case_name<NumericTargetCase>);

TEST_P(DiscreteTargetComparisonTest, evaluatesAtFrameOne)
{
    const DiscreteTargetCase &definition = GetParam();
    const timeline::Document document = import_clean_document("fixtures/target-resolution.json");
    const timeline::FrameInspection inspection = *timeline::inspect_frame(document, 1);

    const ResolvedAttributes attributes =
        resolved_attributes(document, inspection.lanes[definition.lane].items[0].attributes);

    EXPECT_EQ(definition.frame_one, attributes.at("value"));
}

TEST_P(DiscreteTargetComparisonTest, matchesParAnimatorAfterComposition)
{
    const DiscreteTargetCase &definition = GetParam();
    const timeline::Document document = import_clean_document("fixtures/target-resolution.json");
    const timeline::Document comparison = combine_with_music(document);
    const std::vector<std::string> expected =
        read_parameter_values("fixtures/gold-target-resolution.par", definition.parameter);

    ASSERT_EQ(5, timeline::size_cast(expected));
    for (int frame = 0; frame < timeline::size_cast(expected); ++frame)
    {
        const timeline::FrameInspection inspection = *timeline::inspect_frame(comparison, frame);
        const ResolvedAttributes attributes =
            resolved_attributes(comparison, inspection.lanes[definition.lane].items[0].attributes);
        EXPECT_EQ(expected[frame], attributes.at("value"));
    }
}

INSTANTIATE_TEST_SUITE_P(DiscreteTargets, DiscreteTargetComparisonTest, testing::ValuesIn(DISCRETE_TARGET_CASES),
    case_name<DiscreteTargetCase>);

TEST(TargetResolution, hitTestRetainsComparisonIdentity)
{
    const timeline::Document document = import_clean_document("fixtures/target-resolution.json");
    const timeline::Document comparison = combine_with_music(document);
    const timeline::Layout layout(comparison,
        timeline::Viewport(900, 1100, document.frame_grid()->offset(), document.frame_grid()->end_time()),
        timeline::LayoutMetrics(140, 20, 40, 4));
    std::optional<timeline::HitResult> result;
    std::optional<timeline::DisplayId> expected;
    for (const timeline::Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<timeline::Polyline>(primitive))
        {
            const timeline::Polyline &line = std::get<timeline::Polyline>(primitive);
            if (layout.display_list().strings().lookup(line.id.lane_id) == "animation-1[0]")
            {
                result = layout.hit_test(line.points.front(), 0);
                expected = line.id;
                break;
            }
        }
    }

    ASSERT_TRUE(result);
    ASSERT_TRUE(expected);
    EXPECT_EQ(expected->lane_id, result->id.lane_id);
    EXPECT_EQ(expected->item_id, result->id.item_id);
}

TEST(TargetResolution, rejectsInvalidFixture)
{
    const JsonImportResult result = import_timeline_json("fixtures/target-resolution-invalid.json");

    EXPECT_FALSE(result.succeeded());
}

TEST_P(InvalidCatalogValueTest, diagnosesInvalidAnimatedValue)
{
    const InvalidValueCase &definition = GetParam();
    m_catalog["parameters"]["scalar"] = definition.metadata;
    m_catalog["parameters"]["scalar"]["description"] = "Test target";
    m_config["tracks"][3]["keys"][0]["value"] = definition.value;
    m_config["tracks"][3]["keys"][1]["value"] = definition.value;

    const JsonImportResult result = import_catalogs({m_catalog});

    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(1, timeline::size_cast(result.diagnostics));
    EXPECT_NE(std::string::npos, result.diagnostics[0].find("animation-3"));
}

TEST_P(InvalidCatalogValueTest, omitsPartialComponentLanes)
{
    const InvalidValueCase &definition = GetParam();
    m_catalog["parameters"]["scalar"] = definition.metadata;
    m_catalog["parameters"]["scalar"]["description"] = "Test target";
    m_config["tracks"][3]["keys"][0]["value"] = definition.value;
    m_config["tracks"][3]["keys"][1]["value"] = definition.value;

    const JsonImportResult result = import_catalogs({m_catalog});

    ASSERT_TRUE(result.document);
    EXPECT_EQ(8, result.document->lane_count());
}

INSTANTIATE_TEST_SUITE_P(
    InvalidValues, InvalidCatalogValueTest, testing::ValuesIn(INVALID_VALUE_CASES), case_name<InvalidValueCase>);

TEST_P(UnsupportedCatalogTypeTest, diagnosesUnsupportedGenericAnimation)
{
    const NamedStringCase &definition = GetParam();
    Json metadata{{"type", definition.value}, {"description", "Unsupported target"}, {"default-curve", "hold"}};
    if (definition.value == "numeric-tuple-or-enum")
    {
        metadata["values"] = {"off"};
    }
    m_catalog["parameters"]["scalar"] = metadata;

    const JsonImportResult result = import_catalogs({m_catalog});

    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(1, timeline::size_cast(result.diagnostics));
    EXPECT_NE(std::string::npos, result.diagnostics[0].find("unsupported"));
}

TEST_P(UnsupportedCatalogTypeTest, omitsUnsupportedComponentLanes)
{
    const NamedStringCase &definition = GetParam();
    Json metadata{{"type", definition.value}, {"description", "Unsupported target"}, {"default-curve", "hold"}};
    if (definition.value == "numeric-tuple-or-enum")
    {
        metadata["values"] = {"off"};
    }
    m_catalog["parameters"]["scalar"] = metadata;

    const JsonImportResult result = import_catalogs({m_catalog});

    ASSERT_TRUE(result.document);
    EXPECT_EQ(8, result.document->lane_count());
}

INSTANTIATE_TEST_SUITE_P(UnsupportedTypes, UnsupportedCatalogTypeTest, testing::ValuesIn(UNSUPPORTED_TYPE_CASES),
    case_name<NamedStringCase>);

TEST_F(CatalogLoading, diagnosesComplexComponentBounds)
{
    m_catalog["parameters"]["scalar"] = {
        {"type", "complex"}, {"description", "Complex"}, {"default-curve", "linear"}, {"max", 5}};
    m_config["tracks"][3]["keys"][0]["value"] = "1/2";
    m_config["tracks"][3]["keys"][1]["value"] = "3/6";
    m_catalog["fractal-types"]["mandel"]["params"]["groups"]["c"] = m_catalog["parameters"]["scalar"];
    m_catalog["fractal-types"]["mandel"]["params"]["groups"]["c"]["slots"] = {0, 1};
    m_config["tracks"][3]["parameter"] = "params.c";

    const JsonImportResult result = import_catalogs({m_catalog});

    ASSERT_TRUE(result.succeeded());
    EXPECT_EQ(1, timeline::size_cast(result.diagnostics));
}

TEST_F(CatalogLoading, omitsComplexLanesAfterBoundsFailure)
{
    m_catalog["parameters"]["scalar"] = {
        {"type", "complex"}, {"description", "Complex"}, {"default-curve", "linear"}, {"max", 5}};
    m_config["tracks"][3]["keys"][0]["value"] = "1/2";
    m_config["tracks"][3]["keys"][1]["value"] = "3/6";
    m_catalog["fractal-types"]["mandel"]["params"]["groups"]["c"] = m_catalog["parameters"]["scalar"];
    m_catalog["fractal-types"]["mandel"]["params"]["groups"]["c"]["slots"] = {0, 1};
    m_config["tracks"][3]["parameter"] = "params.c";

    const JsonImportResult result = import_catalogs({m_catalog});

    ASSERT_TRUE(result.document);
    EXPECT_EQ(8, result.document->lane_count());
}

TEST_P(MalformedCatalogMetadataTest, rejectsUnusedParameterDefinition)
{
    const NamedJsonCase &definition = GetParam();
    Json catalog = m_catalog;
    catalog["parameters"]["unused"] = definition.value;

    const JsonImportResult result = import_catalogs({catalog});

    EXPECT_FALSE(result.succeeded());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_NE(std::string::npos, result.diagnostics.front().find("catalog"));
}

INSTANTIATE_TEST_SUITE_P(MalformedUnusedMetadata, MalformedCatalogMetadataTest,
    testing::ValuesIn(MALFORMED_METADATA_CASES), case_name<NamedJsonCase>);

TEST_P(NestedCatalogMetadataTest, rejectsUnusedNestedDefinition)
{
    const NamedJsonCase &definition = GetParam();
    Json catalog = m_catalog;
    catalog.update(definition.value);

    const JsonImportResult result = import_catalogs({catalog});

    EXPECT_FALSE(result.succeeded());
    EXPECT_FALSE(result.diagnostics.empty());
}

INSTANTIATE_TEST_SUITE_P(MalformedNestedMetadata, NestedCatalogMetadataTest, testing::ValuesIn(NESTED_METADATA_CASES),
    case_name<NamedJsonCase>);

TEST_F(CatalogLoading, rejectsEmptyCatalogList)
{
    const JsonImportResult result = import_catalogs({});

    EXPECT_FALSE(result.succeeded());
}

TEST_P(DuplicateCatalogSectionTest, rejectsDuplicateNames)
{
    const NamedStringCase &definition = GetParam();
    Json first = m_catalog;
    if (definition.value != "parameters")
    {
        first[definition.value]["unused"] = Json::object();
    }
    Json second{{"parameters", Json::object()}};
    second[definition.value] = first.at(definition.value);

    const JsonImportResult result = import_catalogs({first, second});

    EXPECT_FALSE(result.succeeded());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_NE(std::string::npos, result.diagnostics.front().find("Duplicate"));
}

INSTANTIATE_TEST_SUITE_P(
    NamedSections, DuplicateCatalogSectionTest, testing::ValuesIn(DUPLICATE_SECTION_CASES), case_name<NamedStringCase>);

TEST_F(CatalogLoading, composesDistinctValidCatalogs)
{
    const JsonImportResult result = import_distinct_catalogs();

    ASSERT_TRUE(result.succeeded());
    EXPECT_TRUE(result.diagnostics.empty());
    ASSERT_TRUE(result.document);
    EXPECT_EQ(9, result.document->lane_count());
}

TEST_F(CatalogLoading, preservesMetadataFromFirstCatalogAfterComposition)
{
    const JsonImportResult result = import_distinct_catalogs();
    const timeline::Document document = *result.document;
    const timeline::FrameInspection inspection = *timeline::inspect_frame(document, 1);

    const Json metadata =
        Json::parse(resolved_attributes(document, inspection.lanes[0].items[0].attributes).at("catalog-definition"));

    EXPECT_EQ(m_catalog.at("parameters").at("position"), metadata);
}

TEST_F(CatalogLoading, evaluatesFirstCatalogAfterComposition)
{
    const JsonImportResult result = import_distinct_catalogs();

    const timeline::FrameInspection inspection = *timeline::inspect_frame(*result.document, 1);

    EXPECT_DOUBLE_EQ(2, *inspection.lanes[0].value);
}

TEST_P(CatalogRootTest, rejectsInvalidRoot)
{
    const NamedJsonCase &definition = GetParam();

    const JsonImportResult result = import_catalogs({definition.value});

    EXPECT_FALSE(result.succeeded());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_NE(std::string::npos, result.diagnostics.front().find("catalog"));
}

INSTANTIATE_TEST_SUITE_P(
    InvalidRoots, CatalogRootTest, testing::ValuesIn(CATALOG_ROOT_CASES), case_name<NamedJsonCase>);

TEST_P(CatalogSourceTypeTest, acceptsUnusedMetadataWithoutEvaluatingIt)
{
    const NamedStringCase &definition = GetParam();
    Json catalog = m_catalog;
    catalog["parameters"]["unused"] = source_type_entry(definition);

    const JsonImportResult result = import_catalogs({catalog});

    EXPECT_TRUE(result.succeeded());
    EXPECT_TRUE(result.diagnostics.empty());
}

INSTANTIATE_TEST_SUITE_P(
    SourceTypes, CatalogSourceTypeTest, testing::ValuesIn(SOURCE_TYPE_CASES), case_name<NamedStringCase>);

TEST(CatalogCompatibility, importsValidCatalogComposition)
{
    const JsonImportResult result = import_timeline_json("fixtures/catalog-loading.json");

    ASSERT_TRUE(result.succeeded());
    EXPECT_TRUE(result.diagnostics.empty());
    ASSERT_TRUE(result.document);
    EXPECT_EQ(9, result.document->lane_count());
}

TEST(CatalogCompatibility, evaluatesValidCatalogComposition)
{
    const timeline::Document document = import_clean_document("fixtures/catalog-loading.json");

    const timeline::FrameInspection inspection = *timeline::inspect_frame(document, 1);

    EXPECT_DOUBLE_EQ(2, *inspection.lanes[0].value);
}

TEST(CatalogCompatibility, rejectsMalformedUnusedMetadata)
{
    const JsonImportResult result = import_timeline_json("fixtures/catalog-loading-invalid.json");

    EXPECT_FALSE(result.succeeded());
    ASSERT_EQ(1, timeline::size_cast(result.diagnostics));
    EXPECT_NE(std::string::npos, result.diagnostics[0].find("unused"));
    EXPECT_NE(std::string::npos, result.diagnostics[0].find("description"));
}

TEST(CatalogCompatibility, ownsCatalogDefinitionAfterImportResultRelease)
{
    JsonImportResult imported = import_timeline_json("fixtures/catalog-loading.json");
    ASSERT_TRUE(imported.succeeded());
    const timeline::Document document = *imported.document;
    imported.document.reset();

    const timeline::FrameInspection inspection = *timeline::inspect_frame(document, 1);
    const ResolvedAttributes attributes = resolved_attributes(document, inspection.lanes[0].items[0].attributes);

    EXPECT_TRUE(Json::parse(attributes.at("catalog-definition")).contains("type"));
}

TEST(CatalogCompatibility, ownsTrackDefinitionAfterImportResultRelease)
{
    JsonImportResult imported = import_timeline_json("fixtures/catalog-loading.json");
    ASSERT_TRUE(imported.succeeded());
    const timeline::Document document = *imported.document;
    imported.document.reset();

    const timeline::FrameInspection inspection = *timeline::inspect_frame(document, 1);
    const ResolvedAttributes attributes = resolved_attributes(document, inspection.lanes[0].items[0].attributes);

    const Json track = Json::parse(attributes.at("track-definition"));

    EXPECT_EQ(attributes.at("parameter"), track.at("parameter").get<std::string>());
}

TEST_P(CatalogComponentTest, evaluatesAtFrameOne)
{
    const ComponentCase &definition = GetParam();
    const timeline::Document document = import_clean_document("fixtures/catalog-loading.json");

    const timeline::FrameInspection inspection = *timeline::inspect_frame(document, 1);

    EXPECT_DOUBLE_EQ(definition.value, *inspection.lanes[definition.lane].value);
}

TEST_P(CatalogComponentTest, preservesComponentMetadata)
{
    const ComponentCase &definition = GetParam();
    const timeline::Document document = import_clean_document("fixtures/catalog-loading.json");
    const timeline::FrameInspection inspection = *timeline::inspect_frame(document, 1);

    const ResolvedAttributes attributes =
        resolved_attributes(document, inspection.lanes[definition.lane].items[0].attributes);

    EXPECT_EQ(std::to_string(definition.component), attributes.at("component"));
    EXPECT_EQ(std::to_string(definition.arity), attributes.at("arity"));
}

TEST_P(CatalogComponentTest, retainsOwnedCatalogDefinition)
{
    const ComponentCase &definition = GetParam();
    const timeline::Document document = import_clean_document("fixtures/catalog-loading.json");
    const timeline::FrameInspection inspection = *timeline::inspect_frame(document, 1);
    const ResolvedAttributes attributes =
        resolved_attributes(document, inspection.lanes[definition.lane].items[0].attributes);

    const Json catalog = Json::parse(attributes.at("catalog-definition"));

    EXPECT_TRUE(catalog.contains("type"));
}

TEST_P(CatalogComponentTest, retainsOwnedTrackDefinition)
{
    const ComponentCase &definition = GetParam();
    const timeline::Document document = import_clean_document("fixtures/catalog-loading.json");
    const timeline::FrameInspection inspection = *timeline::inspect_frame(document, 1);
    const ResolvedAttributes attributes =
        resolved_attributes(document, inspection.lanes[definition.lane].items[0].attributes);

    const Json track = Json::parse(attributes.at("track-definition"));

    EXPECT_EQ(attributes.at("parameter"), track.at("parameter").get<std::string>());
}

INSTANTIATE_TEST_SUITE_P(
    Components, CatalogComponentTest, testing::ValuesIn(COMPONENT_CASES), case_name<ComponentCase>);

TEST(CatalogCompatibility, exposesRoundedIntegerOutput)
{
    const timeline::Document document = import_clean_document("fixtures/catalog-loading.json");

    const timeline::FrameInspection inspection = *timeline::inspect_frame(document, 1);

    EXPECT_DOUBLE_EQ(101, *inspection.lanes[8].output_value);
}

TEST(CatalogCompatibility, omitsOutputForOrdinaryComponent)
{
    const timeline::Document document = import_clean_document("fixtures/catalog-loading.json");

    const timeline::FrameInspection inspection = *timeline::inspect_frame(document, 1);

    EXPECT_FALSE(inspection.lanes[0].output_value);
}

TEST(CatalogCompatibility, composesWithMusicDocument)
{
    const timeline::Document document = import_clean_document("fixtures/catalog-loading.json");

    const timeline::Document combined = combine_with_music(document);

    EXPECT_EQ(13, combined.lane_count());
}

TEST(CatalogCompatibility, preservesValuesAfterDocumentComposition)
{
    const timeline::Document document = import_clean_document("fixtures/catalog-loading.json");
    const timeline::Document combined = combine_with_music(document);

    const timeline::FrameInspection inspection = *timeline::inspect_frame(combined, 1);

    EXPECT_DOUBLE_EQ(4, *inspection.lanes[1].value);
}

TEST(CatalogCompatibility, hitTestRetainsImportedIdentity)
{
    const timeline::Document document = import_clean_document("fixtures/catalog-loading.json");
    const timeline::Document combined = combine_with_music(document);
    const timeline::Layout layout(combined,
        timeline::Viewport(600, 400, document.frame_grid()->offset(), document.frame_grid()->end_time()),
        timeline::LayoutMetrics(100, 20, 40, 4));
    std::optional<timeline::HitResult> result;
    std::optional<timeline::DisplayId> expected;
    for (const timeline::Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<timeline::Polyline>(primitive))
        {
            const timeline::Polyline &line = std::get<timeline::Polyline>(primitive);
            if (layout.display_list().strings().lookup(line.id.lane_id) == "animation-0[0]")
            {
                result = layout.hit_test(line.points.front(), 0);
                expected = line.id;
                break;
            }
        }
    }

    ASSERT_TRUE(result);
    ASSERT_TRUE(expected);
    EXPECT_EQ(expected->lane_id, result->id.lane_id);
    EXPECT_EQ(expected->item_id, result->id.item_id);
}

TEST(CatalogCompatibility, reportsEveryInvalidSourceTrack)
{
    const JsonImportResult result = import_timeline_json("fixtures/catalog-audit-invalid.json");

    EXPECT_FALSE(result.succeeded());
    EXPECT_EQ(16, timeline::size_cast(result.diagnostics));
}

TEST_P(CatalogSourceDiagnosticTest, identifiesInvalidSourceTrack)
{
    const DiagnosticCase &definition = GetParam();

    const JsonImportResult result = import_timeline_json("fixtures/catalog-audit-invalid.json");

    ASSERT_GT(timeline::size_cast(result.diagnostics), definition.index);
    EXPECT_NE(std::string::npos,
        result.diagnostics[definition.index].find("animation-" + std::to_string(definition.index) + ":"));
    EXPECT_NE(std::string::npos, result.diagnostics[definition.index].find(definition.message));
}

INSTANTIATE_TEST_SUITE_P(SourceTrackFailures, CatalogSourceDiagnosticTest, testing::ValuesIn(SOURCE_DIAGNOSTIC_CASES),
    case_name<DiagnosticCase>);

TEST_P(CatalogReferenceTest, matchesParAnimatorAtEveryFrame)
{
    const ReferenceCase &definition = GetParam();
    const timeline::Document document = import_clean_document("fixtures/catalog-loading.json");
    const std::vector<std::string> expected =
        read_parameter_values("fixtures/gold-catalog-audit.par", definition.parameter);

    ASSERT_EQ(5, timeline::size_cast(expected));
    for (int frame = 0; frame < timeline::size_cast(expected); ++frame)
    {
        const std::vector<double> components = parse_components(expected[frame]);
        const timeline::FrameInspection inspection = *timeline::inspect_frame(document, frame);
        for (int component = 0; component < timeline::size_cast(components); ++component)
        {
            const double actual = definition.output ? *inspection.lanes[definition.first_lane + component].output_value
                                                    : *inspection.lanes[definition.first_lane + component].value;
            EXPECT_DOUBLE_EQ(components[component], actual);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
    ReferenceParameters, CatalogReferenceTest, testing::ValuesIn(REFERENCE_CASES), case_name<ReferenceCase>);
