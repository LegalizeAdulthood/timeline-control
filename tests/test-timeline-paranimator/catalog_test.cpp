// Copyright (c) 2026 Richard Thomson

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
#include <iterator>
#include <map>
#include <sstream>

using namespace timeline_par_animator;

namespace
{

using Json = nlohmann::json;

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
        std::ifstream input("fixtures/catalog-audit.json");
        input >> m_config;
        m_config["source"]["file"] = std::filesystem::absolute("fixtures/input/catalog-audit-source.par").string();
        std::ifstream catalog("fixtures/input/catalog-audit.json");
        catalog >> m_catalog;
    }

    void TearDown() override
    {
        std::filesystem::remove_all(m_directory);
    }

    JsonImportResult import(const std::vector<Json> &catalogs)
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

    std::filesystem::path m_directory;
    Json m_config;
    Json m_catalog;
};

} // namespace

TEST_F(CatalogLoading, rejectsUnknownTargetsWithoutDiscardingValidTracks)
{
    m_config["tracks"][1]["parameter"] = "undeclared";
    const JsonImportResult result = import({m_catalog});
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(1, timeline::size_cast(result.diagnostics));
    EXPECT_NE(std::string::npos, result.diagnostics[0].find("animation-1"));
    EXPECT_NE(std::string::npos, result.diagnostics[0].find("Unknown animated parameter"));
    EXPECT_EQ(6, result.document->lane_count());
}

TEST_F(CatalogLoading, resolvesFormulaKnobsAndFunctionsInBothNameForms)
{
    std::ifstream catalog("fixtures/input/target-resolution-catalog.json");
    catalog >> m_catalog;
    m_config["source"] = {
        {"file", std::filesystem::absolute("fixtures/input/source.par").string()}, {"name", "Function_Demo"}};
    m_config["tracks"] = Json::parse(R"([
        {"parameter":"Larry.c","keys":[{"frame":0,"value":"1/2"},{"frame":4,"value":"3/4"}]},
        {"parameter":"Larry[\"amount\"]","keys":[{"frame":0,"value":1},{"frame":4,"value":5}]},
        {"parameter":"Larry.fn2","keys":[{"frame":0,"value":"cos"},{"frame":4,"value":"sin"}]},
        {"parameter":"Larry[\"fn2\"]","keys":[{"frame":0,"value":"cos"},{"frame":4,"value":"sin"}]}
    ])");
    const JsonImportResult result = import({m_catalog});
    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.diagnostics.empty()) << result.diagnostics.front();
    ASSERT_EQ(5, result.document->lane_count());
    const timeline::Document copy = *result.document;
    const timeline::FrameInspection inspection = *timeline::inspect_frame(copy, 1);
    EXPECT_DOUBLE_EQ(1.5, *inspection.lanes[0].value);
    EXPECT_EQ("params", inspection.lanes[0].items[0].attributes.at("output-parameter"));
    EXPECT_EQ("[0,1]", inspection.lanes[0].items[0].attributes.at("slots"));
    EXPECT_EQ("double", Json::parse(inspection.lanes[2].items[0].attributes.at("catalog-definition")).at("type"));
    EXPECT_EQ("sin/cos", inspection.lanes[3].items[0].attributes.at("value"));
    EXPECT_EQ("real", Json::parse(inspection.lanes[2].items[0].attributes.at("catalog-source-definition")).at("type"));
    EXPECT_EQ("sin/cos", inspection.lanes[4].items[0].attributes.at("value"));
}

