// Copyright (c) 2026 Richard Thomson

#include <ResolvedAttributes.h>

#include <timelineParAnimator/TimelineJson.h>

#include <timeline/Layout.h>
#include <timeline/Query.h>
#include <timeline/size_cast.h>
#include <timeline/Snapshot.h>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using namespace timeline_par_animator;

namespace
{

struct AnalyticPathCase
{
    std::string name;
    std::filesystem::path fixture;
    std::filesystem::path golden;
    std::vector<double> x;
    std::vector<double> y;
    std::vector<std::string> parameters;
    std::string recipe;
    int midpoint_lane;
    double midpoint;
    double tolerance;
    int hit_point;
};

struct AnimationShapeCase
{
    std::string name;
    std::filesystem::path fixture;
    int lanes;
    std::optional<int> tracks;
    std::optional<int> keyframes;
};

const std::vector<AnalyticPathCase> ANALYTIC_PATH_CASES{
    {"CatmullRom", "fixtures/catmull-rom-path.json", "fixtures/gold-catmull-rom-path.par",
        {0.0, 0.4375, 1.0, 2.0, 3.0, 3.5625, 4.0}, {0.0, 1.125, 2.0, 2.25, 2.0, 1.125, 0.0},
        {"0/0", "0.4375/1.125", "1/2", "2/2.25", "3/2", "3.5625/1.125", "4/0"},
        "{\"control-points\":[\"0/0\",\"1/2\",\"3/2\",\"4/0\"],\"kind\":\"catmull-rom\"}", 1, 0.546875, 0.0, 3},
    {"Bezier", "fixtures/bezier-path.json", "fixtures/gold-bezier-path.par", {0.0, 1.0, 2.0, 3.0, 4.0},
        {0.0, 1.5, 2.0, 1.5, 0.0}, {"0/0", "1/1.5", "2/2", "3/1.5", "4/0"},
        "{\"control-points\":[\"0/0\",\"2/4\",\"4/0\"],\"kind\":\"bezier\"}", 1, 0.875, 0.0, 2},
    {"Spiral", "fixtures/spiral-path.json", "fixtures/gold-spiral-path.par", {1.0, 0.0, -2.0, 0.0, 3.0},
        {0.0, 1.5, 0.0, -2.5, 0.0}, {"1/0", "0/1.5", "-2/0", "0/-2.5", "3/0"},
        "{\"center\":\"0/0\",\"from-radius\":1,\"kind\":\"spiral\",\"to-radius\":3,\"turns\":1}", 0,
        1.25 / std::sqrt(2.0), 1e-12, 2},
    {"Lissajous", "fixtures/lissajous-path.json", "fixtures/gold-lissajous-path.par", {2.0, 0.0, -2.0, 0.0, 2.0},
        {0.0, 1.0, 0.0, -1.0, 0.0}, {"2/0", "0/1", "-2/0", "0/-1", "2/0"},
        "{\"center\":\"0/0\",\"kind\":\"lissajous\",\"x-frequency\":1,\"x-radius\":2,\"y-frequency\":1,\"y-radius\":1}",
        0, std::sqrt(2.0), 1e-12, 2},
    {"Ellipse", "fixtures/ellipse-path.json", "fixtures/gold-ellipse-path.par", {2.0, 0.0, -2.0, 0.0, 2.0},
        {0.0, 1.0, 0.0, -1.0, 0.0}, {"2/0", "0/1", "-2/0", "0/-1", "2/0"}, "ellipse", 0, std::sqrt(2.0), 1e-12, 1},
};

const std::vector<AnimationShapeCase> ANIMATION_SHAPE_CASES{
    {"FunctionSlotPwm", "fixtures/function-slot-pwm.json", 2, std::nullopt, std::nullopt},
    {"FunctionSlotVariants", "fixtures/function-slot-variants.json", 4, std::nullopt, std::nullopt},
    {"YesNoPwm", "fixtures/yes-no-pwm.json", 2, 1, 2},
    {"PwmVariants", "fixtures/pwm-variants.json", 8, 4, 8},
    {"CatmullRomTuples", "fixtures/catmull-rom-tuples.json", 6, 3, 0},
    {"BezierTuples", "fixtures/bezier-tuples.json", 15, 6, 0},
    {"PathGenerators", "fixtures/path-generators.json", 3, 2, 0},
    {"CompoundParameters", "fixtures/multi-track.json", 4, 2, 4},
    {"CategoricalHold", "fixtures/inside-hold.json", 1, std::nullopt, 2},
};

void PrintTo(const AnalyticPathCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const AnimationShapeCase &value, std::ostream *stream)
{
    *stream << value.name;
}

std::string analytic_path_name(const testing::TestParamInfo<AnalyticPathCase> &info)
{
    return info.param.name;
}

std::string animation_shape_name(const testing::TestParamInfo<AnimationShapeCase> &info)
{
    return info.param.name;
}

JsonImportResult import_fixture(const std::filesystem::path &fixture)
{
    return import_timeline_json(fixture);
}

timeline::Document import_clean_document(const std::filesystem::path &fixture)
{
    JsonImportResult result = import_fixture(fixture);
    if (!result.succeeded() || !result.diagnostics.empty())
    {
        throw std::runtime_error("animation fixture import failed");
    }
    return std::move(*result.document);
}

std::string read_text(const std::filesystem::path &path)
{
    std::ifstream file(path);
    if (!file)
    {
        throw std::runtime_error("unable to open animation golden file");
    }
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

std::string golden_frame(const std::string &golden, int frame)
{
    const std::size_t start = golden.find("frame-000" + std::to_string(frame + 1) + " {");
    if (start == std::string::npos)
    {
        throw std::runtime_error("animation golden frame not found");
    }
    return golden.substr(start, golden.find('}', start) - start);
}

const timeline::Curve &curve(const timeline::Document &document, int lane)
{
    return std::get<timeline::Curve>(document.lanes()[lane].items().front());
}

std::vector<timeline::Polyline> rendered_curves(const timeline::Document &document)
{
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Layout layout(document, timeline::Viewport(500, 140, grid.offset(), grid.end_time()),
        timeline::LayoutMetrics(100, 20, 30, 4));
    std::vector<timeline::Polyline> result;
    for (const timeline::Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<timeline::Polyline>(primitive))
        {
            const timeline::Polyline &line = std::get<timeline::Polyline>(primitive);
            if (line.style == timeline::StyleRole::CURVE)
            {
                result.push_back(line);
            }
        }
    }
    return result;
}

/// Parameterized animation import with shared document access.
///
template <typename Definition>
class ParameterizedAnimationImportTest : public testing::TestWithParam<Definition>
{
protected:
    void SetUp() override;
    const JsonImportResult &result() const
    {
        return m_result;
    }
    const timeline::Document &document() const
    {
        return *m_result.document;
    }
    const timeline::FrameGrid &frame_grid() const
    {
        return *document().frame_grid();
    }
    const timeline::Document &composed_music_document();

private:
    JsonImportResult m_result;
    std::optional<timeline::Document> m_composed_music_document;
};

template <typename Definition>
void ParameterizedAnimationImportTest<Definition>::SetUp()
{
    m_result = import_fixture(this->GetParam().fixture);
}

template <typename Definition>
const timeline::Document &ParameterizedAnimationImportTest<Definition>::composed_music_document()
{
    if (!m_composed_music_document)
    {
        m_composed_music_document.emplace(
            timeline::combine_documents(import_clean_document("fixtures/beat-keys/rms.beat-keys.json"), document()));
    }
    return *m_composed_music_document;
}

/// Imported analytic path selected by the active test parameter.
///
class AnalyticPathImportTest : public ParameterizedAnimationImportTest<AnalyticPathCase>
{
};

/// Imported animation shape selected by the active test parameter.
///
class AnimationShapeImportTest : public ParameterizedAnimationImportTest<AnimationShapeCase>
{
};

} // namespace

TEST_P(AnalyticPathImportTest, importsExpectedDocumentShape)
{
    const JsonImportResult &imported = result();

    ASSERT_TRUE(imported.succeeded());
    EXPECT_TRUE(imported.diagnostics.empty());
    ASSERT_TRUE(imported.document);
    EXPECT_EQ(2, imported.document->lane_count());
    EXPECT_EQ(1, imported.document->track_count());
    EXPECT_EQ(0, imported.document->keyframe_count());
}

