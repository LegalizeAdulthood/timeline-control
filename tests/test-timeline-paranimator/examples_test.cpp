// Copyright (c) 2026 Richard Thomson

#include <timelineParAnimator/TimelineJson.h>

#include <timeline/Curve.h>
#include <timeline/Event.h>
#include <timeline/Interval.h>
#include <timeline/Keyframe.h>
#include <timeline/Palette.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <ostream>
#include <string>
#include <variant>

using namespace timeline_par_animator;

namespace
{

struct ExampleCase
{
    std::string name;
    std::string filename;
};

void PrintTo(const ExampleCase &example, std::ostream *output)
{
    *output << example.filename;
}

class ExampleImportTest : public testing::TestWithParam<ExampleCase>
{
};

std::string diagnostic_text(const JsonImportResult &result)
{
    std::string text;
    for (const std::string &diagnostic : result.diagnostics)
    {
        text += diagnostic + '\n';
    }
    return text;
}

void expect_all_imported_item_types(const timeline::Document &document)
{
    bool has_instant = false;
    bool has_interval = false;
    bool has_curve = false;
    bool has_keyframe = false;
    bool has_palette = false;
    for (const timeline::Lane &lane : document.lanes())
    {
        for (const timeline::Item &item : lane.items())
        {
            has_instant = has_instant || std::holds_alternative<timeline::Instant>(item);
            has_interval = has_interval || std::holds_alternative<timeline::Interval>(item);
            has_curve = has_curve || std::holds_alternative<timeline::Curve>(item);
            has_keyframe = has_keyframe || std::holds_alternative<timeline::Keyframe>(item);
            has_palette = has_palette || std::holds_alternative<timeline::PaletteCurve>(item);
        }
    }

    EXPECT_TRUE(has_instant);
    EXPECT_TRUE(has_interval);
    EXPECT_TRUE(has_curve);
    EXPECT_TRUE(has_keyframe);
    EXPECT_TRUE(has_palette);
}

TEST_P(ExampleImportTest, importsWithoutDiagnostics)
{
    const ExampleCase &example = GetParam();
    const std::filesystem::path path = std::filesystem::path("examples") / example.filename;

    const JsonImportResult result = import_timeline_json(path);

    ASSERT_TRUE(result.succeeded()) << diagnostic_text(result);
    EXPECT_TRUE(result.diagnostics.empty()) << diagnostic_text(result);
    EXPECT_FALSE(result.document->lanes().empty());
}

TEST(ViewerExample, allFeaturesIncludesEveryImportedItemType)
{
    const std::filesystem::path path = "examples/all-features.json";

    const JsonImportResult result = import_timeline_json(path);

    ASSERT_TRUE(result.succeeded()) << diagnostic_text(result);
    ASSERT_TRUE(result.diagnostics.empty()) << diagnostic_text(result);
    expect_all_imported_item_types(*result.document);
}

INSTANTIATE_TEST_SUITE_P(ViewerExamples, ExampleImportTest,
    testing::Values(ExampleCase{"allFeatures", "all-features.json"},
        ExampleCase{"categoricalAndPwm", "categorical-and-pwm.json"},
        ExampleCase{"keyframeInterpolation", "keyframe-interpolation.json"},
        ExampleCase{"musicEventsAndFeatures", "music-events-and-features.json"},
        ExampleCase{"palettes", "palettes.json"}, ExampleCase{"proceduralCurves", "procedural-curves.json"}),
    [](const testing::TestParamInfo<ExampleCase> &info) { return info.param.name; });

} // namespace