TEST(TargetResolution, preservesDeclaredTargetsOwnedMetadataAndInspection)
{
    const JsonImportResult result = import_timeline_json("fixtures/target-resolution.json");
    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.diagnostics.empty()) << result.diagnostics.front();
    ASSERT_EQ(17, result.document->lane_count());
    const timeline::Document copy = *result.document;
    const timeline::FrameInspection inspection = *timeline::inspect_frame(copy, 1);
    EXPECT_DOUBLE_EQ(101, *inspection.lanes[0].value);
    EXPECT_DOUBLE_EQ(1.5, *inspection.lanes[1].value);
    EXPECT_EQ("sin/sin", inspection.lanes[4].items[0].attributes.at("value"));
    EXPECT_EQ("1", inspection.lanes[5].items[0].attributes.at("value"));
    EXPECT_EQ("no", inspection.lanes[15].items[0].attributes.at("value"));
    EXPECT_EQ("sin/cos", inspection.lanes[16].items[0].attributes.at("value"));
    EXPECT_EQ("params", inspection.lanes[1].items[0].attributes.at("output-parameter"));
    EXPECT_EQ("[0,1]", inspection.lanes[1].items[0].attributes.at("slots"));
    EXPECT_EQ("complex", Json::parse(inspection.lanes[1].items[0].attributes.at("catalog-definition")).at("type"));
    EXPECT_EQ("params.c", Json::parse(inspection.lanes[1].items[0].attributes.at("track-definition")).at("parameter"));
}

TEST_F(CatalogLoading, requiresSourceValuesAndDefaultsEvenWithExplicitCurves)
{
    m_catalog["parameters"]["scalar"].erase("default-curve");
    m_config["tracks"][3]["keys"][1]["curve"] = "linear";
    m_catalog["parameters"]["absent"] = m_catalog["parameters"]["maxiter"];
    m_config["tracks"][4]["parameter"] = "absent";
    const JsonImportResult result = import({m_catalog});
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(2, timeline::size_cast(result.diagnostics));
    EXPECT_NE(std::string::npos, result.diagnostics[0].find("default-curve"));
    EXPECT_NE(std::string::npos, result.diagnostics[1].find("source parameter"));
    EXPECT_EQ(7, result.document->lane_count());
}

TEST_F(CatalogLoading, requiresCategoricalDefaultsAndOrdinaryFunctionSources)
{
    for (const std::string type : {"string", "enum", "function-list"})
    {
        SCOPED_TRACE(type);
        m_catalog["parameters"]["scalar"] = {{"type", type}, {"description", "Discrete target"}};
        if (type == "enum")
        {
            m_catalog["parameters"]["scalar"]["values"] = {"a", "b"};
        }
        if (type == "function-list")
        {
            m_catalog["parameters"]["scalar"]["values"] = "id-functions";
        }
        const JsonImportResult result = import({m_catalog});
        ASSERT_TRUE(result.succeeded());
        ASSERT_EQ(1, timeline::size_cast(result.diagnostics));
        EXPECT_NE(std::string::npos, result.diagnostics[0].find("default-curve"));
    }
    m_catalog["parameters"]["function"] = {
        {"type", "function-list"}, {"description", "Functions"}, {"values", "id-functions"}, {"default-curve", "hold"}};
    m_config["tracks"][3]["parameter"] = "function";
    const JsonImportResult result = import({m_catalog});
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(1, timeline::size_cast(result.diagnostics));
    EXPECT_NE(std::string::npos, result.diagnostics[0].find("source parameter"));
}

TEST_F(CatalogLoading, suppliesCenterMagOptionalFieldsWithoutChangingAuthoredValues)
{
    m_catalog["parameters"]["scalar"] = {{"type", "center-mag"}, {"description", "Center and magnification"}};
    m_config["tracks"][3]["keys"][0]["value"] = "0/0/1";
    m_config["tracks"][3]["keys"][1]["value"] = "4/8/16/0/90/45";
    const JsonImportResult result = import({m_catalog});
    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.diagnostics.empty()) << result.diagnostics.front();
    ASSERT_EQ(14, result.document->lane_count());
    const timeline::FrameInspection inspection = *timeline::inspect_frame(*result.document, 1);
    EXPECT_NEAR(2, *inspection.lanes[9].value, 1e-12);
    EXPECT_DOUBLE_EQ(1, *inspection.lanes[10].value);
    EXPECT_DOUBLE_EQ(22.5, *inspection.lanes[11].value);
    EXPECT_EQ("0/0/1", inspection.lanes[9].items[0].attributes.at("value"));
}