TEST_P(AnalyticPathImportTest, samplesLikeParanimator)
{
    const AnalyticPathCase &definition = GetParam();
    ASSERT_TRUE(result().succeeded());
    const timeline::Document &imported = document();

    for (int frame = 0; frame < timeline::size_cast(definition.x); ++frame)
    {
        const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(imported, frame);
        ASSERT_TRUE(inspection);
        ASSERT_EQ(2, timeline::size_cast(inspection->lanes));
        ASSERT_TRUE(inspection->lanes[0].items.front().value);
        ASSERT_TRUE(inspection->lanes[1].items.front().value);
        EXPECT_DOUBLE_EQ(definition.x[frame], *inspection->lanes[0].items.front().value);
        EXPECT_DOUBLE_EQ(definition.y[frame], *inspection->lanes[1].items.front().value);
        EXPECT_EQ(timeline::InspectionItemRole::SAMPLED, inspection->lanes[0].items.front().role);
        EXPECT_EQ(timeline::InspectionItemRole::SAMPLED, inspection->lanes[1].items.front().role);
    }
}

TEST_P(AnalyticPathImportTest, matchesParanimatorGoldenOutput)
{
    const AnalyticPathCase &definition = GetParam();
    const std::string golden = read_text(definition.golden);

    std::vector<std::string> frames;
    for (int frame = 0; frame < timeline::size_cast(definition.parameters); ++frame)
    {
        frames.push_back(golden_frame(golden, frame));
    }

    ASSERT_EQ(timeline::size_cast(definition.parameters), timeline::size_cast(frames));
    for (int frame = 0; frame < timeline::size_cast(frames); ++frame)
    {
        EXPECT_NE(std::string::npos, frames[frame].find("params=" + definition.parameters[frame]));
    }
}

TEST_P(AnalyticPathImportTest, preservesOwnedRecipeMetadata)
{
    const AnalyticPathCase &definition = GetParam();
    ASSERT_TRUE(result().succeeded());
    ASSERT_TRUE(result().diagnostics.empty());
    const timeline::Document &imported = document();
    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(imported, 0);
    ASSERT_TRUE(inspection);

    const ResolvedAttributes first = resolved_attributes(imported, inspection->lanes[0].items.front().attributes);
    const ResolvedAttributes second = resolved_attributes(imported, inspection->lanes[1].items.front().attributes);

    EXPECT_EQ("params.c", first.at("parameter"));
    EXPECT_EQ("animation-0", first.at("track"));
    EXPECT_EQ("0", first.at("component"));
    EXPECT_EQ("1", second.at("component"));
    if (definition.name == "Ellipse")
    {
        EXPECT_NE(std::string::npos, first.at("path").find(definition.recipe));
        EXPECT_NE(std::string::npos, second.at("path").find(definition.recipe));
    }
    else
    {
        EXPECT_EQ(definition.recipe, first.at("path"));
        EXPECT_EQ(definition.recipe, second.at("path"));
    }
}

TEST_P(AnalyticPathImportTest, retainsAnalyticDefinition)
{
    const AnalyticPathCase &definition = GetParam();
    ASSERT_TRUE(result().succeeded());
    ASSERT_TRUE(result().diagnostics.empty());
    const timeline::Curve &item = curve(document(), definition.midpoint_lane);
    const timeline::FrameGrid &grid = frame_grid();

    const double midpoint =
        item.sample(grid.offset() + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2));

    EXPECT_EQ(timeline::CurveInterpolation::ANALYTIC, item.interpolation());
    EXPECT_EQ(0, item.sample_count());
    if (definition.tolerance == 0.0)
    {
        EXPECT_DOUBLE_EQ(definition.midpoint, midpoint);
    }
    else
    {
        EXPECT_NEAR(definition.midpoint, midpoint, definition.tolerance);
    }
}

TEST_P(AnalyticPathImportTest, laysOutOnePolylinePerComponent)
{
    const AnalyticPathCase &definition = GetParam();
    ASSERT_TRUE(result().succeeded());
    ASSERT_TRUE(result().diagnostics.empty());

    const std::vector<timeline::Polyline> lines = rendered_curves(document());

    ASSERT_EQ(2, timeline::size_cast(lines));
    EXPECT_EQ(timeline::size_cast(definition.x), timeline::size_cast(lines[0].points));
    EXPECT_EQ(timeline::size_cast(definition.x), timeline::size_cast(lines[1].points));
}

TEST_P(AnalyticPathImportTest, hitTestsRenderedComponents)
{
    const AnalyticPathCase &definition = GetParam();
    ASSERT_TRUE(result().succeeded());
    ASSERT_TRUE(result().diagnostics.empty());
    const timeline::Document &imported = document();
    const timeline::FrameGrid &grid = frame_grid();
    const timeline::Layout layout(imported, timeline::Viewport(500, 140, grid.offset(), grid.end_time()),
        timeline::LayoutMetrics(100, 20, 30, 4));
    const std::vector<timeline::Polyline> lines = rendered_curves(imported);

    const std::optional<timeline::HitResult> first = layout.hit_test(lines[0].points[definition.hit_point], 2);
    const std::optional<timeline::HitResult> second = layout.hit_test(lines[1].points[definition.hit_point], 2);

    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_EQ(lines[0].id.item_id, first->id.item_id);
    EXPECT_EQ(lines[1].id.item_id, second->id.item_id);
}

INSTANTIATE_TEST_SUITE_P(
    AnalyticPaths, AnalyticPathImportTest, testing::ValuesIn(ANALYTIC_PATH_CASES), analytic_path_name);

TEST_P(AnimationShapeImportTest, importsExpectedCounts)
{
    const AnimationShapeCase &definition = GetParam();
    const JsonImportResult &imported = result();

    ASSERT_TRUE(imported.succeeded());
    ASSERT_TRUE(imported.document);
    EXPECT_EQ(definition.lanes, imported.document->lane_count());
    if (definition.tracks)
    {
        EXPECT_EQ(*definition.tracks, imported.document->track_count());
    }
    if (definition.keyframes)
    {
        EXPECT_EQ(*definition.keyframes, imported.document->keyframe_count());
    }
}

INSTANTIATE_TEST_SUITE_P(
    AnimationShapes, AnimationShapeImportTest, testing::ValuesIn(ANIMATION_SHAPE_CASES), animation_shape_name);

TEST(AnimationImport, preservesSourceFunctionSamplesLikeParanimator)
{
    const JsonImportResult result = import_fixture("fixtures/function-slot-pwm.json");
    const std::array<std::string, 4> values{"sin/tan", "sin/tan", "sin/log", "sin/log"};
    const std::string golden = read_text("fixtures/gold-function-slot-pwm.par");
    ASSERT_TRUE(result.succeeded());

    const timeline::Document &document = *result.document;

    for (int frame = 0; frame < 4; ++frame)
    {
        const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(document, frame);
        ASSERT_TRUE(inspection);
        EXPECT_EQ(
            values[frame], resolved_attributes(document, inspection->lanes[0].items.front().attributes).at("value"));
        ASSERT_TRUE(inspection->lanes[1].value);
        EXPECT_DOUBLE_EQ(frame / 3.0, *inspection->lanes[1].value);
        EXPECT_NE(std::string::npos, golden_frame(golden, frame).find("function=" + values[frame]));
    }
}

TEST(AnimationImport, preservesSourceFunctionMetadata)
{
    const JsonImportResult result = import_fixture("fixtures/function-slot-pwm.json");
    ASSERT_TRUE(result.succeeded());

    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, 0);
    ASSERT_TRUE(inspection);
    const ResolvedAttributes attributes = resolved_attributes(result, inspection->lanes[0].items.front().attributes);

    EXPECT_EQ("1", attributes.at("slot"));
    EXPECT_EQ("sin/cos", attributes.at("source-value"));
    EXPECT_EQ("Function_Demo", attributes.at("source-entry"));
    EXPECT_EQ("function[1]", attributes.at("parameter"));
}

