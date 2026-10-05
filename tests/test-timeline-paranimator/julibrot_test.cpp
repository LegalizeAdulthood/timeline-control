// Copyright (c) 2026 Richard Thomson

#include <ResolvedAttributes.h>

#include <timelineParAnimator/TimelineJson.h>

#include <timeline/Layout.h>
#include <timeline/Query.h>
#include <timeline/size_cast.h>
#include <timeline/Snapshot.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>

using namespace timeline_par_animator;

namespace
{

std::vector<std::map<std::string, std::string>> julibrot_source_frames(const std::string &fixture)
{
    std::ifstream input("fixtures/gold-julibrot-view-" + fixture + ".par");
    if (!input)
    {
        throw std::runtime_error("Missing Julibrot golden");
    }
    std::vector<std::map<std::string, std::string>> frames;
    for (std::string line; std::getline(input, line);)
    {
        if (line.substr(0, 6) == "frame-")
        {
            frames.emplace_back();
        }
        const std::size_t equal = line.find('=');
        if (equal != std::string::npos)
        {
            const std::size_t start = line.find_first_not_of(" \t");
            std::string value = line.substr(equal + 1);
            if (!value.empty() && value.back() == '\r')
            {
                value.pop_back();
            }
            frames.back()[line.substr(start, equal - start)] = value;
        }
    }
    return frames;
}

} // namespace

TEST(JulibrotView, matchesSourceGoldenAndPreservesOwnedRecipes)
{
    for (const std::string &fixture : {"camera", "keyed", "hold", "normalized", "override", "camera-hold", "subnormal",
             "range", "range-hold", "tiny-component", "near-axis", "near-axis-hold", "near-axis-tolerance"})
    {
        SCOPED_TRACE(fixture);
        const timeline::Document document = [&fixture]
        {
            JsonImportOptions options;
            options.frames_per_second_numerator = 30000;
            options.frames_per_second_denominator = 1001;
            const JsonImportResult imported =
                import_timeline_json("fixtures/julibrot-view-" + fixture + ".json", options);
            if (!imported.succeeded() || !imported.diagnostics.empty())
            {
                throw std::runtime_error("Julibrot import failed: " + imported.diagnostics.front());
            }
            return *imported.document;
        }();
        const int outputs = fixture == "keyed" || fixture == "hold" ? 12 : 6;
        ASSERT_EQ(outputs == 12 ? 12 : 15, document.lane_count());
        const timeline::FrameGrid &grid = *document.frame_grid();
        EXPECT_EQ(4004, grid.frame_duration().ticks());
        const std::vector<std::map<std::string, std::string>> frames = julibrot_source_frames(fixture);
        ASSERT_EQ(grid.frame_count(), timeline::size_cast(frames));
        for (int frame = 0; frame < timeline::size_cast(frames); ++frame)
        {
            const timeline::FrameInspection inspected = *timeline::inspect_frame(document, frame);
            for (int component = 0; component < outputs; ++component)
            {
                const timeline::Lane &lane = document.lanes()[component];
                const timeline::Attributes stored_attributes =
                    std::visit([](const auto &item) { return item.attributes(); }, lane.items().front());
                const ResolvedAttributes attributes = resolved_attributes(document, stored_attributes);
                EXPECT_EQ("view", attributes.at("view-name"));
                EXPECT_NE(std::string::npos, attributes.at("julibrot-view").find("outputs"));
                std::string value = frames[frame].at(std::string(attributes.at("parameter")));
                const timeline::LaneInspection &sample = inspected.lanes[component];
                if (attributes.at("member") == "mode")
                {
                    EXPECT_EQ(value, resolved_attributes(document, sample.items.front().attributes).at("value"));
                    continue;
                }
                std::replace(value.begin(), value.end(), '/', ' ');
                std::istringstream values(value);
                double expected = 0;
                for (int scalar = 0; scalar <= std::stoi(std::string(attributes.at("component"))); ++scalar)
                {
                    ASSERT_TRUE(values >> expected);
                }
                EXPECT_NEAR(expected, sample.value ? *sample.value : *sample.items.front().value, 1e-9);
                if (std::holds_alternative<timeline::Curve>(lane.items().front()))
                {
                    const timeline::Curve &curve = std::get<timeline::Curve>(lane.items().front());
                    EXPECT_TRUE(curve.samples().empty());
                    EXPECT_EQ(std::string(document.strings().lookup(lane.id())) + "-view",
                        document.strings().lookup(curve.id()));
                }
            }
        }
        EXPECT_EQ("animation-0-geometry[0]", document.strings().lookup(document.lanes()[outputs == 12 ? 1 : 0].id()));
        const JsonImportResult music = import_timeline_json("fixtures/beat-keys/rms.beat-keys.json");
        const JsonImportResult animation = import_timeline_json("fixtures/julibrot-view-" + fixture + ".json");
        ASSERT_TRUE(music.succeeded());
        ASSERT_TRUE(animation.succeeded());
        const timeline::Document combined = timeline::combine_documents(*animation.document, *music.document);
        EXPECT_EQ(document.lane_count() + 4, combined.lane_count());
        const timeline::Layout layout(combined, timeline::Viewport(550, 850, grid.offset(), grid.end_time()),
            timeline::LayoutMetrics(110, 20, 30, 4));
        EXPECT_NE(std::string::npos, timeline::render_snapshot(layout.display_list()).find("geometry"));
    }
}