TEST(TargetResolution, matchesReferenceFramesAndRetainsComparisonHitIdentities)
{
    const JsonImportResult imported = import_timeline_json("fixtures/target-resolution.json");
    ASSERT_TRUE(imported.succeeded());
    const timeline::Document copy = *imported.document;
    const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
    ASSERT_TRUE(music.succeeded());
    const timeline::Document comparison = timeline::combine_documents(copy, *music.document);
    const std::map<std::string, std::vector<int>> numeric{{"maxiter", {0}}, {"params", {1, 2, 3}}, {"complex", {6, 7}},
        {"center-mag", {8, 9, 10}}, {"corners", {11, 12, 13, 14}}};
    const std::map<std::string, int> discrete{{"function", 4}, {"label", 5}, {"showorbit", 15}, {"functions", 16}};
    std::ifstream golden("fixtures/gold-target-resolution.par");
    ASSERT_TRUE(golden);
    std::string line;
    int frame = -1;
    int checks = 0;
    while (std::getline(golden, line))
    {
        if (line.substr(0, 6) == "frame-")
        {
            ++frame;
        }
        const std::size_t equal = line.find('=');
        const std::size_t start = line.find_first_not_of(' ');
        if (equal == std::string::npos || start == std::string::npos || frame < 0)
        {
            continue;
        }
        const std::string name = line.substr(start, equal - start);
        const std::string value = line.substr(equal + 1);
        const timeline::FrameInspection inspection = *timeline::inspect_frame(comparison, frame);
        if (numeric.count(name))
        {
            std::string components = value;
            std::replace(components.begin(), components.end(), '/', ' ');
            std::istringstream input(components);
            for (int lane : numeric.at(name))
            {
                double expected = 0;
                ASSERT_TRUE(input >> expected);
                EXPECT_NEAR(expected, *inspection.lanes[lane].value, 1e-10);
                ++checks;
            }
        }
        else if (discrete.count(name))
        {
            EXPECT_EQ(value, inspection.lanes[discrete.at(name)].items[0].attributes.at("value"));
            ++checks;
        }
    }
    EXPECT_EQ(85, checks);
    const timeline::Layout layout(comparison,
        timeline::Viewport(900, 1100, copy.frame_grid()->offset(), copy.frame_grid()->end_time()),
        timeline::LayoutMetrics(140, 20, 40, 4));
    bool found = false;
    for (const timeline::Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<timeline::Polyline>(primitive))
        {
            const timeline::Polyline &line = std::get<timeline::Polyline>(primitive);
            if (layout.display_list().strings().lookup(line.id.lane_id) == "animation-1[0]")
            {
                const std::optional<timeline::HitResult> hit = layout.hit_test(line.points.front(), 0);
                ASSERT_TRUE(hit);
                EXPECT_EQ(line.id.lane_id, hit->id.lane_id);
                EXPECT_EQ(line.id.item_id, hit->id.item_id);
                found = true;
            }
        }
    }
    EXPECT_TRUE(found);
    EXPECT_FALSE(import_timeline_json("fixtures/target-resolution-invalid.json").succeeded());
}

TEST_F(CatalogLoading, enforcesDeclaredValueTypesAndComplexArity)
{
    const std::vector<std::pair<Json, Json>> invalid{
        {{{"type", "double"}, {"default-curve", "linear"}}, Json::array({1})},
        {{{"type", "integer"}, {"default-curve", "linear"}}, "1.5"},
        {{{"type", "complex"}, {"default-curve", "linear"}}, "1/2/3"},
        {{{"type", "complex"}, {"default-curve", "linear"}}, Json::array({1, 2})},
        {{{"type", "enum"}, {"default-curve", "hold"}, {"values", {"a", "b"}}}, "unknown"},
        {{{"type", "enum"}, {"default-curve", "hold"}, {"values", {"1", "2"}}}, 1},
        {{{"type", "yes-no"}, {"default-curve", "hold"}}, "yes"}, {{{"type", "string"}, {"default-curve", "hold"}}, 1},
        {{{"type", "function-list"}, {"default-curve", "hold"}, {"values", "id-functions"}}, Json::array({"bad"})},
        {{{"type", "corners"}}, "1/2/3"}, {{{"type", "center-mag"}}, "1/2"}};
    for (const auto &[metadata, value] : invalid)
    {
        SCOPED_TRACE(metadata.dump() + " " + value.dump());
        m_catalog["parameters"]["scalar"] = metadata;
        m_catalog["parameters"]["scalar"]["description"] = "Test target";
        m_config["tracks"][3]["keys"][0]["value"] = value;
        m_config["tracks"][3]["keys"][1]["value"] = value;
        const JsonImportResult result = import({m_catalog});
        ASSERT_TRUE(result.succeeded());
        ASSERT_EQ(1, timeline::size_cast(result.diagnostics));
        EXPECT_NE(std::string::npos, result.diagnostics[0].find("animation-3"));
        EXPECT_EQ(8, result.document->lane_count());
    }
}

