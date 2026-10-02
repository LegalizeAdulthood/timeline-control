// Copyright (c) 2026 Richard Thomson

#include <timeline/Layout.h>
#include <timeline/Query.h>
#include <timeline/size_cast.h>
#include <timelineParAnimator/TimelineJson.h>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
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

TEST_F(CatalogLoading, rejects_malformed_unused_metadata_before_importing_tracks)
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

TEST_F(CatalogLoading, validates_nested_unused_entries)
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

TEST_F(CatalogLoading, rejects_duplicates_in_each_named_section_and_empty_catalog_lists)
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

TEST_F(CatalogLoading, composes_distinct_valid_catalogs_without_changing_owned_metadata)
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

TEST_F(CatalogLoading, validates_catalog_root_and_parameter_section)
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

TEST_F(CatalogLoading, accepts_source_types_and_optional_metadata_without_evaluating_unused_entries)
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

TEST(CatalogCompatibility, imports_valid_catalog_composition_and_rejects_unused_malformed_metadata)
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

TEST(CatalogCompatibility, retains_component_metadata_and_owned_definitions)
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
            if (line.id.lane_id == "animation-0[0]")
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

TEST(CatalogCompatibility, rejects_source_invalid_tracks_without_partial_lanes)
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

TEST(CatalogCompatibility, matches_reference_output_at_every_frame)
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