TEST(AnimationImport, laysOutSourceFunctionOutput)
{
    const timeline::Document document = import_clean_document("fixtures/function-slot-pwm.json");
    const timeline::FrameGrid &grid = *document.frame_grid();

    const timeline::Layout layout(document, timeline::Viewport(500, 140, grid.offset(), grid.end_time()),
        timeline::LayoutMetrics(100, 20, 30, 4));
    const std::string snapshot = timeline::render_snapshot(layout.display_list());

    EXPECT_NE(std::string::npos, snapshot.find("function[1] / mix"));
}

TEST(AnimationImport, hitTestsSourceFunctionOutput)
{
    const timeline::Document document = import_clean_document("fixtures/function-slot-pwm.json");
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Layout layout(document, timeline::Viewport(500, 140, grid.offset(), grid.end_time()),
        timeline::LayoutMetrics(100, 20, 30, 4));

    const std::optional<timeline::HitResult> hit = layout.hit_test({350, 35}, 2);

    ASSERT_TRUE(hit);
    EXPECT_EQ("animation-0-pwm-2", document.strings().lookup(hit->id.item_id));
}

TEST(AnimationImport, retainsAuthoredFunctionSlots)
{
    const timeline::Document document = import_clean_document("fixtures/function-slot-variants.json");
    const std::array<std::string, 4> values{"log/cos", "tan/cos", "log/cos", "tan/cos"};

    std::vector<std::string> actual;
    for (int frame = 0; frame < 4; ++frame)
    {
        const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(document, frame);
        actual.push_back(
            std::string(resolved_attributes(document, inspection->lanes[0].items.front().attributes).at("value")));
    }

    EXPECT_EQ(std::vector<std::string>(values.begin(), values.end()), actual);
}

TEST(AnimationImport, fillsMissingFunctionSlotsWithIdent)
{
    const timeline::Document document = import_clean_document("fixtures/function-slot-variants.json");

    std::vector<std::string> actual;
    for (int frame = 0; frame < 4; ++frame)
    {
        const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(document, frame);
        actual.push_back(
            std::string(resolved_attributes(document, inspection->lanes[2].items.front().attributes).at("value")));
    }

    EXPECT_EQ(
        (std::vector<std::string>{"sin/cos/ident/tan", "sin/cos/ident/tan", "sin/cos/ident/log", "sin/cos/ident/log"}),
        actual);
}

TEST(AnimationImport, copiesFunctionSlotsThroughComposition)
{
    const timeline::Document document = import_clean_document("fixtures/function-slot-variants.json");
    const timeline::Document music = import_clean_document("fixtures/beat-keys/rms.beat-keys.json");

    const timeline::Document combined = timeline::combine_documents(music, document);
    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(combined, 2);

    ASSERT_TRUE(inspection);
    EXPECT_EQ(8, combined.lane_count());
    EXPECT_EQ("log/cos", resolved_attributes(combined, inspection->lanes[4].items.front().attributes).at("value"));
}

TEST(AnimationImport, diagnosesInvalidFunctionSlotsByIndex)
{
    const JsonImportResult result = import_fixture("fixtures/partial-function-slot-pwm.json");

    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(6, timeline::size_cast(result.diagnostics));
    for (int index = 0; index < 6; ++index)
    {
        EXPECT_NE(std::string::npos, result.diagnostics[index].find("animation-" + std::to_string(index)));
    }
}

TEST(AnimationImport, retainsValidFunctionSlotOutput)
{
    const JsonImportResult result = import_fixture("fixtures/partial-function-slot-pwm.json");
    ASSERT_TRUE(result.succeeded());

    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, 2);
    ASSERT_TRUE(inspection);

    EXPECT_EQ(2, result.document->lane_count());
    EXPECT_EQ("animation-6", result.document->strings().lookup(result.document->lanes().front().id()));
    EXPECT_EQ("sin/log", resolved_attributes(result, inspection->lanes.front().items.front().attributes).at("value"));
}

TEST(AnimationImport, resolvesLayerAndCatalogFunctionSources)
{
    const JsonImportResult result = import_fixture("fixtures/function-slot-sources.json");
    ASSERT_TRUE(result.succeeded());

    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, 2);
    ASSERT_TRUE(inspection);

    EXPECT_EQ("log/exp", resolved_attributes(result, inspection->lanes[0].items.front().attributes).at("value"));
    EXPECT_EQ("fractal", resolved_attributes(result, inspection->lanes[0].items.front().attributes).at("layer"));
    EXPECT_EQ(
        "ident/ident/log", resolved_attributes(result, inspection->lanes[2].items.front().attributes).at("value"));
    EXPECT_EQ("sin/log/exp", resolved_attributes(result, inspection->lanes[4].items.front().attributes).at("value"));
    EXPECT_EQ(
        "sin/cos/exp", resolved_attributes(result, inspection->lanes[4].items.front().attributes).at("source-value"));
}

TEST(AnimationImport, diagnosesUndeclaredLayerFunctionSource)
{
    const JsonImportResult result = import_fixture("fixtures/function-slot-sources.json");

    ASSERT_EQ(4, timeline::size_cast(result.diagnostics));
    EXPECT_NE(std::string::npos, result.diagnostics[0].find("animation-layer-0-1"));
    EXPECT_NE(std::string::npos, result.diagnostics[0].find("not declared"));
}

TEST(AnimationImport, diagnosesMissingParFunctionSource)
{
    const JsonImportResult result = import_fixture("fixtures/function-slot-sources.json");

    ASSERT_EQ(4, timeline::size_cast(result.diagnostics));
    EXPECT_NE(std::string::npos, result.diagnostics[1].find("unable to open PAR source"));
}

TEST(AnimationImport, diagnosesMissingFunctionSourceEntry)
{
    const JsonImportResult result = import_fixture("fixtures/function-slot-sources.json");

    ASSERT_EQ(4, timeline::size_cast(result.diagnostics));
    EXPECT_NE(std::string::npos, result.diagnostics[2].find("entry not found"));
}

TEST(AnimationImport, diagnosesUnterminatedFunctionSourceEntry)
{
    const JsonImportResult result = import_fixture("fixtures/function-slot-sources.json");

    ASSERT_EQ(4, timeline::size_cast(result.diagnostics));
    EXPECT_NE(std::string::npos, result.diagnostics[3].find("unterminated PAR source entry"));
}

TEST(AnimationImport, samplesPwmOutputLikeParanimator)
{
    const JsonImportResult result = import_fixture("fixtures/yes-no-pwm.json");
    const std::array<std::string, 4> values{"no", "no", "yes", "yes"};
    const std::string golden = read_text("fixtures/gold-yes-no-pwm.par");
    ASSERT_TRUE(result.succeeded());

    for (int frame = 0; frame < 4; ++frame)
    {
        const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, frame);
        ASSERT_TRUE(inspection);
        EXPECT_EQ(
            values[frame], resolved_attributes(result, inspection->lanes[0].items.front().attributes).at("value"));
        ASSERT_TRUE(inspection->lanes[1].value);
        EXPECT_DOUBLE_EQ(frame / 3.0, *inspection->lanes[1].value);
        EXPECT_NE(std::string::npos, golden_frame(golden, frame).find("showorbit=" + values[frame]));
    }
}

TEST(AnimationImport, preservesPwmRecipeMetadata)
{
    const JsonImportResult result = import_fixture("fixtures/yes-no-pwm.json");
    ASSERT_TRUE(result.succeeded());

    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, 0);
    ASSERT_TRUE(inspection);
    const timeline::InspectionItem &item = inspection->lanes[0].items.front();
    const ResolvedAttributes attributes = resolved_attributes(result, item.attributes);

    EXPECT_EQ(timeline::InspectionItemType::INTERVAL, item.type);
    EXPECT_FALSE(item.value);
    EXPECT_EQ("pwm", attributes.at("mode"));
    EXPECT_NE(std::string::npos, attributes.at("pwm").find("duty"));
}