TEST(JulibrotView, samplesFractionalDistanceAndNormalizesAfterInterpolation)
{
    const JsonImportResult imported = import_timeline_json("fixtures/julibrot-view-normalized.json");
    ASSERT_TRUE(imported.succeeded());
    const timeline::Document document = *imported.document;
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Time half = grid.offset() + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2);
    const timeline::Curve &distance = std::get<timeline::Curve>(document.lanes()[5].items().front());
    EXPECT_DOUBLE_EQ(21.25, distance.sample(half));
    EXPECT_DOUBLE_EQ(18.5, distance.sample(grid.frame_start(1)));
    EXPECT_DOUBLE_EQ(13, distance.sample(grid.end_time()));
    for (int component = 0; component < 3; ++component)
    {
        const timeline::Curve &up = std::get<timeline::Curve>(document.lanes()[12 + component].items().front());
        const double expected = component == 0 ? 0 : component == 1 ? 1.75 : 0.25;
        EXPECT_NEAR(expected / std::sqrt(1.75 * 1.75 + 0.25 * 0.25), up.sample(half), 1e-12);
        EXPECT_EQ("true", resolved_attributes(document, up.attributes()).at("normalize"));
        EXPECT_NE(std::string::npos, resolved_attributes(document, up.attributes()).at("signal").find("0/2/1"));
        for (int index = 0; index <= 100; ++index)
        {
            const timeline::Time time = grid.offset() +
                timeline::Duration::from_ticks((grid.frame_start(2) - grid.offset()).ticks() * index / 100);
            EXPECT_GE(up.sample(time), up.minimum());
            EXPECT_LE(up.sample(time), up.maximum());
        }
    }
    const JsonImportResult keyed = import_timeline_json("fixtures/julibrot-view-keyed.json");
    ASSERT_TRUE(keyed.succeeded());
    EXPECT_DOUBLE_EQ(0.8125, *keyed.document->lanes()[7].evaluate_keyframes(half));
    EXPECT_EQ("monocular",
        resolved_attributes(keyed, timeline::inspect_frame(*keyed.document, 1)->lanes[0].items.front().attributes)
            .at("value"));
    EXPECT_EQ("red-blue",
        resolved_attributes(keyed, timeline::inspect_frame(*keyed.document, 2)->lanes[0].items.front().attributes)
            .at("value"));
}