TEST_F(CatalogLoading, diagnosesRecognizedButUnsupportedGenericTypes)
{
    for (const std::string type : {"miim", "potential", "numeric-tuple-or-enum", "color-map"})
    {
        SCOPED_TRACE(type);
        Json metadata{{"type", type}, {"description", "Unsupported target"}, {"default-curve", "hold"}};
        if (type == "numeric-tuple-or-enum")
        {
            metadata["values"] = {"off"};
        }
        m_catalog["parameters"]["scalar"] = metadata;
        const JsonImportResult result = import({m_catalog});
        ASSERT_TRUE(result.succeeded());
        ASSERT_EQ(1, timeline::size_cast(result.diagnostics));
        EXPECT_NE(std::string::npos, result.diagnostics[0].find("unsupported"));
        EXPECT_EQ(8, result.document->lane_count());
    }
}

TEST_F(CatalogLoading, rejectsComplexBoundsBeforeCreatingComponentLanes)
{
    m_catalog["parameters"]["scalar"] = {
        {"type", "complex"}, {"description", "Complex"}, {"default-curve", "linear"}, {"max", 5}};
    m_config["tracks"][3]["keys"][0]["value"] = "1/2";
    m_config["tracks"][3]["keys"][1]["value"] = "3/6";
    m_catalog["fractal-types"]["mandel"]["params"]["groups"]["c"] = m_catalog["parameters"]["scalar"];
    m_catalog["fractal-types"]["mandel"]["params"]["groups"]["c"]["slots"] = {0, 1};
    m_config["tracks"][3]["parameter"] = "params.c";
    const JsonImportResult result = import({m_catalog});
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(1, timeline::size_cast(result.diagnostics));
    EXPECT_EQ(8, result.document->lane_count());
}

TEST_F(CatalogLoading, rejectsMalformedUnusedMetadataBeforeImportingTracks)
{
    const std::vector<Json> invalid{nullptr, Json::array(), Json::object(),
        {{"type", "unknown"}, {"description", "Unused"}}, {{"type", "double"}, {"description", ""}},
        {{"type", "double"}, {"description", 1}},
        {{"type", "double"}, {"description", "Unused"}, {"format", "unknown"}},
        {{"type", "double"}, {"description", "Unused"}, {"default-curve", "unknown"}},
        {{"type", "double"}, {"description", "Unused"}, {"extrapolate", "unknown"}},
        {{"type", "double"}, {"description", "Unused"}, {"min", "zero"}},
        {{"type", "double"}, {"description", "Unused"}, {"max", nullptr}},
        {{"type", "double"}, {"description", "Unused"}, {"normalize", 1}},
        {{"type", "numeric-tuple"}, {"description", "Unused"}, {"arity", 0}},
        {{"type", "numeric-tuple"}, {"description", "Unused"}, {"arity", 1.5}},
        {{"type", "point2"}, {"description", "Unused"}, {"arity", 3}}, {{"type", "enum"}, {"description", "Unused"}},
        {{"type", "enum"}, {"description", "Unused"}, {"values", Json::array()}},
        {{"type", "enum"}, {"description", "Unused"}, {"values", {1}}},
        {{"type", "double"}, {"description", "Unused"}, {"values", {"a"}}},
        {{"type", "function-list"}, {"description", "Unused"}, {"values", {"sin"}}},
        {{"type", "function-list"}, {"description", "Unused"}, {"values", "unknown"}}};
    for (const Json &entry : invalid)
    {
        SCOPED_TRACE(entry.dump());
        Json catalog = m_catalog;
        catalog["parameters"]["unused"] = entry;
        const JsonImportResult result = import({catalog});
        EXPECT_FALSE(result.succeeded());
        ASSERT_FALSE(result.diagnostics.empty());
        EXPECT_NE(std::string::npos, result.diagnostics.front().find("catalog"));
    }
}