TEST(AnimationImport, representsPwmOutputAsFrameAlignedIntervals)
{
    const timeline::Document document = import_clean_document("fixtures/yes-no-pwm.json");
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Lane &output = document.lanes()[0];
    ASSERT_EQ(2, output.item_count());

    const timeline::Interval &first = std::get<timeline::Interval>(output.items().front());
    const timeline::Interval &last = std::get<timeline::Interval>(output.items().back());

    EXPECT_EQ(grid.offset(), first.start());
    EXPECT_EQ(grid.frame_start(2), first.end());
    EXPECT_EQ(grid.end_time(), last.end());
}

TEST(AnimationImport, laysOutPwmOutput)
{
    const timeline::Document document = import_clean_document("fixtures/yes-no-pwm.json");
    const timeline::FrameGrid &grid = *document.frame_grid();

    const timeline::Layout layout(document, timeline::Viewport(500, 140, grid.offset(), grid.end_time()),
        timeline::LayoutMetrics(100, 20, 30, 4));
    const std::string snapshot = timeline::render_snapshot(layout.display_list());

    EXPECT_NE(std::string::npos, snapshot.find("showorbit / mix"));
}

TEST(AnimationImport, hitTestsPwmOutput)
{
    const timeline::Document document = import_clean_document("fixtures/yes-no-pwm.json");
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Layout layout(document, timeline::Viewport(500, 140, grid.offset(), grid.end_time()),
        timeline::LayoutMetrics(100, 20, 30, 4));
    const timeline::Interval &first = std::get<timeline::Interval>(document.lanes()[0].items().front());

    const std::optional<timeline::HitResult> hit = layout.hit_test({150, 35}, 2);

    ASSERT_TRUE(hit);
    EXPECT_EQ(first.id(), hit->id.item_id);
}

TEST(AnimationImport, preservesPwmVariantSemantics)
{
    const timeline::Document document = import_clean_document("fixtures/pwm-variants.json");
    const std::array<std::array<std::string, 8>, 4> values{{
        {"bof60", "bof60", "1", "1", "bof60", "bof60", "1", "1"},
        {"no", "yes", "no", "yes", "no", "yes", "no", "yes"},
        {"no", "no", "no", "no", "show", "show", "show", "show"},
        {"yes", "yes", "no", "yes", "no", "no", "no", "no"},
    }};

    std::vector<std::string> actual;
    for (int frame = 0; frame < 8; ++frame)
    {
        const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(document, frame);
        for (int track = 0; track < 4; ++track)
        {
            actual.push_back(std::string(
                resolved_attributes(document, inspection->lanes[2 * track].items.front().attributes).at("value")));
        }
    }

    ASSERT_EQ(32, timeline::size_cast(actual));
    for (int frame = 0; frame < 8; ++frame)
    {
        for (int track = 0; track < 4; ++track)
        {
            EXPECT_EQ(values[track][frame], actual[4 * frame + track]);
        }
    }
}

TEST(AnimationImport, copiesPwmVariantsThroughComposition)
{
    const timeline::Document document = import_clean_document("fixtures/pwm-variants.json");
    const timeline::Document music = import_clean_document("fixtures/beat-keys/rms.beat-keys.json");

    const timeline::Document combined = timeline::combine_documents(music, document);
    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(combined, 2);

    ASSERT_TRUE(inspection);
    EXPECT_EQ(12, combined.lane_count());
    EXPECT_EQ("1", resolved_attributes(combined, inspection->lanes[4].items.front().attributes).at("value"));
}

TEST(AnimationImport, diagnosesInvalidPwmRecipesByIndex)
{
    const JsonImportResult result = import_fixture("fixtures/partial-pwm.json");

    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(14, timeline::size_cast(result.diagnostics));
    for (int index = 0; index < 14; ++index)
    {
        EXPECT_NE(std::string::npos, result.diagnostics[index].find("animation-" + std::to_string(index)));
    }
    EXPECT_NE(std::string::npos, result.diagnostics[13].find("slot"));
}

TEST(AnimationImport, retainsValidPwmOutput)
{
    const JsonImportResult result = import_fixture("fixtures/partial-pwm.json");
    ASSERT_TRUE(result.succeeded());

    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, 2);
    ASSERT_TRUE(inspection);

    EXPECT_EQ("animation-14", result.document->strings().lookup(result.document->lanes().front().id()));
    EXPECT_EQ("yes", resolved_attributes(result, inspection->lanes.front().items.front().attributes).at("value"));
}

TEST(AnimationImport, preservesCatmullRomBounds)
{
    const timeline::Document document = import_clean_document("fixtures/catmull-rom-path.json");
    const timeline::Curve &item = curve(document, 1);

    ASSERT_TRUE(item.minimum());
    ASSERT_TRUE(item.maximum());
    EXPECT_LE(*item.minimum(), 0.0);
    EXPECT_GE(*item.maximum(), 2.25);
}

TEST(AnimationImport, preservesBezierBounds)
{
    const timeline::Document document = import_clean_document("fixtures/bezier-path.json");
    const timeline::Curve &item = curve(document, 1);

    ASSERT_TRUE(item.minimum());
    ASSERT_TRUE(item.maximum());
    EXPECT_DOUBLE_EQ(0.0, *item.minimum());
    EXPECT_DOUBLE_EQ(4.0, *item.maximum());
}

TEST(AnimationImport, preservesSpiralBounds)
{
    const timeline::Document document = import_clean_document("fixtures/spiral-path.json");
    const timeline::Curve &item = curve(document, 0);

    ASSERT_TRUE(item.minimum());
    ASSERT_TRUE(item.maximum());
    EXPECT_DOUBLE_EQ(-3.0, *item.minimum());
    EXPECT_DOUBLE_EQ(3.0, *item.maximum());
}

TEST(AnimationImport, supportsCatmullRomTupleVariants)
{
    const timeline::Document document = import_clean_document("fixtures/catmull-rom-tuples.json");
    const timeline::FrameGrid &grid = *document.frame_grid();
    const std::array<double, 9> x{0.0, 0.4375, 1.0, 2.0, 3.0, 3.5, 4.0, 4.9375, 6.0};
    const std::array<double, 9> y{0.0, 1.125, 2.0, 2.25, 2.0, 1.125, 0.0, -1.0, -2.0};

    for (int frame = 0; frame < 9; ++frame)
    {
        EXPECT_DOUBLE_EQ(x[frame], curve(document, 0).sample(grid.frame_start(frame)));
        EXPECT_DOUBLE_EQ(y[frame], curve(document, 1).sample(grid.frame_start(frame)));
        EXPECT_DOUBLE_EQ(3.0 * x[frame], curve(document, 2).sample(grid.frame_start(frame)));
    }
}

TEST(AnimationImport, supportsConstantAndLineCatmullRomTupleVariants)
{
    const timeline::Document document = import_clean_document("fixtures/catmull-rom-tuples.json");
    const timeline::FrameGrid &grid = *document.frame_grid();

    for (int frame = 0; frame < 9; ++frame)
    {
        EXPECT_DOUBLE_EQ(3.0, curve(document, 3).sample(grid.frame_start(frame)));
        EXPECT_DOUBLE_EQ(3.0 * frame / 8.0, curve(document, 4).sample(grid.frame_start(frame)));
        EXPECT_DOUBLE_EQ(6.0 * frame / 8.0, curve(document, 5).sample(grid.frame_start(frame)));
    }
}

TEST(AnimationImport, copiesCatmullRomDefinitionsThroughComposition)
{
    const timeline::Document document = import_clean_document("fixtures/catmull-rom-tuples.json");
    const timeline::Document music = import_clean_document("fixtures/beat-keys/rms.beat-keys.json");
    const timeline::FrameGrid &grid = *document.frame_grid();

    const timeline::Document combined = timeline::combine_documents(music, document);
    const timeline::Curve &copy = curve(combined, 5);

    EXPECT_EQ(10, combined.lane_count());
    EXPECT_DOUBLE_EQ(2.25, copy.sample(grid.frame_start(3)));
    EXPECT_EQ(0, copy.sample_count());
}