TEST(JulibrotView, evaluatesNearAxisDistanceAndSignedHintsContinuously)
{
    for (const std::string &fixture : {"near-axis", "near-axis-hold", "near-axis-tolerance"})
    {
        SCOPED_TRACE(fixture);
        JsonImportOptions options;
        options.frames_per_second_numerator = 30000;
        options.frames_per_second_denominator = 1001;
        const JsonImportResult imported = import_timeline_json("fixtures/julibrot-view-" + fixture + ".json", options);
        ASSERT_TRUE(imported.succeeded());
        ASSERT_TRUE(imported.diagnostics.empty());
        const timeline::Document document = *imported.document;
        const timeline::FrameGrid &grid = *document.frame_grid();
        const timeline::Curve &distance = std::get<timeline::Curve>(document.lanes()[5].items().front());
        for (int index = 0; index <= 200; ++index)
        {
            const timeline::Time time = grid.offset() +
                timeline::Duration::from_ticks((grid.frame_start(2) - grid.offset()).ticks() * index / 200);
            std::array<double, 3> target{};
            for (int axis = 0; axis < 3; ++axis)
            {
                const double eye = *document.lanes()[6 + axis].evaluate_keyframes(time);
                const double look = *document.lanes()[9 + axis].evaluate_keyframes(time);
                target[axis] = (std::abs(look) < 1e-12 ? 0 : look) - (std::abs(eye) < 1e-12 ? 0 : eye);
            }
            EXPECT_DOUBLE_EQ(std::sqrt(target[0] * target[0] + target[1] * target[1] + target[2] * target[2]),
                distance.sample(time));
            EXPECT_GE(distance.sample(time), distance.minimum());
            EXPECT_LE(distance.sample(time), distance.maximum());
            for (int axis = 0; axis < 3; ++axis)
            {
                const timeline::Curve &hint = std::get<timeline::Curve>(document.lanes()[12 + axis].items().front());
                EXPECT_GE(hint.sample(time), hint.minimum());
                EXPECT_LE(hint.sample(time), hint.maximum());
                EXPECT_TRUE(hint.samples().empty());
                EXPECT_EQ("true", resolved_attributes(document, hint.attributes()).at("used-by-camera"));
            }
        }
        EXPECT_EQ("128/8/8/7/10/24", resolved_attributes(document, distance.attributes()).at("source-value"));
        const timeline::Curve &up_x = std::get<timeline::Curve>(document.lanes()[12].items().front());
        EXPECT_LT(up_x.sample(grid.frame_start(2)), 0);
        const double expected_x = fixture == "near-axis-tolerance" ? -9.99999e-10 : -4e-10 / std::sqrt(5.0);
        EXPECT_NEAR(expected_x, up_x.sample(grid.frame_start(2)), 1e-22);
        if (fixture == "near-axis-hold")
        {
            EXPECT_DOUBLE_EQ(distance.sample(grid.offset()), distance.sample(grid.frame_start(1)));
            EXPECT_NE(distance.sample(grid.frame_start(1)), distance.sample(grid.frame_start(2)));
        }
    }
}

TEST(JulibrotView, rejectsNearAxisBoundariesAndOffGridDepartures)
{
    const JsonImportResult imported = import_timeline_json("fixtures/invalid-julibrot-near-axis.json");
    EXPECT_FALSE(imported.succeeded());
    const std::array<std::string, 7> diagnostics{
        "centered", "straight-on", "straight-on", "view-up", "view-up", "straight-on", "view-up"};
    ASSERT_EQ(diagnostics.size() + 1, imported.diagnostics.size());
    EXPECT_NE(std::string::npos, imported.diagnostics.back().find("no supported animation tracks"));
    for (int index = 0; index < timeline::size_cast(diagnostics); ++index)
    {
        SCOPED_TRACE(index);
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find("animation-" + std::to_string(index) + ":"));
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find(diagnostics[index]));
    }
    const JsonImportResult partial = import_timeline_json("fixtures/partial-julibrot-near-axis.json");
    ASSERT_TRUE(partial.succeeded());
    ASSERT_EQ(7, timeline::size_cast(partial.diagnostics));
    ASSERT_EQ(30, partial.document->lane_count());
    EXPECT_EQ("animation-7-geometry[0]", partial.document->strings().lookup(partial.document->lanes()[0].id()));
    EXPECT_EQ("animation-8-geometry[0]", partial.document->strings().lookup(partial.document->lanes()[15].id()));
    const timeline::Time middle = partial.document->frame_grid()->frame_start(1);
    EXPECT_DOUBLE_EQ(18.5, *partial.document->lanes()[20].evaluate_keyframes(middle));
    const timeline::Keyframe &raw_eye = std::get<timeline::Keyframe>(partial.document->lanes()[21].items().front());
    EXPECT_DOUBLE_EQ(1, raw_eye.value());
    EXPECT_EQ("false", resolved_attributes(partial, raw_eye.attributes()).at("used-by-camera"));
}