TEST_F(CatalogLoading, validatesNestedUnusedEntries)
{
    const Json invalid = Json::parse(R"([
        {"fractal-types": []}, {"formula-entries": []},
        {"fractal-types": {"unused": 1}},
        {"fractal-types": {"unused": {"params": {"slots": 1}}}},
        {"fractal-types": {"unused": {"params": {"slots": [{"index": 0, "name": "slot"}]}}}},
        {"fractal-types": {"unused": {"params": {"groups": {"c": {"type": "complex", "description": "C"}}}}}},
        {"fractal-types": {"unused": {"functions": {"fn5": {"type": "enum", "values": "id-functions", "description": "Function"}}}}},
        {"formula-entries": {"unused": {"params": {"knobs": {"k": {"type": "real", "description": "Knob", "variable": "p5.real"}}}}}},
        {"formula-entries": {"unused": {"functions": {"fn1": {"type": "double", "description": "Function", "values": "id-functions"}}}}}
    ])");
    for (const Json &sections : invalid)
    {
        SCOPED_TRACE(sections.dump());
        Json catalog = m_catalog;
        catalog.update(sections);
        const JsonImportResult result = import({catalog});
        EXPECT_FALSE(result.succeeded());
        ASSERT_FALSE(result.diagnostics.empty());
    }
}

TEST_F(CatalogLoading, rejectsDuplicatesInEachNamedSectionAndEmptyCatalogLists)
{
    EXPECT_FALSE(import({}).succeeded());
    for (const std::string section : {"parameters", "fractal-types", "formula-entries"})
    {
        SCOPED_TRACE(section);
        Json first = m_catalog;
        if (section != "parameters")
        {
            first[section]["unused"] = Json::object();
        }
        Json second{{"parameters", Json::object()}};
        second[section] = first.at(section);
        const JsonImportResult result = import({first, second});
        EXPECT_FALSE(result.succeeded());
        ASSERT_FALSE(result.diagnostics.empty());
        EXPECT_NE(std::string::npos, result.diagnostics.front().find("Duplicate"));
    }
}

TEST_F(CatalogLoading, composesDistinctValidCatalogsWithoutChangingOwnedMetadata)
{
    const Json extra{
        {"parameters",
            {{"unused", {{"type", "point2"}, {"description", "Unused alias"}, {"normalize", false}}},
                {"choice", {{"type", "enum"}, {"description", "Choice"}, {"values", {"a", "b"}}}},
                {"function", {{"type", "function-list"}, {"description", "Functions"}, {"values", "id-functions"}}}}},
        {"fractal-types",
            {{"unused",
                {{"params",
                     {{"slots", {{{"index", 0}, {"name", "slot"}, {"type", "double"}, {"description", "Slot"}}}},
                         {"groups", {{"c", {{"type", "complex"}, {"description", "C"}, {"slots", {0, 1}}}}}}}},
                    {"functions",
                        {{"fn1", {{"type", "enum"}, {"values", "id-functions"}, {"description", "Function"}}}}}}}}},
        {"formula-entries",
            {{"unused",
                {{"params",
                    {{"knobs",
                        {{"k", {{"type", "real"}, {"description", "Knob"}, {"variable", "p1.real"}}},
                            {"c", {{"type", "complex"}, {"description", "Complex knob"}, {"variable", "p2"}}}}}}}}}}}};
    const JsonImportResult result = import({m_catalog, extra});
    ASSERT_TRUE(result.succeeded());
    EXPECT_TRUE(result.diagnostics.empty());
    ASSERT_EQ(9, result.document->lane_count());
    const timeline::Document copy = *result.document;
    const Json metadata =
        Json::parse(timeline::inspect_frame(copy, 1)->lanes[0].items[0].attributes.at("catalog-definition"));
    EXPECT_EQ(m_catalog.at("parameters").at("position"), metadata);
    EXPECT_DOUBLE_EQ(2, *timeline::inspect_frame(copy, 1)->lanes[0].value);
}

