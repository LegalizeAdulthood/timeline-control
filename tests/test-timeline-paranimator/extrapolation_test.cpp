// Copyright (c) 2026 Richard Thomson

#include <timeline/Layout.h>
#include <timeline/Query.h>
#include <timelineParAnimator/TimelineJson.h>

#include <gtest/gtest.h>

#include <array>
#include <fstream>
#include <iterator>
#include <limits>

using namespace timeline_par_animator;

TEST(Extrapolation, matches_source_output_and_preserves_owned_keys_in_comparison)
{
    for (const std::string &policy : {"base", "omit", "cycle", "ping-pong"})
    {
        SCOPED_TRACE(policy);
        JsonImportOptions options;
        options.frames_per_second_numerator = 30000;
        options.frames_per_second_denominator = 1001;
        JsonImportResult imported = import_timeline_json("fixtures/maxiter-" + policy + ".json", options);
        ASSERT_TRUE(imported.succeeded());
        ASSERT_TRUE(imported.diagnostics.empty());
        const timeline::Document document = *imported.document;
        imported.document.reset();
        const timeline::Lane &lane = document.lanes()[0];
        ASSERT_EQ(2, lane.item_count());
        const timeline::Keyframe &key = std::get<timeline::Keyframe>(lane.items()[0]);
        EXPECT_EQ(policy, key.attributes().at("extrapolate"));
        EXPECT_NE(std::string::npos, key.attributes().at("track-definition").find("keys"));
        EXPECT_EQ(4004, key.time().ticks());
        const timeline::FrameGrid &grid = *document.frame_grid();
        std::ifstream input("fixtures/gold-maxiter-" + policy + ".par");
        ASSERT_TRUE(input);
        const std::string golden{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        for (int frame = 0; frame < grid.frame_count(); ++frame)
        {
            SCOPED_TRACE(frame);
            const std::size_t start = golden.find("frame-000" + std::to_string(frame + 1) + " {");
            ASSERT_NE(std::string::npos, start);
            const std::string entry = golden.substr(start, golden.find('}', start) - start);
            const std::size_t value_start = entry.find("maxiter=");
            const std::optional<double> value = lane.evaluate_keyframes(grid.frame_start(frame));
            const timeline::FrameInspection inspection = *timeline::inspect_frame(document, frame);
            EXPECT_EQ(value, inspection.lanes[0].value);
            if (value_start == std::string::npos)
            {
                EXPECT_FALSE(value);
            }
            else
            {
                ASSERT_TRUE(value);
                EXPECT_DOUBLE_EQ(std::stod(entry.substr(value_start + 8)), *value);
            }
        }
        const JsonImportResult comparison = import_timeline_json("fixtures/maxiter-" + policy + ".json");
        ASSERT_TRUE(comparison.succeeded());
        const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
        ASSERT_TRUE(music.succeeded());
        const timeline::Document combined = timeline::combine_documents(*comparison.document, *music.document);
        EXPECT_EQ(5, combined.lane_count());
        EXPECT_EQ(lane.evaluate_keyframes(timeline::Time{}), combined.lanes()[0].evaluate_keyframes(timeline::Time{}));
        const timeline::Layout layout(combined, timeline::Viewport(600, 300, grid.offset(), grid.end_time()),
            timeline::LayoutMetrics(100, 20, 40, 4));
        bool hit = false;
        for (const timeline::Primitive &primitive : layout.display_list().primitives())
        {
            if (std::holds_alternative<timeline::Polyline>(primitive))
            {
                const timeline::Polyline &line = std::get<timeline::Polyline>(primitive);
                if (line.id.lane_id == lane.id())
                {
                    for (const timeline::Point &point : line.points)
                    {
                        EXPECT_GE(point.y, 24);
                        EXPECT_LT(point.y, 56);
                    }
                    const std::optional<timeline::HitResult> result = layout.hit_test(line.points.front(), 0);
                    ASSERT_TRUE(result);
                    EXPECT_EQ(lane.id(), result->id.lane_id);
                    hit = true;
                }
            }
        }
        EXPECT_TRUE(hit);
    }
}

TEST(Extrapolation, keeps_continuous_values_and_exact_periods_before_and_after_keys)
{
    const JsonImportResult cycle = import_timeline_json("fixtures/maxiter-cycle.json");
    const JsonImportResult ping = import_timeline_json("fixtures/maxiter-ping-pong.json");
    const JsonImportResult base = import_timeline_json("fixtures/maxiter-base.json");
    const JsonImportResult omit = import_timeline_json("fixtures/maxiter-omit.json");
    ASSERT_TRUE(cycle.succeeded());
    ASSERT_TRUE(ping.succeeded());
    ASSERT_TRUE(base.succeeded());
    ASSERT_TRUE(omit.succeeded());
    const timeline::Lane &cycling = cycle.document->lanes()[0];
    const timeline::Lane &reflecting = ping.document->lanes()[0];
    const timeline::Ticks width = cycle.document->frame_grid()->frame_duration().ticks();
    for (int frame : {-7, -1, 2, 5, 11})
    {
        EXPECT_DOUBLE_EQ(200, *cycling.evaluate_keyframes(timeline::Time::from_ticks(frame * width)));
    }
    EXPECT_DOUBLE_EQ(300, *cycling.evaluate_keyframes(timeline::Time::from_ticks(7 * width / 2)));
    EXPECT_DOUBLE_EQ(150, *cycling.evaluate_keyframes(timeline::Time::from_ticks(9 * width / 2)));
    EXPECT_DOUBLE_EQ(250, *reflecting.evaluate_keyframes(timeline::Time::from_ticks(7 * width / 2)));
    EXPECT_DOUBLE_EQ(200, *reflecting.evaluate_keyframes(timeline::Time::from_ticks(-4 * width)));
    EXPECT_DOUBLE_EQ(150, *cycling.evaluate_keyframes(timeline::Time::from_ticks(3 * width / 2)));
    EXPECT_DOUBLE_EQ(100, *base.document->lanes()[0].evaluate_keyframes(timeline::Time::from_ticks(width)));
    EXPECT_DOUBLE_EQ(200, *base.document->lanes()[0].evaluate_keyframes(timeline::Time::from_ticks(2 * width)));
    EXPECT_DOUBLE_EQ(678, *base.document->lanes()[0].evaluate_keyframes(timeline::Time::from_ticks(width - 1)));
    EXPECT_DOUBLE_EQ(678, *base.document->lanes()[0].evaluate_keyframes(timeline::Time::from_ticks(2 * width + 1)));
    EXPECT_FALSE(omit.document->lanes()[0].evaluate_keyframes(timeline::Time::from_ticks(width - 1)));
    EXPECT_FALSE(omit.document->lanes()[0].evaluate_keyframes(timeline::Time::from_ticks(2 * width + 1)));
    EXPECT_TRUE(cycling.evaluate_keyframes(timeline::Time::from_ticks(std::numeric_limits<timeline::Ticks>::min())));
    EXPECT_TRUE(reflecting.evaluate_keyframes(timeline::Time::from_ticks(std::numeric_limits<timeline::Ticks>::max())));
}

TEST(Extrapolation, diagnoses_malformed_keys_and_unsupported_forms_without_partial_lanes)
{
    const JsonImportResult partial = import_timeline_json("fixtures/extrapolation-partial.json");
    ASSERT_TRUE(partial.succeeded());
    ASSERT_EQ(6, partial.diagnostics.size());
    ASSERT_EQ(1, partial.document->lane_count());
    EXPECT_EQ("animation-6", partial.document->lanes()[0].id());
    for (int index = 0; index < 6; ++index)
    {
        EXPECT_NE(std::string::npos, partial.diagnostics[index].find("animation-" + std::to_string(index) + ":"));
    }
    EXPECT_NE(std::string::npos, partial.diagnostics[0].find("strictly increasing"));
    EXPECT_NE(std::string::npos, partial.diagnostics[1].find("numeric scalar"));
    EXPECT_NE(std::string::npos, partial.diagnostics[2].find("two keys"));
    EXPECT_NE(std::string::npos, partial.diagnostics[3].find("source value"));
    EXPECT_NE(std::string::npos, partial.diagnostics[4].find("integral"));
    EXPECT_NE(std::string::npos, partial.diagnostics[5].find("JSON integer"));
    const JsonImportResult invalid = import_timeline_json("fixtures/extrapolation-invalid.json");
    EXPECT_FALSE(invalid.succeeded());
    ASSERT_EQ(1, timeline::size_cast(invalid.diagnostics));
    EXPECT_NE(std::string::npos, invalid.diagnostics[0].find("unknown extrapolate"));
}

TEST(Extrapolation, matches_source_double_integer_or_enum_and_hold_evaluation)
{
    const JsonImportResult imported = import_timeline_json("fixtures/extrapolation-variants.json");
    ASSERT_TRUE(imported.succeeded());
    ASSERT_TRUE(imported.diagnostics.empty());
    ASSERT_EQ(3, imported.document->lane_count());
    const timeline::Document document = *imported.document;
    const timeline::FrameGrid &grid = *document.frame_grid();
    std::ifstream input("fixtures/gold-extrapolation-variants.par");
    ASSERT_TRUE(input);
    const std::string golden{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    const std::array<std::string, 3> parameters{"maxiter", "bailout", "choice"};
    for (int frame = 0; frame < grid.frame_count(); ++frame)
    {
        const std::size_t start = golden.find("frame-000" + std::to_string(frame + 1) + " {");
        ASSERT_NE(std::string::npos, start);
        const std::string entry = golden.substr(start, golden.find('}', start) - start);
        for (int index = 0; index < 3; ++index)
        {
            const std::string name = parameters[index] + "=";
            const std::size_t offset = entry.find(name);
            ASSERT_NE(std::string::npos, offset);
            const std::optional<double> actual = document.lanes()[index].evaluate_keyframes(grid.frame_start(frame));
            ASSERT_TRUE(actual);
            EXPECT_DOUBLE_EQ(std::stod(entry.substr(offset + name.size())), *actual);
        }
    }
    const timeline::Lane &base = document.lanes()[1];
    const timeline::Keyframe &key = std::get<timeline::Keyframe>(base.items()[0]);
    EXPECT_EQ("1.25", key.attributes().at("source-value"));
    EXPECT_EQ("Bailout_Demo", key.attributes().at("source-entry"));
    EXPECT_NE(std::string::npos, key.attributes().at("catalog-definition").find("double"));
    const timeline::Time half = timeline::Time::from_ticks(grid.frame_duration().ticks() * 3 / 2);
    EXPECT_DOUBLE_EQ(2, *base.evaluate_keyframes(half));
    EXPECT_DOUBLE_EQ(100, *document.lanes()[0].evaluate_keyframes(half));
}