TEST(AnimationImport, diagnosesMalformedCatmullRomRecipesByIndex)
{
    const JsonImportResult result = import_fixture("fixtures/partial-catmull-rom-paths.json");

    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(10, timeline::size_cast(result.diagnostics));
    for (int index = 0; index < 11; ++index)
    {
        if (index != 7)
        {
            const int diagnostic = index < 7 ? index : index - 1;
            EXPECT_NE(std::string::npos, result.diagnostics[diagnostic].find("animation-" + std::to_string(index)));
        }
    }
    EXPECT_NE(std::string::npos, result.diagnostics[0].find("at least four control points"));
}

TEST(AnimationImport, retainsValidCatmullRomTracks)
{
    const JsonImportResult result = import_fixture("fixtures/partial-catmull-rom-paths.json");
    ASSERT_TRUE(result.succeeded());

    const timeline::Curve &item = curve(*result.document, 3);

    EXPECT_EQ(4, result.document->lane_count());
    EXPECT_EQ("animation-7[0]", result.document->strings().lookup(result.document->lanes()[0].id()));
    EXPECT_EQ("true", resolved_attributes(result, curve(*result.document, 0).attributes()).at("normalize"));
    EXPECT_EQ("animation-11[0]", result.document->strings().lookup(result.document->lanes()[2].id()));
    EXPECT_DOUBLE_EQ(2.25, item.sample(result.document->frame_grid()->frame_start(3)));
}

TEST(AnimationImport, rejectsSingleFrameCatmullRomPath)
{
    const JsonImportResult result = import_fixture("fixtures/single-frame-catmull-rom.json");

    EXPECT_FALSE(result.succeeded());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_NE(std::string::npos, result.diagnostics.front().find("at least two frames"));
}

TEST(AnimationImport, supportsBezierTupleVariants)
{
    const timeline::Document document = import_clean_document("fixtures/bezier-tuples.json");
    const timeline::FrameGrid &grid = *document.frame_grid();
    const std::array<double, 15> midpoint{2.0, 4.0, 0.5, 0.5, 2.0, 2.0, 5.0, 2.0, 3.0, 1.0, 1.0, 2.0, 3.0, 4.0, 2.0};

    for (int index = 0; index < document.lane_count(); ++index)
    {
        EXPECT_DOUBLE_EQ(midpoint[index], curve(document, index).sample(grid.frame_start(2)));
        EXPECT_EQ(0, curve(document, index).sample_count());
        EXPECT_NE(std::string::npos,
            resolved_attributes(document, curve(document, index).attributes()).at("path").find("control-points"));
    }
}

TEST(AnimationImport, supportsQuarticBezierVariant)
{
    const timeline::Document document = import_clean_document("fixtures/bezier-tuples.json");
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Curve &quartic = curve(document, 10);

    const std::array<double, 3> values{
        quartic.sample(grid.frame_start(0)), quartic.sample(grid.frame_start(4)), quartic.sample(grid.frame_start(1))};

    EXPECT_DOUBLE_EQ(0.0, values[0]);
    EXPECT_DOUBLE_EQ(16.0, values[1]);
    EXPECT_DOUBLE_EQ(0.0625, values[2]);
}

TEST(AnimationImport, copiesBezierDefinitionsThroughComposition)
{
    const timeline::Document document = import_clean_document("fixtures/bezier-tuples.json");
    const timeline::Document music = import_clean_document("fixtures/beat-keys/rms.beat-keys.json");
    const timeline::FrameGrid &grid = *document.frame_grid();

    const timeline::Document combined = timeline::combine_documents(music, document);
    const timeline::Curve &copy = curve(combined, 14);

    EXPECT_EQ(19, combined.lane_count());
    EXPECT_DOUBLE_EQ(1.0, copy.sample(grid.frame_start(2)));
    EXPECT_EQ(resolved_attributes(document, curve(document, 10).attributes()).values(),
        resolved_attributes(combined, copy.attributes()).values());
}

TEST(AnimationImport, diagnosesMalformedBezierRecipesByIndex)
{
    const JsonImportResult result = import_fixture("fixtures/partial-bezier-paths.json");

    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(10, timeline::size_cast(result.diagnostics));
    for (int index = 0; index < 11; ++index)
    {
        if (index != 7)
        {
            const int diagnostic = index < 7 ? index : index - 1;
            EXPECT_NE(std::string::npos, result.diagnostics[diagnostic].find("animation-" + std::to_string(index)));
        }
    }
}

TEST(AnimationImport, retainsValidBezierTracks)
{
    const JsonImportResult result = import_fixture("fixtures/partial-bezier-paths.json");
    ASSERT_TRUE(result.succeeded());

    const timeline::FrameGrid &grid = *result.document->frame_grid();

    EXPECT_EQ(6, result.document->lane_count());
    EXPECT_EQ("animation-7[0]", result.document->strings().lookup(result.document->lanes()[0].id()));
    EXPECT_EQ("true", resolved_attributes(result, curve(*result.document, 0).attributes()).at("normalize"));
    EXPECT_EQ("animation-11[0]", result.document->strings().lookup(result.document->lanes()[2].id()));
    EXPECT_DOUBLE_EQ(1.0, curve(*result.document, 2).sample(grid.frame_start(2)));
    EXPECT_DOUBLE_EQ(4.0, curve(*result.document, 5).sample(grid.frame_start(2)));
}

TEST(AnimationImport, rejectsSingleFrameBezierPath)
{
    const JsonImportResult result = import_fixture("fixtures/single-frame-bezier.json");

    EXPECT_FALSE(result.succeeded());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_NE(std::string::npos, result.diagnostics.front().find("at least two frames"));
}

TEST(AnimationImport, supportsReverseShrinkingSpiralVariant)
{
    const timeline::Document document = import_clean_document("fixtures/spiral-variants.json");
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Curve &x = curve(document, 0);
    const timeline::Curve &y = curve(document, 1);

    const std::array<double, 8> values{x.sample(grid.frame_start(0)), y.sample(grid.frame_start(0)),
        x.sample(grid.frame_start(2)), y.sample(grid.frame_start(2)), x.sample(grid.frame_start(4)),
        y.sample(grid.frame_start(4)), *x.minimum(), *x.maximum()};

    EXPECT_NEAR(1.0, values[0], 1e-12);
    EXPECT_DOUBLE_EQ(1.0, values[1]);
    EXPECT_DOUBLE_EQ(3.0, values[2]);
    EXPECT_DOUBLE_EQ(-2.0, values[3]);
    EXPECT_NEAR(1.0, values[4], 1e-12);
    EXPECT_DOUBLE_EQ(-3.0, values[5]);
    EXPECT_DOUBLE_EQ(-2.0, values[6]);
    EXPECT_DOUBLE_EQ(4.0, values[7]);
}

TEST(AnimationImport, supportsTranslatedSpiralVariant)
{
    const timeline::Document document = import_clean_document("fixtures/spiral-variants.json");
    const timeline::FrameGrid &grid = *document.frame_grid();

    const double x = curve(document, 0).sample(grid.frame_start(1));
    const double y = curve(document, 1).sample(grid.frame_start(1));

    EXPECT_NEAR(1.0 + 2.5 / std::sqrt(2.0), x, 1e-12);
    EXPECT_NEAR(-2.0 + 2.5 / std::sqrt(2.0), y, 1e-12);
}

TEST(AnimationImport, constantRadiusSpiralMatchesCircle)
{
    const timeline::Document document = import_clean_document("fixtures/spiral-variants.json");
    const timeline::Document circle = import_clean_document("fixtures/circle-path.json");
    const timeline::FrameGrid &grid = *document.frame_grid();

    std::vector<double> differences;
    for (int component = 0; component < 2; ++component)
    {
        for (int frame = 0; frame < 5; ++frame)
        {
            differences.push_back(curve(document, component + 2).sample(grid.frame_start(frame)) -
                curve(circle, component).sample(grid.frame_start(frame)));
        }
    }

    for (double difference : differences)
    {
        EXPECT_DOUBLE_EQ(0.0, difference);
    }
}

