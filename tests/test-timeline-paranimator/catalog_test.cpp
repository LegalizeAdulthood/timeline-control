// Copyright (c) 2026 Richard Thomson

#include <timeline/Layout.h>
#include <timeline/Query.h>
#include <timeline/size_cast.h>
#include <timelineParAnimator/TimelineJson.h>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <sstream>

using namespace timeline_par_animator;

TEST(CatalogCompatibility, retains_component_metadata_and_owned_definitions)
{
    JsonImportResult imported = import_timeline_json("fixtures/catalog-audit.json");
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
    const JsonImportResult imported = import_timeline_json("fixtures/catalog-audit.json");
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