TEST_F(CatalogLoading, validatesCatalogRootAndParameterSection)
{
    for (const Json &catalog : std::vector<Json>{nullptr, Json::array(), Json::object(), {{"parameters", 1}}})
    {
        SCOPED_TRACE(catalog.dump());
        const JsonImportResult result = import({catalog});
        EXPECT_FALSE(result.succeeded());
        ASSERT_FALSE(result.diagnostics.empty());
        EXPECT_NE(std::string::npos, result.diagnostics.front().find("catalog"));
    }
}

TEST_F(CatalogLoading, acceptsSourceTypesAndOptionalMetadataWithoutEvaluatingUnusedEntries)
{
    for (const std::string type : {"center-mag", "color-map", "corners", "complex", "double", "yes-no", "integer",
             "integer-tuple", "miim", "numeric-tuple", "point2", "point3", "potential", "string", "vector2", "vector3",
             "enum", "inside", "outside", "integer-or-enum", "numeric-tuple-or-enum", "function-list"})
    {
        SCOPED_TRACE(type);
        Json catalog = m_catalog;
        Json entry{{"type", type}, {"description", "Unused"}, {"format", "raw"}, {"default-curve", "geometric"},
            {"extrapolate", "base"}, {"normalize", true}, {"min", 10}, {"max", -10}};
        if (type == "enum" || type == "inside" || type == "outside" || type == "integer-or-enum" ||
            type == "numeric-tuple-or-enum")
        {
            entry["values"] = Json::array({"choice"});
        }
        else if (type == "function-list")
        {
            entry["values"] = "id-functions";
        }
        catalog["parameters"]["unused"] = entry;
        const JsonImportResult result = import({catalog});
        EXPECT_TRUE(result.succeeded());
        EXPECT_TRUE(result.diagnostics.empty());
    }
}

TEST(CatalogCompatibility, importsValidCatalogCompositionAndRejectsUnusedMalformedMetadata)
{
    const JsonImportResult valid = import_timeline_json("fixtures/catalog-loading.json");
    ASSERT_TRUE(valid.succeeded());
    EXPECT_TRUE(valid.diagnostics.empty());
    ASSERT_EQ(9, valid.document->lane_count());
    EXPECT_DOUBLE_EQ(2, *timeline::inspect_frame(*valid.document, 1)->lanes[0].value);
    const JsonImportResult invalid = import_timeline_json("fixtures/catalog-loading-invalid.json");
    EXPECT_FALSE(invalid.succeeded());
    ASSERT_EQ(1, timeline::size_cast(invalid.diagnostics));
    EXPECT_NE(std::string::npos, invalid.diagnostics[0].find("unused"));
    EXPECT_NE(std::string::npos, invalid.diagnostics[0].find("description"));
}