TEST(AnimationImport, copiesSpiralDefinitionsThroughComposition)
{
    const timeline::Document document = import_clean_document("fixtures/spiral-variants.json");
    const timeline::Document music = import_clean_document("fixtures/beat-keys/rms.beat-keys.json");
    const timeline::FrameGrid &grid = *document.frame_grid();

    const timeline::Document combined = timeline::combine_documents(music, document);
    const timeline::Curve &copy = curve(combined, 4);

    EXPECT_EQ(8, combined.lane_count());
    EXPECT_DOUBLE_EQ(3.0, copy.sample(grid.frame_start(2)));
    EXPECT_EQ(resolved_attributes(document, curve(document, 0).attributes()).values(),
        resolved_attributes(combined, copy.attributes()).values());
}

TEST(AnimationImport, diagnosesInvalidSpiralRecipesByIndex)
{
    const JsonImportResult result = import_fixture("fixtures/partial-spiral-paths.json");

    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(8, timeline::size_cast(result.diagnostics));
    for (int index = 0; index < 8; ++index)
    {
        EXPECT_NE(std::string::npos, result.diagnostics[index].find("animation-" + std::to_string(index)));
    }
}

TEST(AnimationImport, keepsSpiralDefaults)
{
    const JsonImportResult result = import_fixture("fixtures/partial-spiral-paths.json");
    ASSERT_TRUE(result.succeeded());
    const timeline::FrameGrid &grid = *result.document->frame_grid();

    const std::array<double, 5> values{curve(*result.document, 0).sample(grid.frame_start(0)),
        curve(*result.document, 1).sample(grid.frame_start(0)), curve(*result.document, 0).sample(grid.frame_start(2)),
        curve(*result.document, 1).sample(grid.frame_start(2)), curve(*result.document, 0).sample(grid.frame_start(4))};

    EXPECT_EQ("animation-8[0]", result.document->strings().lookup(result.document->lanes()[0].id()));
    EXPECT_DOUBLE_EQ(1.0, values[0]);
    EXPECT_DOUBLE_EQ(-2.0, values[1]);
    EXPECT_DOUBLE_EQ(0.0, values[2]);
    EXPECT_DOUBLE_EQ(-2.0, values[3]);
    EXPECT_DOUBLE_EQ(3.0, values[4]);
}

TEST(AnimationImport, acceptsZeroRadiusSpiral)
{
    const timeline::Document document = *import_fixture("fixtures/partial-spiral-paths.json").document;
    const timeline::FrameGrid &grid = *document.frame_grid();

    const double x = curve(document, 2).sample(grid.frame_start(3));
    const double y = curve(document, 3).sample(grid.frame_start(3));

    EXPECT_DOUBLE_EQ(1.0, x);
    EXPECT_DOUBLE_EQ(-2.0, y);
}

TEST(AnimationImport, keepsLissajousFrequenciesIndependentAndPhaseOnXOnly)
{
    const timeline::Document document = import_clean_document("fixtures/lissajous-phase.json");
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Curve &x = curve(document, 0);
    const timeline::Curve &y = curve(document, 1);

    const std::array<double, 8> values{x.sample(grid.frame_start(0)), y.sample(grid.frame_start(0)),
        x.sample(grid.frame_start(1)), y.sample(grid.frame_start(1)), x.sample(grid.frame_start(2)),
        y.sample(grid.frame_start(2)), x.sample(grid.frame_start(4)), y.sample(grid.frame_start(4))};

    EXPECT_NEAR(2.0, values[0], 1e-12);
    EXPECT_DOUBLE_EQ(-2.0, values[1]);
    EXPECT_NEAR(1.0 + (std::sqrt(2.0) - std::sqrt(6.0)) / 2.0, values[2], 1e-12);
    EXPECT_NEAR(-2.0 + 3.0 * std::sqrt(2.0) / 2.0, values[3], 1e-12);
    EXPECT_NEAR(1.0 - std::sqrt(3.0), values[4], 1e-12);
    EXPECT_DOUBLE_EQ(-5.0, values[5]);
    EXPECT_DOUBLE_EQ(0.0, values[6]);
    EXPECT_NEAR(-2.0, values[7], 1e-12);
}

TEST(AnimationImport, copiesLissajousDefinitionsThroughComposition)
{
    const timeline::Document document = import_clean_document("fixtures/lissajous-phase.json");
    const timeline::Document music = import_clean_document("fixtures/beat-keys/rms.beat-keys.json");
    const timeline::FrameGrid &grid = *document.frame_grid();

    const timeline::Document combined = timeline::combine_documents(music, document);
    const timeline::Curve &copy = curve(combined, 4);

    EXPECT_EQ(6, combined.lane_count());
    EXPECT_NEAR(1.0 - std::sqrt(3.0), copy.sample(grid.frame_start(2)), 1e-12);
    EXPECT_EQ(resolved_attributes(document, curve(document, 0).attributes()).values(),
        resolved_attributes(combined, copy.attributes()).values());
}

TEST(AnimationImport, diagnosesInvalidLissajousRecipesByIndex)
{
    const JsonImportResult result = import_fixture("fixtures/partial-lissajous-paths.json");

    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(8, timeline::size_cast(result.diagnostics));
    for (int index = 0; index < 8; ++index)
    {
        EXPECT_NE(std::string::npos, result.diagnostics[index].find("animation-" + std::to_string(index)));
    }
}

TEST(AnimationImport, acceptsZeroRadiusLissajous)
{
    const JsonImportResult result = import_fixture("fixtures/partial-lissajous-paths.json");
    ASSERT_TRUE(result.succeeded());

    const timeline::Time time = result.document->frame_grid()->frame_start(3);

    EXPECT_EQ("animation-8[0]", result.document->strings().lookup(result.document->lanes()[0].id()));
    EXPECT_DOUBLE_EQ(1.0, curve(*result.document, 0).sample(time));
    EXPECT_DOUBLE_EQ(-2.0, curve(*result.document, 1).sample(time));
}

TEST(AnimationImport, honorsCirclePhaseAndReverseTurns)
{
    const timeline::Document document = import_clean_document("fixtures/circle-path.json");
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Curve &x = curve(document, 0);
    const timeline::Curve &y = curve(document, 1);

    const std::array<double, 6> values{x.sample(grid.frame_start(1)), y.sample(grid.frame_start(1)),
        x.sample(grid.frame_start(2)), y.sample(grid.frame_start(2)), x.sample(grid.frame_start(4)),
        y.sample(grid.frame_start(4))};

    EXPECT_NEAR(1.0 + std::sqrt(2.0), values[0], 1e-12);
    EXPECT_NEAR(-2.0 + std::sqrt(2.0), values[1], 1e-12);
    EXPECT_DOUBLE_EQ(3.0, values[2]);
    EXPECT_DOUBLE_EQ(-2.0, values[3]);
    EXPECT_NEAR(1.0, values[4], 1e-12);
    EXPECT_DOUBLE_EQ(-4.0, values[5]);
}

TEST(AnimationImport, copiesCircleDefinitionsThroughComposition)
{
    const timeline::Document document = import_clean_document("fixtures/circle-path.json");
    const timeline::Document music = import_clean_document("fixtures/beat-keys/rms.beat-keys.json");
    const timeline::FrameGrid &grid = *document.frame_grid();

    const timeline::Document combined = timeline::combine_documents(music, document);
    const timeline::Curve &copy = curve(combined, 4);

    EXPECT_EQ(6, combined.lane_count());
    EXPECT_DOUBLE_EQ(3.0, copy.sample(grid.frame_start(2)));
    EXPECT_EQ(resolved_attributes(document, curve(document, 0).attributes()).values(),
        resolved_attributes(combined, copy.attributes()).values());
}

TEST(AnimationImport, diagnosesInvalidPlanarPathsByIndex)
{
    const JsonImportResult result = import_fixture("fixtures/partial-planar-paths.json");

    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(10, timeline::size_cast(result.diagnostics));
    for (int index = 0; index < 10; ++index)
    {
        EXPECT_NE(std::string::npos, result.diagnostics[index].find("animation-" + std::to_string(index)));
    }
}