TEST(JulibrotView, preservesLayerBaseGeometryAliasesAndUnusedCamera)
{
    const timeline::Document document = []
    {
        const JsonImportResult imported = import_timeline_json("fixtures/julibrot-view-layers.json");
        if (!imported.succeeded() || !imported.diagnostics.empty())
        {
            throw std::runtime_error("Julibrot layer import failed: " + imported.diagnostics.front());
        }
        return *imported.document;
    }();
    ASSERT_EQ(30, document.lane_count());
    const timeline::Curve &width = std::get<timeline::Curve>(document.lanes()[4].items().front());
    EXPECT_DOUBLE_EQ(6, width.sample(document.frame_grid()->frame_start(1)));
    EXPECT_EQ("geometry-alias", resolved_attributes(document, width.attributes()).at("parameter"));
    EXPECT_EQ("Aliased", resolved_attributes(document, width.attributes()).at("source-entry"));
    EXPECT_EQ("aliased", resolved_attributes(document, width.attributes()).at("layer"));
    EXPECT_EQ("64/3/4/5/6/99", resolved_attributes(document, width.attributes()).at("source-value"));
    EXPECT_EQ("animation-layer-0-0-geometry[4]", document.strings().lookup(document.lanes()[4].id()));
    const timeline::Lane &overridden = document.lanes()[20];
    EXPECT_DOUBLE_EQ(18.5, *overridden.evaluate_keyframes(document.frame_grid()->frame_start(1)));
    const timeline::Keyframe &up = std::get<timeline::Keyframe>(document.lanes()[28].items().front());
    EXPECT_DOUBLE_EQ(0, up.value());
    EXPECT_EQ("false", resolved_attributes(document, up.attributes()).at("used-by-camera"));
    EXPECT_EQ("overridden", resolved_attributes(document, up.attributes()).at("layer"));
}

TEST(JulibrotView, rejectsInvalidTracksTransactionallyWithIndexedDiagnostics)
{
    const JsonImportResult imported = import_timeline_json("fixtures/partial-julibrot-view.json");
    ASSERT_TRUE(imported.succeeded());
    const std::array<std::string, 19> diagnostics{"paths", "centered", "straight-on", "straight-on", "view-up",
        "normalize", "full frame range", "interpolation", "enum", "JSON number", "component count", "arity", "output",
        "catalog", "field", "view-up", "finite difference", "interpolation", "straight-on"};
    ASSERT_EQ(diagnostics.size(), imported.diagnostics.size());
    ASSERT_EQ(15, imported.document->lane_count());
    EXPECT_EQ("animation-19-geometry[0]", imported.document->strings().lookup(imported.document->lanes()[0].id()));
    for (int index = 0; index < timeline::size_cast(diagnostics); ++index)
    {
        SCOPED_TRACE(index);
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find("animation-" + std::to_string(index) + ":"));
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find(diagnostics[index]));
    }
    const JsonImportResult failed = import_timeline_json("fixtures/invalid-julibrot-view.json");
    EXPECT_FALSE(failed.succeeded());
    EXPECT_FALSE(failed.diagnostics.empty());
}

TEST(JulibrotView, diagnosesSourceGeometryCatalogPoliciesAndOffGridNormalization)
{
    const JsonImportResult imported = import_timeline_json("fixtures/partial-julibrot-sources.json");
    ASSERT_TRUE(imported.succeeded());
    const std::array<std::string, 5> diagnostics{"source geometry", "six values", "numeric", "view-up", "bounds"};
    ASSERT_EQ(diagnostics.size(), imported.diagnostics.size());
    ASSERT_EQ(15, imported.document->lane_count());
    EXPECT_EQ(
        "animation-layer-5-0-geometry[0]", imported.document->strings().lookup(imported.document->lanes()[0].id()));
    for (int index = 0; index < timeline::size_cast(diagnostics); ++index)
    {
        SCOPED_TRACE(index);
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find("animation-layer-" + std::to_string(index)));
        EXPECT_NE(std::string::npos, imported.diagnostics[index].find(diagnostics[index]));
    }
}