TEST(CatalogCompatibility, retainsComponentMetadataAndOwnedDefinitions)
{
    JsonImportResult imported = import_timeline_json("fixtures/catalog-loading.json");
    ASSERT_TRUE(imported.succeeded());
    ASSERT_TRUE(imported.diagnostics.empty());
    const timeline::Document document = *imported.document;
    imported.document.reset();
    ASSERT_EQ(9, document.lane_count());
    const std::array<int, 9> components{0, 1, 0, 1, 2, 0, 1, 0, 0};
    const std::array<int, 9> arities{2, 2, 3, 3, 3, 2, 2, 1, 1};
    const timeline::FrameInspection inspection = *timeline::inspect_frame(document, 1);
    const std::array<double, 9> values{2, 4, 1, 2, 3, 2, 4, 4.75, 100.75};
    using Json = nlohmann::json;
    for (int index = 0; index < document.lane_count(); ++index)
    {
        SCOPED_TRACE(index);
        EXPECT_DOUBLE_EQ(values[index], *inspection.lanes[index].value);
        const timeline::Attributes &attributes = inspection.lanes[index].items[0].attributes;
        EXPECT_EQ(std::to_string(components[index]), attributes.at("component"));
        EXPECT_EQ(std::to_string(arities[index]), attributes.at("arity"));
        const Json catalog = Json::parse(attributes.at("catalog-definition"));
        EXPECT_TRUE(catalog.contains("type"));
        const Json track = Json::parse(attributes.at("track-definition"));
        EXPECT_EQ(attributes.at("parameter"), track.at("parameter"));
    }
    EXPECT_DOUBLE_EQ(101, *inspection.lanes[8].output_value);
    EXPECT_FALSE(inspection.lanes[0].output_value);
    const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
    ASSERT_TRUE(music.succeeded());
    const timeline::Document combined = timeline::combine_documents(document, *music.document);
    ASSERT_EQ(13, combined.lane_count());
    EXPECT_DOUBLE_EQ(4, *timeline::inspect_frame(combined, 1)->lanes[1].value);
    const timeline::Layout layout(combined,
        timeline::Viewport(600, 400, document.frame_grid()->offset(), document.frame_grid()->end_time()),
        timeline::LayoutMetrics(100, 20, 40, 4));
    bool hit = false;
    for (const timeline::Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<timeline::Polyline>(primitive))
        {
            const timeline::Polyline &line = std::get<timeline::Polyline>(primitive);
            if (layout.display_list().strings().lookup(line.id.lane_id) == "animation-0[0]")
            {
                const std::optional<timeline::HitResult> result = layout.hit_test(line.points.front(), 0);
                ASSERT_TRUE(result);
                EXPECT_EQ(line.id.lane_id, result->id.lane_id);
                EXPECT_EQ(line.id.item_id, result->id.item_id);
                hit = true;
            }
        }
    }
    EXPECT_TRUE(hit);
}

TEST(CatalogCompatibility, rejectsSourceInvalidTracksWithoutPartialLanes)
{
    const JsonImportResult imported = import_timeline_json("fixtures/catalog-audit-invalid.json");
    EXPECT_FALSE(imported.succeeded());
    ASSERT_EQ(16, imported.diagnostics.size());
    const std::array<std::string, 15> messages{"arity", "arity", "scalar", "array", "bounds", "bounds", "default-curve",
        "path", "path", "curve", "curve", "array", "default-curve", "path", "path"};
    for (int index = 0; index < timeline::size_cast(messages); ++index)
    {
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find("animation-" + std::to_string(index) + ":"));
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find(messages[index]));
    }
}

TEST(CatalogCompatibility, matchesReferenceOutputAtEveryFrame)
{
    const JsonImportResult imported = import_timeline_json("fixtures/catalog-loading.json");
    ASSERT_TRUE(imported.succeeded());
    std::ifstream golden("fixtures/gold-catalog-audit.par");
    ASSERT_TRUE(golden);
    int frame = -1;
    int checked = 0;
    std::string token;
    const std::array<std::string, 5> parameters{"position", "direction", "tuple", "scalar", "maxiter"};
    const std::array<int, 5> lanes{0, 2, 5, 7, 8};
    while (golden >> token)
    {
        if (token.substr(0, 6) == "frame-")
        {
            ++frame;
        }
        for (int index = 0; index < timeline::size_cast(parameters); ++index)
        {
            const std::string prefix = parameters[index] + "=";
            if (token.substr(0, prefix.size()) != prefix)
            {
                continue;
            }
            std::string values = token.substr(prefix.size());
            std::replace(values.begin(), values.end(), '/', ' ');
            std::istringstream components(values);
            double expected = 0;
            int lane = lanes[index];
            const timeline::FrameInspection inspection = *timeline::inspect_frame(*imported.document, frame);
            while (components >> expected)
            {
                EXPECT_DOUBLE_EQ(
                    expected, index == 4 ? *inspection.lanes[lane].output_value : *inspection.lanes[lane].value);
                ++lane;
                ++checked;
            }
        }
    }
    EXPECT_EQ(4, frame);
    EXPECT_EQ(45, checked);
}