TEST(AnimationImport, keepsZeroRadiusPlanarDefaults)
{
    const JsonImportResult result = import_fixture("fixtures/partial-planar-paths.json");
    ASSERT_TRUE(result.succeeded());

    const timeline::Time time = result.document->frame_grid()->frame_start(3);

    EXPECT_EQ("animation-10[0]", result.document->strings().lookup(result.document->lanes()[0].id()));
    EXPECT_DOUBLE_EQ(1.0, curve(*result.document, 0).sample(time));
    EXPECT_DOUBLE_EQ(-2.0, curve(*result.document, 1).sample(time));
}

TEST(AnimationImport, samplesConstantAndLinePathsLikeParanimator)
{
    const JsonImportResult result = import_fixture("fixtures/path-generators.json");
    const std::string golden = read_text("fixtures/gold-path-generators.par");
    ASSERT_TRUE(result.succeeded());

    std::vector<std::array<double, 3>> values;
    for (int frame = 0; frame < 3; ++frame)
    {
        const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, frame);
        values.push_back({*inspection->lanes[0].value, *inspection->lanes[1].value, *inspection->lanes[2].value});
        EXPECT_NE(std::string::npos, golden_frame(golden, frame).find("maxiter=321"));
        EXPECT_NE(std::string::npos,
            golden_frame(golden, frame).find("params=" + std::to_string(frame) + "/" + std::to_string(frame + 1)));
    }

    for (int frame = 0; frame < 3; ++frame)
    {
        EXPECT_DOUBLE_EQ(321.0, values[frame][0]);
        EXPECT_DOUBLE_EQ(static_cast<double>(frame), values[frame][1]);
        EXPECT_DOUBLE_EQ(static_cast<double>(frame + 1), values[frame][2]);
    }
}

TEST(AnimationImport, preservesConstantAndLineRecipes)
{
    const JsonImportResult result = import_fixture("fixtures/path-generators.json");
    ASSERT_TRUE(result.succeeded());
    const timeline::Keyframe &constant = std::get<timeline::Keyframe>(result.document->lanes()[0].items().front());
    const timeline::Lane &line = result.document->lanes()[1];

    std::vector<ResolvedAttributes> line_attributes;
    for (const timeline::Item &item : line.items())
    {
        line_attributes.push_back(resolved_attributes(result, std::get<timeline::Keyframe>(item).attributes()));
    }

    EXPECT_EQ(timeline::KeyframeInterpolation::HOLD, constant.interpolation());
    EXPECT_EQ(
        "{\"kind\":\"constant\",\"value\":\"321\"}", resolved_attributes(result, constant.attributes()).at("path"));
    for (const ResolvedAttributes &attributes : line_attributes)
    {
        EXPECT_EQ("{\"from\":\"0/1\",\"kind\":\"line\",\"to\":\"2/3\"}", attributes.at("path"));
        EXPECT_EQ("params.c", attributes.at("parameter"));
    }
}

TEST(AnimationImport, interpolatesLinePathBetweenFrames)
{
    const timeline::Document document = import_clean_document("fixtures/path-generators.json");
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Lane &line = document.lanes()[1];

    const std::optional<double> value = line.evaluate_keyframes(
        grid.frame_start(0) + timeline::Duration::from_ticks(grid.frame_duration().ticks() / 2));

    ASSERT_TRUE(value);
    EXPECT_DOUBLE_EQ(0.5, *value);
}

TEST(AnimationImport, laysOutConstantAndLinePaths)
{
    const timeline::Document document = import_clean_document("fixtures/path-generators.json");
    const timeline::FrameGrid &grid = *document.frame_grid();

    const timeline::Layout layout(document, timeline::Viewport(400, 160, grid.offset(), grid.end_time()),
        timeline::LayoutMetrics(100, 20, 30, 4));
    const std::string snapshot = timeline::render_snapshot(layout.display_list());

    EXPECT_NE(std::string::npos, snapshot.find("params.c[0]"));
    EXPECT_NE(std::string::npos, snapshot.find("params.c[1]"));
    EXPECT_NE(std::string::npos, snapshot.find("KEYFRAME_MARKER"));
}

TEST(AnimationImport, diagnosesInvalidPathRecipesByIndex)
{
    const JsonImportResult result = import_fixture("fixtures/partial-paths.json");

    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(7, timeline::size_cast(result.diagnostics));
    for (int index = 0; index < 7; ++index)
    {
        EXPECT_NE(std::string::npos, result.diagnostics[index].find("animation-" + std::to_string(index)));
    }
}

TEST(AnimationImport, retainsValidConstantPath)
{
    const JsonImportResult result = import_fixture("fixtures/partial-paths.json");
    ASSERT_TRUE(result.succeeded());

    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, 1);
    ASSERT_TRUE(inspection);
    const ResolvedAttributes attributes =
        resolved_attributes(result, inspection->lanes.front().items.front().attributes);

    EXPECT_EQ("animation-7", result.document->strings().lookup(result.document->lanes().front().id()));
    EXPECT_EQ("bof60", attributes.at("value"));
    EXPECT_EQ("{\"kind\":\"constant\",\"value\":\"bof60\"}", attributes.at("path"));
}

TEST(AnimationImport, rejectsPathsWithFewerThanTwoFrames)
{
    const JsonImportResult result = import_fixture("fixtures/single-frame-path.json");

    EXPECT_FALSE(result.succeeded());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_NE(std::string::npos, result.diagnostics.front().find("at least two frames"));
}

TEST(AnimationImport, translatesDestinationCurveToOutgoingSegment)
{
    const JsonImportResult result = import_fixture("fixtures/maxiter-step.json");
    ASSERT_TRUE(result.succeeded());
    const timeline::Lane &lane = result.document->lanes().front();
    const timeline::FrameGrid &grid = *result.document->frame_grid();
    const timeline::Keyframe &first = std::get<timeline::Keyframe>(lane.items().front());

    const std::optional<double> before = lane.evaluate_keyframes(grid.frame_start(2));
    const std::optional<double> after = lane.evaluate_keyframes(grid.frame_start(3));

    EXPECT_EQ(timeline::KeyframeInterpolation::HOLD, first.interpolation());
    EXPECT_EQ("step", resolved_attributes(result, first.attributes()).at("outgoing-curve"));
    EXPECT_DOUBLE_EQ(100.0, *before);
    EXPECT_DOUBLE_EQ(200.0, *after);
}

TEST(AnimationImport, splitsCompoundParametersWithoutLosingAuthoredValues)
{
    const JsonImportResult result = import_fixture("fixtures/multi-track.json");
    ASSERT_TRUE(result.succeeded());
    const timeline::FrameGrid &grid = *result.document->frame_grid();
    const timeline::Lane &magnification = result.document->lanes()[2];
    const timeline::Keyframe &first = std::get<timeline::Keyframe>(magnification.items().front());

    const std::optional<double> value = magnification.evaluate_keyframes(grid.frame_start(1));

    EXPECT_EQ(4, result.document->lane_count());
    EXPECT_EQ("center-mag[2]", result.document->strings().lookup(magnification.label()));
    EXPECT_NEAR(std::sqrt(10.0), *value, 1e-12);
    EXPECT_EQ("-0.5/0/1", resolved_attributes(result, first.attributes()).at("value"));
    EXPECT_EQ("center-mag", resolved_attributes(result, first.attributes()).at("parameter"));
    EXPECT_EQ("geometric", resolved_attributes(result, first.attributes()).at("outgoing-curve"));
}

TEST(AnimationImport, inspectsSplitCompoundParameters)
{
    const JsonImportResult result = import_fixture("fixtures/multi-track.json");
    ASSERT_TRUE(result.succeeded());

    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, 1);

    ASSERT_TRUE(inspection);
    ASSERT_TRUE(inspection->lanes[2].value);
    EXPECT_NEAR(std::sqrt(10.0), *inspection->lanes[2].value, 1e-12);
}

TEST(AnimationImport, retainsLayerIdentity)
{
    const JsonImportResult result = import_fixture("fixtures/single-layer.json");
    ASSERT_TRUE(result.succeeded());
    const timeline::Lane &lane = result.document->lanes().front();
    const timeline::Keyframe &first = std::get<timeline::Keyframe>(lane.items().front());

    const std::string label(result.document->strings().lookup(lane.label()));
    const std::string layer(resolved_attributes(result, first.attributes()).at("layer"));

    EXPECT_EQ("base / maxiter", label);
    EXPECT_EQ("base", layer);
}

TEST(AnimationImport, displaysLayerIdentity)
{
    const timeline::Document document = import_clean_document("fixtures/single-layer.json");
    const timeline::FrameGrid &grid = *document.frame_grid();

    const timeline::Layout layout(document, timeline::Viewport(400, 120, grid.offset(), grid.end_time()),
        timeline::LayoutMetrics(100, 20, 30, 4));
    const std::string snapshot = timeline::render_snapshot(layout.display_list());

    EXPECT_NE(std::string::npos, snapshot.find("KEYFRAME_MARKER"));
    EXPECT_NE(std::string::npos, snapshot.find("base / maxiter"));
}

TEST(AnimationImport, inspectsLayerIdentity)
{
    const JsonImportResult result = import_fixture("fixtures/single-layer.json");
    ASSERT_TRUE(result.succeeded());

    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, 1);

    ASSERT_TRUE(inspection);
    EXPECT_EQ("base", resolved_attributes(result, inspection->lanes.front().items.front().attributes).at("layer"));
    EXPECT_DOUBLE_EQ(
        150.0, *result.document->lanes().front().evaluate_keyframes(result.document->frame_grid()->frame_start(1)));
}

TEST(AnimationImport, composesMusicAndAnimationDocuments)
{
    const JsonImportResult music = import_fixture("fixtures/beat-keys/rms.beat-keys.json");
    ASSERT_TRUE(music.succeeded());
    JsonImportOptions options;
    options.frames_per_second_numerator = music.document->frame_grid()->frames_per_second_numerator();
    options.frames_per_second_denominator = music.document->frame_grid()->frames_per_second_denominator();
    const JsonImportResult animation = import_timeline_json("fixtures/maxiter.json", options);
    ASSERT_TRUE(animation.succeeded());

    const timeline::Document combined = timeline::combine_documents(*music.document, *animation.document);

    EXPECT_EQ(5, combined.lane_count());
    EXPECT_EQ(5, combined.frame_grid()->frame_count());
    EXPECT_EQ(11, combined.keyframe_count());
    EXPECT_EQ(music.document->lanes().front().id(), combined.lanes().front().id());
}

TEST(AnimationImport, preservesAnimationTimingThroughComposition)
{
    const JsonImportResult music = import_fixture("fixtures/beat-keys/rms.beat-keys.json");
    JsonImportOptions options;
    options.frames_per_second_numerator = music.document->frame_grid()->frames_per_second_numerator();
    options.frames_per_second_denominator = music.document->frame_grid()->frames_per_second_denominator();
    const JsonImportResult animation = import_timeline_json("fixtures/maxiter.json", options);
    const timeline::Document combined = timeline::combine_documents(*music.document, *animation.document);
    const timeline::Lane &authored = combined.lanes().back();

    const std::optional<double> value = authored.evaluate_keyframes(combined.frame_grid()->frame_start(1));

    EXPECT_EQ("maxiter", combined.strings().lookup(authored.label()));
    EXPECT_DOUBLE_EQ(150.0, *value);
    EXPECT_EQ(std::get<timeline::Keyframe>(animation.document->lanes().front().items().front()).time(),
        std::get<timeline::Keyframe>(authored.items().front()).time());
}

TEST(AnimationImport, rewritesRepeatedAnimationIdentities)
{
    const JsonImportResult music = import_fixture("fixtures/beat-keys/rms.beat-keys.json");
    JsonImportOptions options;
    options.frames_per_second_numerator = music.document->frame_grid()->frames_per_second_numerator();
    options.frames_per_second_denominator = music.document->frame_grid()->frames_per_second_denominator();
    const JsonImportResult animation = import_timeline_json("fixtures/maxiter.json", options);
    const timeline::Document combined = timeline::combine_documents(*music.document, *animation.document);

    const timeline::Document repeated = timeline::combine_documents(combined, *animation.document);

    ASSERT_EQ(6, repeated.lane_count());
    EXPECT_NE(repeated.lanes()[4].id(), repeated.lanes()[5].id());
}

TEST(AnimationImport, laysOutComposedDocuments)
{
    const JsonImportResult music = import_fixture("fixtures/beat-keys/rms.beat-keys.json");
    JsonImportOptions options;
    options.frames_per_second_numerator = music.document->frame_grid()->frames_per_second_numerator();
    options.frames_per_second_denominator = music.document->frame_grid()->frames_per_second_denominator();
    const JsonImportResult animation = import_timeline_json("fixtures/maxiter.json", options);
    const timeline::Document combined = timeline::combine_documents(*music.document, *animation.document);

    const timeline::Layout layout(combined,
        timeline::Viewport(600, 240, combined.frame_grid()->offset(), combined.frame_grid()->end_time()),
        timeline::LayoutMetrics(120, 20, 30, 4));
    const std::string snapshot = timeline::render_snapshot(layout.display_list());

    EXPECT_NE(std::string::npos, snapshot.find("RMS"));
    EXPECT_NE(std::string::npos, snapshot.find("camera.zoom"));
    EXPECT_NE(std::string::npos, snapshot.find("maxiter"));
}

TEST(AnimationImport, rejectsIncompatibleComparisonTiming)
{
    const JsonImportResult music = import_fixture("fixtures/beat-keys/rms.beat-keys.json");
    JsonImportOptions options;
    options.frames_per_second_numerator = 10;
    const JsonImportResult animation = import_timeline_json("fixtures/maxiter.json", options);
    ASSERT_TRUE(music.succeeded());
    ASSERT_TRUE(animation.succeeded());

    const auto combine = [&]
    {
        timeline::combine_documents(*music.document, *animation.document);
    };

    EXPECT_THROW(combine(), std::invalid_argument);
}

TEST(AnimationImport, representsCategoricalValuesAsHeldSpans)
{
    const JsonImportResult result = import_fixture("fixtures/inside-hold.json");
    ASSERT_TRUE(result.succeeded());

    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, 1);
    ASSERT_TRUE(inspection);
    const timeline::InspectionItem &held = inspection->lanes.front().items.front();
    const ResolvedAttributes attributes = resolved_attributes(result, held.attributes);

    EXPECT_FALSE(held.value);
    EXPECT_EQ("bof60", attributes.at("value"));
    EXPECT_EQ("hold", attributes.at("outgoing-curve"));
}

TEST(AnimationImport, representsCategoricalKeysAsInstants)
{
    const JsonImportResult result = import_fixture("fixtures/inside-hold.json");
    ASSERT_TRUE(result.succeeded());

    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(*result.document, 2);

    ASSERT_TRUE(inspection);
    EXPECT_EQ(2, result.document->keyframe_count());
    EXPECT_EQ(4, result.document->lanes().front().item_count());
    for (const timeline::InspectionItem &item : inspection->lanes.front().items)
    {
        EXPECT_EQ("zmag", resolved_attributes(result, item.attributes).at("value"));
    }
}

TEST(AnimationImport, diagnosesInvalidAnimationKeysByIndex)
{
    const JsonImportResult result = import_fixture("fixtures/partial-animation.json");

    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(6, timeline::size_cast(result.diagnostics));
    for (int index = 0; index < 6; ++index)
    {
        EXPECT_NE(std::string::npos, result.diagnostics[index].find("animation-" + std::to_string(index)));
    }
}

TEST(AnimationImport, retainsValidAnimationTracks)
{
    const JsonImportResult result = import_fixture("fixtures/partial-animation.json");

    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(1, result.document->lane_count());
    EXPECT_EQ("animation-6", result.document->strings().lookup(result.document->lanes().front().id()));
}

TEST(AnimationImport, rejectsUnusableAnimation)
{
    const JsonImportResult result = import_fixture("fixtures/invalid-animation.json");

    EXPECT_FALSE(result.succeeded());
    EXPECT_FALSE(result.diagnostics.empty());
}

TEST(AnimationImport, rejectsMissingCatalog)
{
    const JsonImportResult result = import_fixture("fixtures/missing-catalog.json");

    EXPECT_FALSE(result.succeeded());
    EXPECT_FALSE(result.diagnostics.empty());
}
