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

struct NestedCameraCase
{
    std::string name;
    std::filesystem::path fixture;
    std::filesystem::path golden;
    int lanes;
    int up_lane;
};

struct MovingCameraCase
{
    std::string name;
    std::filesystem::path fixture;
    std::filesystem::path golden;
    int keyframes;
};

struct EyePathCase
{
    std::string name;
    std::filesystem::path fixture;
    std::filesystem::path golden;
    std::string path_needle;
    std::string signal_needle;
    int bounded_components;
    int subdivisions;
    int last_subdivision;
};

struct CurvedLookCase
{
    std::string name;
    std::string kind;
    std::filesystem::path fixture;
    std::filesystem::path golden;
};

struct StraightCameraCase
{
    std::string name;
    std::filesystem::path fixture;
    std::filesystem::path golden;
    int lanes;
    double midpoint;
};

struct CameraComparisonCase
{
    std::string name;
    std::filesystem::path fixture;
    std::filesystem::path golden;
    double tolerance;
};

struct DiagnosticFixtureCase
{
    std::string name;
    std::filesystem::path fixture;
    int diagnostics;
    int lanes;
    std::vector<int> source_indices;
    std::vector<std::pair<int, std::string>> retained_lanes;
};

struct DiagnosticMessageCase
{
    std::string name;
    std::filesystem::path fixture;
    int diagnostic;
    std::string message;
};

struct LayerDiagnosticCase
{
    std::string name;
    int diagnostic;
    std::string source;
};

const std::vector<NestedCameraCase> NESTED_CAMERA_CASES{
    {"Linear", "fixtures/camera2d-nested-linear.json", "fixtures/gold-camera2d-nested-linear.par", 11, 8},
    {"Hold", "fixtures/camera2d-nested-hold.json", "fixtures/gold-camera2d-nested-hold.par", 11, 8},
    {"EyeZero", "fixtures/camera2d-nested-eye-zero.json", "fixtures/gold-camera2d-nested-eye-zero.par", 15, 11},
    {"EyeCrossing", "fixtures/camera2d-nested-eye-crossing.json", "fixtures/gold-camera2d-nested-eye-crossing.par", 15,
        11},
};

const std::vector<NestedCameraCase> NESTED_EYE_CASES{
    {"EyeZero", "fixtures/camera2d-nested-eye-zero.json", "fixtures/gold-camera2d-nested-eye-zero.par", 15, 11},
    {"EyeCrossing", "fixtures/camera2d-nested-eye-crossing.json", "fixtures/gold-camera2d-nested-eye-crossing.par", 15,
        11},
};

const std::vector<MovingCameraCase> MOVING_CAMERA_CASES{
    {"KeyedBezier", "fixtures/camera2d-moving-keyed-bezier.json", "fixtures/gold-camera2d-moving-keyed-bezier.par", 6},
    {"CircleKeyed", "fixtures/camera2d-moving-circle-keyed.json", "fixtures/gold-camera2d-moving-circle-keyed.par", 6},
    {"HoldSpiral", "fixtures/camera2d-moving-hold-spiral.json", "fixtures/gold-camera2d-moving-hold-spiral.par", 6},
    {"BezierCatmull", "fixtures/camera2d-moving-bezier-catmull.json",
        "fixtures/gold-camera2d-moving-bezier-catmull.par", 4},
    {"EllipseLissajous", "fixtures/camera2d-moving-ellipse-lissajous.json",
        "fixtures/gold-camera2d-moving-ellipse-lissajous.par", 4},
};

const std::vector<EyePathCase> EYE_PATH_CASES{
    {"LissajousIndependent", "fixtures/camera2d-eye-lissajous-independent.json",
        "fixtures/gold-camera2d-eye-lissajous-independent.par", "lissajous", "lissajous", 2, 6, 24},
    {"LissajousSlowY", "fixtures/camera2d-eye-lissajous-slow-y.json", "fixtures/gold-camera2d-eye-lissajous-slow-y.par",
        "lissajous", "lissajous", 2, 6, 24},
    {"Bezier", "fixtures/camera2d-eye-bezier.json", "fixtures/gold-camera2d-eye-bezier.par", "bezier", "bezier", 2, 6,
        24},
    {"CatmullRom", "fixtures/camera2d-eye-catmull-rom.json", "fixtures/gold-camera2d-eye-catmull-rom.par",
        "catmull-rom", "catmull-rom", 2, 6, 24},
    {"EqualFrequencyCentered", "fixtures/camera2d-eye-lissajous-centered.json",
        "fixtures/gold-camera2d-eye-lissajous-centered.par", "x-frequency", "lissajous", 2, 2, 8},
    {"EqualFrequencyPhase", "fixtures/camera2d-eye-lissajous-phase.json",
        "fixtures/gold-camera2d-eye-lissajous-phase.par", "x-frequency", "lissajous", 2, 2, 8},
    {"EqualFrequencyPartial", "fixtures/camera2d-eye-lissajous-partial.json",
        "fixtures/gold-camera2d-eye-lissajous-partial.par", "x-frequency", "lissajous", 2, 2, 8},
    {"SpiralExpanding", "fixtures/camera2d-eye-spiral-expanding.json",
        "fixtures/gold-camera2d-eye-spiral-expanding.par", "from-radius", "spiral", 2, 2, 8},
    {"SpiralShrinking", "fixtures/camera2d-eye-spiral-shrinking.json",
        "fixtures/gold-camera2d-eye-spiral-shrinking.par", "from-radius", "spiral", 2, 2, 8},
    {"SpiralOffset", "fixtures/camera2d-eye-spiral-offset.json", "fixtures/gold-camera2d-eye-spiral-offset.par",
        "from-radius", "spiral", 2, 2, 8},
    {"SpiralStationary", "fixtures/camera2d-eye-spiral-stationary.json",
        "fixtures/gold-camera2d-eye-spiral-stationary.par", "from-radius", "spiral", 2, 2, 8},
    {"OffsetCircle", "fixtures/camera2d-offset-circle.json", "fixtures/gold-camera2d-offset-circle.par", "", "path", 1,
        2, 8},
    {"OffsetEllipse", "fixtures/camera2d-offset-ellipse.json", "fixtures/gold-camera2d-offset-ellipse.par", "", "path",
        1, 2, 8},
    {"OffsetPartial", "fixtures/camera2d-offset-partial.json", "fixtures/gold-camera2d-offset-partial.par", "", "path",
        1, 2, 8},
};

const std::vector<CurvedLookCase> CURVED_LOOK_CASES{
    {"Circle", "circle", "fixtures/camera2d-look-circle.json", "fixtures/gold-camera2d-look-circle.par"},
    {"Ellipse", "ellipse", "fixtures/camera2d-look-ellipse.json", "fixtures/gold-camera2d-look-ellipse.par"},
    {"Lissajous", "lissajous", "fixtures/camera2d-look-lissajous.json", "fixtures/gold-camera2d-look-lissajous.par"},
    {"Spiral", "spiral", "fixtures/camera2d-look-spiral.json", "fixtures/gold-camera2d-look-spiral.par"},
    {"Bezier", "bezier", "fixtures/camera2d-look-bezier.json", "fixtures/gold-camera2d-look-bezier.par"},
    {"CatmullRom", "catmull-rom", "fixtures/camera2d-look-catmull-rom.json",
        "fixtures/gold-camera2d-look-catmull-rom.par"},
};

const std::vector<StraightCameraCase> STRAIGHT_CAMERA_CASES{
    {"LookAt", "fixtures/camera2d-straight-paths.json", "fixtures/gold-camera2d-straight-paths.par", 11, 0.5},
    {"EyeAndLookAt", "fixtures/camera2d-straight-eye.json", "fixtures/gold-camera2d-straight-eye.par", 15, 0.25},
};

std::vector<CameraComparisonCase> make_camera_comparison_cases()
{
    std::vector<CameraComparisonCase> result;
    for (const NestedCameraCase &value : NESTED_CAMERA_CASES)
    {
        result.push_back({"Nested" + value.name, value.fixture, value.golden, 1e-9});
    }
    for (const MovingCameraCase &value : MOVING_CAMERA_CASES)
    {
        result.push_back({"Moving" + value.name, value.fixture, value.golden, 1e-9});
    }
    for (const EyePathCase &value : EYE_PATH_CASES)
    {
        result.push_back({"Eye" + value.name, value.fixture, value.golden, 1e-9});
    }
    for (const CurvedLookCase &value : CURVED_LOOK_CASES)
    {
        result.push_back({"CurvedLook" + value.name, value.fixture, value.golden, 1e-10});
    }
    for (const StraightCameraCase &value : STRAIGHT_CAMERA_CASES)
    {
        result.push_back({"Straight" + value.name, value.fixture, value.golden, 1e-10});
    }
    result.push_back(
        {"AnalyticEye", "fixtures/camera2d-eye-center-mag.json", "fixtures/gold-camera2d-eye-center-mag.par", 1e-11});
    return result;
}

const std::vector<CameraComparisonCase> CAMERA_COMPARISON_CASES = make_camera_comparison_cases();

const std::vector<DiagnosticFixtureCase> DIAGNOSTIC_FIXTURE_CASES{
    {"Nested", "fixtures/partial-camera2d-nested.json", 10, 11, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9},
        {{0, "animation-10-center-mag[0]"}}},
    {"MovingLook", "fixtures/partial-camera2d-moving.json", 7, 15, {0, 1, 2, 3, 4, 5, 6},
        {{0, "animation-7-center-mag[0]"}}},
    {"EyePaths", "fixtures/partial-camera2d-eye-paths.json", 11, 30, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11},
        {{0, "animation-10-center-mag[0]"}, {15, "animation-12-center-mag[0]"}}},
    {"LissajousEyes", "fixtures/partial-camera2d-eye-lissajous.json", 7, 30, {0, 1, 2, 3, 4, 5, 7},
        {{0, "animation-6-center-mag[0]"}, {15, "animation-8-center-mag[0]"}}},
    {"SpiralEyes", "fixtures/partial-camera2d-eye-spirals.json", 7, 30, {0, 1, 2, 3, 4, 5, 6},
        {{0, "animation-7-center-mag[0]"}, {15, "animation-8-center-mag[0]"}}},
    {"OffsetOrbits", "fixtures/partial-camera2d-offset-orbits.json", 6, 30, {0, 1, 2, 3, 5, 6},
        {{0, "animation-4-center-mag[0]"}, {15, "animation-7-center-mag[0]"}}},
    {"CurvedLookAt", "fixtures/partial-camera2d-look-paths.json", 7, 26, {0, 1, 2, 3, 4, 5, 7},
        {{0, "animation-6-center-mag[0]"}, {15, "animation-8-center-mag[0]"}}},
    {"StraightCompositions", "fixtures/partial-camera2d-straight-paths.json", 7, 11, {0, 1, 2, 3, 4, 5, 6},
        {{0, "animation-7-center-mag[0]"}}},
    {"EyeCompositions", "fixtures/partial-camera2d-eye.json", 8, 13, {0, 1, 2, 3, 4, 5, 6, 7},
        {{0, "animation-8-center-mag[0]"}}},
    {"CameraForms", "fixtures/partial-camera2d.json", 14, 11, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13},
        {{0, "animation-14-center-mag[0]"}}},
};

const std::vector<DiagnosticMessageCase> DIAGNOSTIC_MESSAGE_CASES{
    {"NestedKeyedZero", "fixtures/partial-camera2d-nested.json", 0, "ParAnimator format"},
    {"NestedKeyedOne", "fixtures/partial-camera2d-nested.json", 1, "ParAnimator format"},
    {"NestedKeyedTwo", "fixtures/partial-camera2d-nested.json", 2, "ParAnimator format"},
    {"NestedKeyedThree", "fixtures/partial-camera2d-nested.json", 3, "ParAnimator format"},
    {"NestedKeyedZeroKeyword", "fixtures/partial-camera2d-nested.json", 0, "keyed"},
    {"NestedKeyedOneKeyword", "fixtures/partial-camera2d-nested.json", 1, "keyed"},
    {"NestedKeyedTwoKeyword", "fixtures/partial-camera2d-nested.json", 2, "keyed"},
    {"NestedKeyedThreeKeyword", "fixtures/partial-camera2d-nested.json", 3, "keyed"},
    {"NestedNormalization", "fixtures/partial-camera2d-nested.json", 4, "normalize"},
    {"NestedNumber", "fixtures/partial-camera2d-nested.json", 5, "number"},
    {"NestedSingularity", "fixtures/partial-camera2d-nested.json", 6, "singular"},
    {"NestedCrossing", "fixtures/partial-camera2d-nested.json", 7, "zero vector between keys"},
    {"NestedInitialHeight", "fixtures/partial-camera2d-nested.json", 8, "positive height"},
    {"NestedTargetHeight", "fixtures/partial-camera2d-nested.json", 9, "positive height"},
    {"MovingSingularityZero", "fixtures/partial-camera2d-moving.json", 0, "singular"},
    {"MovingSingularityOne", "fixtures/partial-camera2d-moving.json", 1, "singular"},
    {"MovingSingularityTwo", "fixtures/partial-camera2d-moving.json", 2, "singular"},
    {"MovingSingularityThree", "fixtures/partial-camera2d-moving.json", 3, "singular"},
    {"MovingSingularityFour", "fixtures/partial-camera2d-moving.json", 4, "singular"},
    {"MovingSingularityFive", "fixtures/partial-camera2d-moving.json", 5, "singular"},
    {"MovingSingularitySix", "fixtures/partial-camera2d-moving.json", 6, "singular"},
    {"MovingValidationLimit", "fixtures/partial-camera2d-moving.json", 6, "validation limit"},
    {"EyePathSingularityZero", "fixtures/partial-camera2d-eye-paths.json", 0, "singular"},
    {"EyePathSingularityOne", "fixtures/partial-camera2d-eye-paths.json", 1, "singular"},
    {"EyePathSingularityTwo", "fixtures/partial-camera2d-eye-paths.json", 2, "singular"},
    {"EyePathSingularityThree", "fixtures/partial-camera2d-eye-paths.json", 3, "singular"},
    {"EyePathSingularityFour", "fixtures/partial-camera2d-eye-paths.json", 4, "singular"},
    {"EyePathSingularityFive", "fixtures/partial-camera2d-eye-paths.json", 5, "singular"},
    {"EyePathSingularitySix", "fixtures/partial-camera2d-eye-paths.json", 6, "singular"},
    {"EyePathSingularitySeven", "fixtures/partial-camera2d-eye-paths.json", 7, "singular"},
    {"EyePathValidationLimit", "fixtures/partial-camera2d-eye-paths.json", 10, "validation limit"},
    {"LissajousSingularityZero", "fixtures/partial-camera2d-eye-lissajous.json", 0, "singular"},
    {"LissajousSingularityOne", "fixtures/partial-camera2d-eye-lissajous.json", 1, "singular"},
    {"LissajousSingularityFive", "fixtures/partial-camera2d-eye-lissajous.json", 5, "singular"},
    {"SpiralSingularityZero", "fixtures/partial-camera2d-eye-spirals.json", 0, "singular"},
    {"SpiralSingularityOne", "fixtures/partial-camera2d-eye-spirals.json", 1, "singular"},
    {"SpiralSingularityTwo", "fixtures/partial-camera2d-eye-spirals.json", 2, "singular"},
    {"OffsetSingularityZero", "fixtures/partial-camera2d-offset-orbits.json", 0, "singular"},
    {"OffsetSingularityOne", "fixtures/partial-camera2d-offset-orbits.json", 1, "singular"},
    {"OffsetSingularityTwo", "fixtures/partial-camera2d-offset-orbits.json", 2, "singular"},
    {"OffsetSingularityThree", "fixtures/partial-camera2d-offset-orbits.json", 3, "singular"},
};

const std::vector<LayerDiagnosticCase> LAYER_DIAGNOSTIC_CASES{
    {"LayerTwo", 0, "animation-layer-2-0:"},
    {"LayerThree", 1, "animation-layer-3-0:"},
    {"LayerFour", 2, "animation-layer-4-0:"},
};

void PrintTo(const NestedCameraCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const MovingCameraCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const EyePathCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const CurvedLookCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const StraightCameraCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const CameraComparisonCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const DiagnosticFixtureCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const DiagnosticMessageCase &value, std::ostream *stream)
{
    *stream << value.name;
}

void PrintTo(const LayerDiagnosticCase &value, std::ostream *stream)
{
    *stream << value.name;
}

template <typename Case>
std::string case_name(const testing::TestParamInfo<Case> &info)
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
        throw std::runtime_error("camera fixture import failed");
    }
    return std::move(*result.document);
}

std::vector<std::array<double, 6>> read_camera_frames(const std::filesystem::path &golden)
{
    std::ifstream file(golden);
    if (!file)
    {
        throw std::runtime_error("unable to open camera golden file");
    }
    std::vector<std::array<double, 6>> result;
    for (std::string line; std::getline(file, line);)
    {
        const std::size_t start = line.find("center-mag=");
        if (start == std::string::npos)
        {
            continue;
        }
        std::string value = line.substr(start + 11);
        std::replace(value.begin(), value.end(), '/', ' ');
        std::istringstream values(value);
        std::array<double, 6> frame{0, 0, 1, 1, 0, 0};
        for (double &component : frame)
        {
            if (!(values >> component))
            {
                break;
            }
        }
        result.push_back(frame);
    }
    return result;
}

const timeline::Curve &curve(const timeline::Document &document, int lane)
{
    return std::get<timeline::Curve>(document.lanes()[lane].items().front());
}

const timeline::Keyframe &keyframe(const timeline::Document &document, int lane)
{
    return std::get<timeline::Keyframe>(document.lanes()[lane].items().front());
}

double sample_lane(const timeline::Lane &lane, timeline::Time time)
{
    if (std::holds_alternative<timeline::Curve>(lane.items().front()))
    {
        return std::get<timeline::Curve>(lane.items().front()).sample(time);
    }
    return *lane.evaluate_keyframes(time);
}

timeline::Time subdivided_frame(const timeline::FrameGrid &grid, int subdivision, int divisions)
{
    return grid.offset() + timeline::Duration::from_ticks(subdivision * grid.frame_duration().ticks() / divisions);
}

timeline::Document combine_with_music(const timeline::Document &document)
{
    JsonImportResult music = import_fixture("fixtures/beat-keys/rms.beat-keys.json");
    if (!music.succeeded() || !music.diagnostics.empty())
    {
        throw std::runtime_error("music fixture import failed");
    }
    return timeline::combine_documents(document, *music.document);
}

std::vector<std::array<double, 6>> inspect_camera_frames(const timeline::Document &document, int frame_count)
{
    std::vector<std::array<double, 6>> result;
    for (int frame = 0; frame < frame_count; ++frame)
    {
        const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(document, frame);
        if (!inspection)
        {
            throw std::runtime_error("camera frame inspection failed");
        }
        std::array<double, 6> values{};
        for (int component = 0; component < 6; ++component)
        {
            if (inspection->lanes[component].items.empty() || !inspection->lanes[component].items.front().value)
            {
                throw std::runtime_error("camera frame component inspection failed");
            }
            values[component] = *inspection->lanes[component].items.front().value;
        }
        result.push_back(values);
    }
    return result;
}

/// Parameterized camera fixture that owns a clean imported document.
///
template <typename Case>
class ParameterizedCameraImportTest : public testing::TestWithParam<Case>
{
protected:
    ParameterizedCameraImportTest() :
        m_document(import_clean_document(this->GetParam().fixture))
    {
    }
    const Case &definition() const
    {
        return this->GetParam();
    }
    const timeline::Document &document() const
    {
        return m_document;
    }
    const timeline::FrameGrid &frame_grid() const
    {
        return *m_document.frame_grid();
    }

private:
    timeline::Document m_document;
};

/// Parameterized camera fixture that owns a raw import result.
///
template <typename Case>
class ParameterizedCameraResultTest : public testing::TestWithParam<Case>
{
protected:
    ParameterizedCameraResultTest() :
        m_result(import_fixture(this->GetParam().fixture))
    {
    }
    const Case &definition() const
    {
        return this->GetParam();
    }
    const JsonImportResult &result() const
    {
        return m_result;
    }

private:
    JsonImportResult m_result;
};

/// Parameterized camera fixture with a composed music document.
///
template <typename Case>
class ParameterizedCameraCompositionTest : public ParameterizedCameraImportTest<Case>
{
protected:
    ParameterizedCameraCompositionTest() :
        m_combined_document(combine_with_music(this->document()))
    {
    }
    const timeline::Document &combined_document() const
    {
        return m_combined_document;
    }

private:
    timeline::Document m_combined_document;
};

/// Parameterized camera fixture with the standard composed layout.
///
template <typename Case>
class ParameterizedCameraLayoutTest : public ParameterizedCameraCompositionTest<Case>
{
protected:
    ParameterizedCameraLayoutTest() :
        m_layout(this->combined_document(),
            timeline::Viewport(500, 600, this->frame_grid().offset(), this->frame_grid().end_time()),
            timeline::LayoutMetrics(100, 20, 30, 4))
    {
    }
    const timeline::Layout &layout() const
    {
        return m_layout;
    }

private:
    timeline::Layout m_layout;
};

/// Fixed camera fixture that owns a clean imported document.
///
class FixedCameraImportTest : public testing::Test
{
protected:
    explicit FixedCameraImportTest(const std::filesystem::path &fixture) :
        m_document(import_clean_document(fixture))
    {
    }
    const timeline::Document &document() const
    {
        return m_document;
    }
    const timeline::FrameGrid &frame_grid() const
    {
        return *m_document.frame_grid();
    }

private:
    timeline::Document m_document;
};

/// Fixed camera fixture with a music-first composed document.
///
class FixedCameraCompositionTest : public FixedCameraImportTest
{
protected:
    explicit FixedCameraCompositionTest(const std::filesystem::path &fixture) :
        FixedCameraImportTest(fixture),
        m_combined_document(
            timeline::combine_documents(import_clean_document("fixtures/beat-keys/rms.beat-keys.json"), document()))
    {
    }
    const timeline::Document &combined_document() const
    {
        return m_combined_document;
    }

private:
    timeline::Document m_combined_document;
};

/// Imported analytic-eye camera document.
///
class AnalyticEyeTest : public FixedCameraImportTest
{
protected:
    AnalyticEyeTest() :
        FixedCameraImportTest("fixtures/camera2d-eye-center-mag.json")
    {
    }
};

/// Analytic-eye camera document with its standard layout.
///
class AnalyticEyeLayoutTest : public AnalyticEyeTest
{
protected:
    AnalyticEyeLayoutTest() :
        m_layout(document(), timeline::Viewport(500, 460, frame_grid().offset(), frame_grid().end_time()),
            timeline::LayoutMetrics(100, 20, 30, 4))
    {
    }
    const timeline::Layout &layout() const
    {
        return m_layout;
    }

private:
    timeline::Layout m_layout;
};

/// Imported eye-variant camera document.
///
class EyeVariantsTest : public FixedCameraImportTest
{
protected:
    EyeVariantsTest() :
        FixedCameraImportTest("fixtures/camera2d-eye-variants.json")
    {
    }
};

/// Eye-variant camera document composed after music data.
///
class EyeVariantsCompositionTest : public FixedCameraCompositionTest
{
protected:
    EyeVariantsCompositionTest() :
        FixedCameraCompositionTest("fixtures/camera2d-eye-variants.json")
    {
    }
};

/// Imported keyed-variant camera document.
///
class KeyedVariantsTest : public FixedCameraImportTest
{
protected:
    KeyedVariantsTest() :
        FixedCameraImportTest("fixtures/camera2d-keyed-variants.json")
    {
    }
};

/// Keyed-variant camera document composed after music data.
///
class KeyedVariantsCompositionTest : public FixedCameraCompositionTest
{
protected:
    KeyedVariantsCompositionTest() :
        FixedCameraCompositionTest("fixtures/camera2d-keyed-variants.json")
    {
    }
};

/// Imported center-magnification camera document.
///
class CenterMagnificationTest : public FixedCameraImportTest
{
protected:
    CenterMagnificationTest() :
        FixedCameraImportTest("fixtures/camera2d-center-mag.json")
    {
    }
};

/// Center-magnification camera document with its standard layout.
///
class CenterMagnificationLayoutTest : public CenterMagnificationTest
{
protected:
    CenterMagnificationLayoutTest() :
        m_layout(document(), timeline::Viewport(500, 380, frame_grid().offset(), frame_grid().end_time()),
            timeline::LayoutMetrics(100, 20, 30, 4))
    {
    }
    const timeline::Layout &layout() const
    {
        return m_layout;
    }

private:
    timeline::Layout m_layout;
};

/// Imported layer-source camera result with retained diagnostics.
///
class LayerSourceCameraTest : public testing::Test
{
protected:
    LayerSourceCameraTest();
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

private:
    JsonImportResult m_result;
};

LayerSourceCameraTest::LayerSourceCameraTest() :
    m_result(import_fixture("fixtures/camera2d-layer-sources.json"))
{
}

void LayerSourceCameraTest::SetUp()
{
    ASSERT_TRUE(m_result.succeeded());
    ASSERT_TRUE(m_result.document);
}

/// Imported nested-camera cases.
///
class NestedCameraImportTest : public ParameterizedCameraImportTest<NestedCameraCase>
{
};

/// Composed nested-camera cases.
///
class NestedCameraCompositionTest : public ParameterizedCameraCompositionTest<NestedCameraCase>
{
};

/// Laid-out nested-camera cases.
///
class NestedCameraLayoutTest : public ParameterizedCameraLayoutTest<NestedCameraCase>
{
};

/// Imported nested-eye cases.
///
class NestedEyeImportTest : public ParameterizedCameraImportTest<NestedCameraCase>
{
};

/// Imported moving-camera cases.
///
class MovingCameraImportTest : public ParameterizedCameraImportTest<MovingCameraCase>
{
};

/// Composed moving-camera cases.
///
class MovingCameraCompositionTest : public ParameterizedCameraCompositionTest<MovingCameraCase>
{
};

/// Laid-out moving-camera cases.
///
class MovingCameraLayoutTest : public ParameterizedCameraLayoutTest<MovingCameraCase>
{
};

/// Imported eye-path cases.
///
class EyePathImportTest : public ParameterizedCameraImportTest<EyePathCase>
{
};

/// Composed eye-path cases.
///
class EyePathCompositionTest : public ParameterizedCameraCompositionTest<EyePathCase>
{
};

/// Laid-out eye-path cases.
///
class EyePathLayoutTest : public ParameterizedCameraLayoutTest<EyePathCase>
{
};

/// Imported curved-look cases.
///
class CurvedLookImportTest : public ParameterizedCameraImportTest<CurvedLookCase>
{
};

/// Composed curved-look cases.
///
class CurvedLookCompositionTest : public ParameterizedCameraCompositionTest<CurvedLookCase>
{
};

/// Laid-out curved-look cases.
///
class CurvedLookLayoutTest : public ParameterizedCameraLayoutTest<CurvedLookCase>
{
};

/// Imported straight-camera cases.
///
class StraightCameraImportTest : public ParameterizedCameraImportTest<StraightCameraCase>
{
};

/// Composed straight-camera cases.
///
class StraightCameraCompositionTest : public ParameterizedCameraCompositionTest<StraightCameraCase>
{
};

/// Laid-out straight-camera cases.
///
class StraightCameraLayoutTest : public ParameterizedCameraLayoutTest<StraightCameraCase>
{
};

/// Imported camera comparison cases.
///
class CameraComparisonTest : public ParameterizedCameraImportTest<CameraComparisonCase>
{
};

/// Camera diagnostic fixture imports selected by parameter.
///
class CameraDiagnosticFixtureTest : public ParameterizedCameraResultTest<DiagnosticFixtureCase>
{
};

/// Camera diagnostic message imports selected by parameter.
///
class CameraDiagnosticMessageTest : public ParameterizedCameraResultTest<DiagnosticMessageCase>
{
};

/// Layer-source diagnostic cases for one fixed document.
///
class LayerCameraDiagnosticTest : public LayerSourceCameraTest, public testing::WithParamInterface<LayerDiagnosticCase>
{
};

} // namespace

TEST_P(CameraComparisonTest, matchesParAnimatorOutput)
{
    const CameraComparisonCase &test_case = definition();
    const std::vector<std::array<double, 6>> expected = read_camera_frames(test_case.golden);

    const std::vector<std::array<double, 6>> actual = inspect_camera_frames(document(), timeline::size_cast(expected));

    ASSERT_EQ(5, timeline::size_cast(expected));
    ASSERT_EQ(timeline::size_cast(expected), timeline::size_cast(actual));
    for (int frame = 0; frame < timeline::size_cast(expected); ++frame)
    {
        for (int component = 0; component < 6; ++component)
        {
            const double difference = component == 4
                ? std::remainder(actual[frame][component] - expected[frame][component], 360)
                : actual[frame][component] - expected[frame][component];
            EXPECT_NEAR(0, difference, test_case.tolerance);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(CameraComparisons, CameraComparisonTest, testing::ValuesIn(CAMERA_COMPARISON_CASES),
    case_name<CameraComparisonCase>);

TEST_P(NestedCameraImportTest, importsExpectedDocumentShape)
{
    EXPECT_EQ(definition().lanes, document().lane_count());
}

TEST_P(NestedCameraImportTest, preservesAnalyticMagnification)
{
    const timeline::Curve &magnification = curve(document(), 2);

    EXPECT_TRUE(magnification.samples().empty());
}

TEST_P(NestedCameraImportTest, preservesHeightSourceRecipe)
{
    const ResolvedAttributes attributes = resolved_attributes(document(), keyframe(document(), 10).attributes());

    EXPECT_FALSE(attributes.at("signal").empty());
}

TEST_P(NestedCameraImportTest, keepsMagnificationWithinPathBounds)
{
    const timeline::Curve &magnification = curve(document(), 2);

    for (int subdivision = 0; subdivision <= 24; ++subdivision)
    {
        const double value = magnification.sample(subdivided_frame(frame_grid(), subdivision, 6));
        EXPECT_LE(*magnification.minimum(), value);
        EXPECT_GE(*magnification.maximum(), value);
    }
}

TEST_P(NestedCameraImportTest, normalizesViewUp)
{
    for (int subdivision = 0; subdivision <= 24; ++subdivision)
    {
        const timeline::Time time = subdivided_frame(frame_grid(), subdivision, 6);
        const double x = curve(document(), definition().up_lane).sample(time);
        const double y = curve(document(), definition().up_lane + 1).sample(time);
        EXPECT_NEAR(1, std::hypot(x, y), 1e-12);
    }
}

TEST_P(NestedCameraCompositionTest, composesWithMusicDocument)
{
    EXPECT_EQ(document().lane_count() + 4, combined_document().lane_count());
}

TEST_P(NestedCameraLayoutTest, rendersCameraLanesAfterComposition)
{
    const std::string snapshot = timeline::render_snapshot(layout().display_list());

    EXPECT_NE(std::string::npos, snapshot.find("view-up"));
    EXPECT_NE(std::string::npos, snapshot.find("height"));
}

TEST_P(NestedEyeImportTest, preservesAuthoredViewUpOutsideCameraComposition)
{
    const timeline::Lane &authored = document().lanes()[13];

    const ResolvedAttributes attributes = resolved_attributes(document(), keyframe(document(), 13).attributes());

    EXPECT_EQ("keyframes", document().strings().lookup(authored.kind()));
    EXPECT_EQ("false", attributes.at("used-by-camera"));
    EXPECT_NE(std::string::npos, attributes.at("signal").find("keys"));
}

TEST_P(NestedEyeImportTest, evaluatesAuthoredViewUpAcrossCrossing)
{
    const timeline::Time crossing = frame_grid().offset() +
        timeline::Duration::from_ticks(3 * (frame_grid().frame_start(4) - frame_grid().offset()).ticks() / 10);

    EXPECT_NEAR(0, *document().lanes()[13].evaluate_keyframes(crossing), 1e-12);
    EXPECT_NEAR(0, *document().lanes()[14].evaluate_keyframes(crossing), 1e-12);
}

INSTANTIATE_TEST_SUITE_P(
    NestedCameras, NestedCameraImportTest, testing::ValuesIn(NESTED_CAMERA_CASES), case_name<NestedCameraCase>);
INSTANTIATE_TEST_SUITE_P(
    NestedCameras, NestedCameraCompositionTest, testing::ValuesIn(NESTED_CAMERA_CASES), case_name<NestedCameraCase>);
INSTANTIATE_TEST_SUITE_P(
    NestedCameras, NestedCameraLayoutTest, testing::ValuesIn(NESTED_CAMERA_CASES), case_name<NestedCameraCase>);
INSTANTIATE_TEST_SUITE_P(
    NestedEyes, NestedEyeImportTest, testing::ValuesIn(NESTED_EYE_CASES), case_name<NestedCameraCase>);

TEST(CameraImport, evaluatesLinearNestedRotationGeometrically)
{
    const timeline::Document document = import_clean_document("fixtures/camera2d-nested-linear.json");
    const timeline::FrameGrid &grid = *document.frame_grid();

    const double rotation = curve(document, 4).sample(grid.frame_start(1));

    EXPECT_NEAR(33.6900675259798, rotation, 1e-12);
}

TEST(CameraImport, preservesGeometricNestedHeightInterpolation)
{
    const timeline::Document document = import_clean_document("fixtures/camera2d-nested-linear.json");

    const timeline::KeyframeInterpolation interpolation = keyframe(document, 10).interpolation();

    EXPECT_EQ(timeline::KeyframeInterpolation::GEOMETRIC, interpolation);
}

TEST(CameraImport, preservesHeldNestedHeightInterpolation)
{
    const timeline::Document document = import_clean_document("fixtures/camera2d-nested-hold.json");

    const timeline::KeyframeInterpolation interpolation = keyframe(document, 10).interpolation();

    EXPECT_EQ(timeline::KeyframeInterpolation::HOLD, interpolation);
}

TEST(CameraImport, evaluatesHeldNestedMagnification)
{
    const timeline::Document document = import_clean_document("fixtures/camera2d-nested-hold.json");
    const timeline::FrameGrid &grid = *document.frame_grid();
    const timeline::Curve &magnification = curve(document, 2);

    const double before = magnification.sample(grid.frame_start(3));
    const double after = magnification.sample(grid.frame_start(4));

    EXPECT_DOUBLE_EQ(1, before);
    EXPECT_DOUBLE_EQ(0.5, after);
}

TEST_P(MovingCameraImportTest, importsExpectedDocumentShape)
{
    EXPECT_EQ(15, document().lane_count());
    EXPECT_EQ(definition().keyframes, document().keyframe_count());
}

TEST_P(MovingCameraImportTest, preservesSourceRecipes)
{
    for (int lane = 6; lane < 10; ++lane)
    {
        const timeline::Item &item = document().lanes()[lane].items().front();
        if (std::holds_alternative<timeline::Curve>(item))
        {
            const timeline::Curve &item_curve = std::get<timeline::Curve>(item);
            const ResolvedAttributes attributes = resolved_attributes(document(), item_curve.attributes());
            EXPECT_NE(std::string::npos, attributes.at("signal").find("path"));
            EXPECT_FALSE(attributes.at("path").empty());
        }
        else
        {
            const ResolvedAttributes attributes =
                resolved_attributes(document(), std::get<timeline::Keyframe>(item).attributes());
            EXPECT_FALSE(attributes.at("signal").empty());
        }
    }
}

TEST_P(MovingCameraImportTest, retainsAnalyticSourceDefinitions)
{
    int analytic_sources = 0;

    for (int lane = 6; lane < 10; ++lane)
    {
        const timeline::Item &item = document().lanes()[lane].items().front();
        if (std::holds_alternative<timeline::Curve>(item))
        {
            ++analytic_sources;
            EXPECT_TRUE(std::get<timeline::Curve>(item).samples().empty());
        }
    }
    EXPECT_GT(analytic_sources, 0);
}

TEST_P(MovingCameraImportTest, derivesViewUpFromMovingEyeAndLookAt)
{
    const timeline::Curve &up_x = curve(document(), 11);
    const timeline::Curve &up_y = curve(document(), 12);

    for (int subdivision = 0; subdivision <= 24; ++subdivision)
    {
        const timeline::Time time = subdivided_frame(frame_grid(), subdivision, 6);
        const double dx = sample_lane(document().lanes()[8], time) - sample_lane(document().lanes()[6], time);
        const double dy = sample_lane(document().lanes()[9], time) - sample_lane(document().lanes()[7], time);
        EXPECT_NEAR(dx / std::hypot(dx, dy), up_x.sample(time), 1e-12);
        EXPECT_NEAR(dy / std::hypot(dx, dy), up_y.sample(time), 1e-12);
    }
    EXPECT_EQ("eye-look-at", resolved_attributes(document(), up_x.attributes()).at("derived-from"));
    EXPECT_TRUE(up_x.samples().empty());
}

TEST_P(MovingCameraImportTest, keepsAnalyticSourcesWithinPathBounds)
{
    int analytic_sources = 0;

    for (int lane = 6; lane < 10; ++lane)
    {
        const timeline::Item &item = document().lanes()[lane].items().front();
        if (!std::holds_alternative<timeline::Curve>(item))
        {
            continue;
        }
        ++analytic_sources;
        const timeline::Curve &item_curve = std::get<timeline::Curve>(item);
        for (int subdivision = 0; subdivision <= 24; ++subdivision)
        {
            const double value = item_curve.sample(subdivided_frame(frame_grid(), subdivision, 6));
            EXPECT_LE(*item_curve.minimum(), value);
            EXPECT_GE(*item_curve.maximum(), value);
        }
    }
    EXPECT_GT(analytic_sources, 0);
}

TEST_P(MovingCameraCompositionTest, composesWithMusicDocument)
{
    EXPECT_EQ(19, combined_document().lane_count());
}

TEST_P(MovingCameraLayoutTest, rendersEyeAfterComposition)
{
    const std::string snapshot = timeline::render_snapshot(layout().display_list());

    EXPECT_NE(std::string::npos, snapshot.find("animation-0-eye[0]"));
}

INSTANTIATE_TEST_SUITE_P(
    MovingCameras, MovingCameraImportTest, testing::ValuesIn(MOVING_CAMERA_CASES), case_name<MovingCameraCase>);
INSTANTIATE_TEST_SUITE_P(
    MovingCameras, MovingCameraCompositionTest, testing::ValuesIn(MOVING_CAMERA_CASES), case_name<MovingCameraCase>);
INSTANTIATE_TEST_SUITE_P(
    MovingCameras, MovingCameraLayoutTest, testing::ValuesIn(MOVING_CAMERA_CASES), case_name<MovingCameraCase>);

TEST(CameraImport, evaluatesHeldMovingLookAt)
{
    const timeline::Document document = import_clean_document("fixtures/camera2d-moving-hold-spiral.json");
    const timeline::FrameGrid &grid = *document.frame_grid();

    const double before = sample_lane(document.lanes()[6], grid.frame_start(3));
    const double after = sample_lane(document.lanes()[6], grid.frame_start(4));

    EXPECT_DOUBLE_EQ(1, before);
    EXPECT_DOUBLE_EQ(-3, after);
}

TEST_P(EyePathImportTest, importsExpectedDocumentShape)
{
    EXPECT_EQ(15, document().lane_count());
    EXPECT_EQ(6, document().keyframe_count());
}

TEST_P(EyePathImportTest, preservesOwnedAnalyticEyeRecipe)
{
    const timeline::Curve &eye = curve(document(), 8);

    const ResolvedAttributes attributes = resolved_attributes(document(), eye.attributes());

    if (!definition().path_needle.empty())
    {
        EXPECT_NE(std::string::npos, attributes.at("path").find(definition().path_needle));
    }
    EXPECT_NE(std::string::npos, attributes.at("signal").find(definition().signal_needle));
}

TEST_P(EyePathImportTest, retainsAnalyticEyeDefinition)
{
    const timeline::Curve &eye = curve(document(), 8);

    EXPECT_TRUE(eye.samples().empty());
}

TEST_P(EyePathImportTest, retainsAnalyticViewUpDefinition)
{
    const timeline::Curve &up = curve(document(), 11);

    EXPECT_TRUE(up.samples().empty());
}

TEST_P(EyePathImportTest, recordsDerivedViewUpProvenance)
{
    const timeline::Curve &up = curve(document(), 11);

    EXPECT_EQ("eye-look-at", resolved_attributes(document(), up.attributes()).at("derived-from"));
}

TEST_P(EyePathImportTest, derivesViewUpFromEyeAndLookAt)
{
    const timeline::Curve &eye_x = curve(document(), 8);
    const timeline::Curve &eye_y = curve(document(), 9);
    const timeline::Curve &up_x = curve(document(), 11);
    const timeline::Curve &up_y = curve(document(), 12);

    for (int subdivision = 0; subdivision <= definition().last_subdivision; ++subdivision)
    {
        const timeline::Time time = subdivided_frame(frame_grid(), subdivision, definition().subdivisions);
        const double dx = eye_x.sample(time) - *document().lanes()[6].evaluate_keyframes(time);
        const double dy = eye_y.sample(time) - *document().lanes()[7].evaluate_keyframes(time);
        EXPECT_NEAR(dx / std::hypot(dx, dy), up_x.sample(time), 1e-12);
        EXPECT_NEAR(dy / std::hypot(dx, dy), up_y.sample(time), 1e-12);
    }
}

TEST_P(EyePathImportTest, keepsEyeWithinPathBounds)
{
    for (int lane = 8; lane < 8 + definition().bounded_components; ++lane)
    {
        const timeline::Curve &eye = curve(document(), lane);
        for (int subdivision = 0; subdivision <= definition().last_subdivision; ++subdivision)
        {
            const double value = eye.sample(subdivided_frame(frame_grid(), subdivision, definition().subdivisions));
            EXPECT_LE(*eye.minimum(), value);
            EXPECT_GE(*eye.maximum(), value);
        }
    }
}

TEST_P(EyePathCompositionTest, composesWithMusicDocument)
{
    EXPECT_EQ(19, combined_document().lane_count());
}

TEST_P(EyePathLayoutTest, rendersEyePathAfterComposition)
{
    const std::string snapshot = timeline::render_snapshot(layout().display_list());

    EXPECT_NE(std::string::npos, snapshot.find("animation-0-eye[0]-path"));
}

INSTANTIATE_TEST_SUITE_P(EyePaths, EyePathImportTest, testing::ValuesIn(EYE_PATH_CASES), case_name<EyePathCase>);
INSTANTIATE_TEST_SUITE_P(EyePaths, EyePathCompositionTest, testing::ValuesIn(EYE_PATH_CASES), case_name<EyePathCase>);
INSTANTIATE_TEST_SUITE_P(EyePaths, EyePathLayoutTest, testing::ValuesIn(EYE_PATH_CASES), case_name<EyePathCase>);

TEST(CameraImport, evaluatesBezierEyePath)
{
    const timeline::Document document = import_clean_document("fixtures/camera2d-eye-bezier.json");
    const timeline::FrameGrid &grid = *document.frame_grid();

    const double x = curve(document, 8).sample(grid.frame_start(2));
    const double y = curve(document, 9).sample(grid.frame_start(2));

    EXPECT_DOUBLE_EQ(1, x);
    EXPECT_DOUBLE_EQ(0.25, y);
}

TEST(CameraImport, evaluatesCatmullRomEyePath)
{
    const timeline::Document document = import_clean_document("fixtures/camera2d-eye-catmull-rom.json");
    const timeline::FrameGrid &grid = *document.frame_grid();

    const double x = curve(document, 8).sample(grid.frame_start(2));
    const double y = curve(document, 9).sample(grid.frame_start(2));

    EXPECT_DOUBLE_EQ(0, x);
    EXPECT_DOUBLE_EQ(2.125, y);
}

TEST(CameraImport, preservesLissajousEyePhaseRecipe)
{
    const timeline::Document document = import_clean_document("fixtures/camera2d-eye-lissajous-phase.json");

    const ResolvedAttributes attributes = resolved_attributes(document, curve(document, 8).attributes());

    EXPECT_NE(std::string::npos, attributes.at("path").find("phase"));
}

TEST(CameraImport, evaluatesLissajousEyePhase)
{
    const timeline::Document document = import_clean_document("fixtures/camera2d-eye-lissajous-phase.json");
    const timeline::FrameGrid &grid = *document.frame_grid();

    const double x = curve(document, 8).sample(grid.frame_start(1));
    const double y = curve(document, 9).sample(grid.frame_start(1));

    EXPECT_NEAR(1 - std::sqrt(2.0), x, 1e-12);
    EXPECT_NEAR(-1, y, 1e-12);
}

TEST_P(CurvedLookImportTest, importsExpectedDocumentShape)
{
    EXPECT_EQ(11, document().lane_count());
    EXPECT_EQ(4, document().keyframe_count());
}

TEST_P(CurvedLookImportTest, preservesOwnedLookAtRecipe)
{
    for (int component = 0; component < 2; ++component)
    {
        const timeline::Curve &look = curve(document(), component + 6);
        const ResolvedAttributes attributes = resolved_attributes(document(), look.attributes());
        EXPECT_NE(std::string::npos, attributes.at("path").find(definition().kind));
        EXPECT_NE(std::string::npos, attributes.at("signal").find(definition().kind));
    }
}

TEST_P(CurvedLookImportTest, retainsAnalyticLookAtDefinition)
{
    for (int component = 0; component < 2; ++component)
    {
        EXPECT_TRUE(curve(document(), component + 6).samples().empty());
    }
}

TEST_P(CurvedLookImportTest, propagatesLookAtBoundsToCameraCenter)
{
    for (int component = 0; component < 2; ++component)
    {
        const timeline::Curve &center = curve(document(), component);
        const timeline::Curve &look = curve(document(), component + 6);
        EXPECT_TRUE(center.samples().empty());
        EXPECT_EQ(look.minimum(), center.minimum());
        EXPECT_EQ(look.maximum(), center.maximum());
    }
}

TEST_P(CurvedLookImportTest, evaluatesCameraCenterFromLookAtPath)
{
    const timeline::Time between = subdivided_frame(frame_grid(), 1, 2);

    for (int component = 0; component < 2; ++component)
    {
        EXPECT_DOUBLE_EQ(
            curve(document(), component + 6).sample(between), curve(document(), component).sample(between));
    }
}

TEST_P(CurvedLookImportTest, keepsCameraCenterWithinPathBounds)
{
    for (int component = 0; component < 2; ++component)
    {
        const timeline::Curve &center = curve(document(), component);
        const double value = center.sample(frame_grid().frame_start(2));
        EXPECT_LE(*center.minimum(), value);
        EXPECT_GE(*center.maximum(), value);
    }
}

TEST_P(CurvedLookCompositionTest, composesWithMusicDocument)
{
    EXPECT_EQ(15, combined_document().lane_count());
}

TEST_P(CurvedLookLayoutTest, rendersLookAtPathAfterComposition)
{
    const std::string snapshot = timeline::render_snapshot(layout().display_list());

    EXPECT_NE(std::string::npos, snapshot.find("animation-0-look-at[0]-path"));
}

INSTANTIATE_TEST_SUITE_P(
    CurvedLookPaths, CurvedLookImportTest, testing::ValuesIn(CURVED_LOOK_CASES), case_name<CurvedLookCase>);
INSTANTIATE_TEST_SUITE_P(
    CurvedLookPaths, CurvedLookCompositionTest, testing::ValuesIn(CURVED_LOOK_CASES), case_name<CurvedLookCase>);
INSTANTIATE_TEST_SUITE_P(
    CurvedLookPaths, CurvedLookLayoutTest, testing::ValuesIn(CURVED_LOOK_CASES), case_name<CurvedLookCase>);

TEST_P(StraightCameraImportTest, importsExpectedDocumentShape)
{
    EXPECT_EQ(definition().lanes, document().lane_count());
}

TEST_P(StraightCameraImportTest, preservesOwnedLookAtRecipe)
{
    const timeline::Keyframe &look = keyframe(document(), 6);

    const ResolvedAttributes attributes = resolved_attributes(document(), look.attributes());

    EXPECT_NE(std::string::npos, attributes.at("path").find("line"));
    EXPECT_NE(std::string::npos, attributes.at("signal").find("path"));
}

TEST_P(StraightCameraImportTest, preservesLookAtKeyIdentity)
{
    const timeline::Keyframe &look = keyframe(document(), 6);

    EXPECT_EQ("animation-0-look-at-key-0", document().strings().lookup(look.id()));
}

TEST_P(StraightCameraImportTest, evaluatesLookAtPathBetweenFrames)
{
    const double value = *document().lanes()[6].evaluate_keyframes(subdivided_frame(frame_grid(), 1, 2));

    EXPECT_DOUBLE_EQ(definition().midpoint, value);
}

TEST_P(StraightCameraCompositionTest, composesWithMusicDocument)
{
    EXPECT_EQ(document().lane_count() + 4, combined_document().lane_count());
}

TEST_P(StraightCameraLayoutTest, rendersLookAtAfterComposition)
{
    const std::string snapshot = timeline::render_snapshot(layout().display_list());

    EXPECT_NE(std::string::npos, snapshot.find("animation-0-look-at"));
}

INSTANTIATE_TEST_SUITE_P(
    StraightCameras, StraightCameraImportTest, testing::ValuesIn(STRAIGHT_CAMERA_CASES), case_name<StraightCameraCase>);
INSTANTIATE_TEST_SUITE_P(StraightCameras, StraightCameraCompositionTest, testing::ValuesIn(STRAIGHT_CAMERA_CASES),
    case_name<StraightCameraCase>);
INSTANTIATE_TEST_SUITE_P(
    StraightCameras, StraightCameraLayoutTest, testing::ValuesIn(STRAIGHT_CAMERA_CASES), case_name<StraightCameraCase>);

TEST(CameraImport, preservesConstantEyeRecipeForStraightComposition)
{
    const timeline::Document document = import_clean_document("fixtures/camera2d-straight-eye.json");

    const ResolvedAttributes attributes = resolved_attributes(document, keyframe(document, 8).attributes());

    EXPECT_NE(std::string::npos, attributes.at("path").find("constant"));
}

TEST(CameraImport, evaluatesConstantEyeForStraightComposition)
{
    const timeline::Document document = import_clean_document("fixtures/camera2d-straight-eye.json");
    const timeline::FrameGrid &grid = *document.frame_grid();

    const double value = *document.lanes()[8].evaluate_keyframes(subdivided_frame(grid, 1, 2));

    EXPECT_DOUBLE_EQ(0, value);
}

TEST_P(CameraDiagnosticFixtureTest, importsRemainingValidCameras)
{
    ASSERT_TRUE(result().succeeded());
    ASSERT_TRUE(result().document);
    EXPECT_EQ(definition().diagnostics, timeline::size_cast(result().diagnostics));
    EXPECT_EQ(definition().lanes, result().document->lane_count());
}

TEST_P(CameraDiagnosticFixtureTest, identifiesEachFailedAnimationSource)
{
    ASSERT_TRUE(result().succeeded());
    ASSERT_EQ(timeline::size_cast(definition().source_indices), timeline::size_cast(result().diagnostics));
    for (int diagnostic = 0; diagnostic < timeline::size_cast(definition().source_indices); ++diagnostic)
    {
        const std::string prefix = "animation-" + std::to_string(definition().source_indices[diagnostic]) + ":";
        EXPECT_NE(std::string::npos, result().diagnostics[diagnostic].find(prefix));
    }
}

TEST_P(CameraDiagnosticFixtureTest, omitsPartialLanesFromFailedCameras)
{
    ASSERT_TRUE(result().succeeded());
    ASSERT_TRUE(result().document);
    for (const std::pair<int, std::string> &retained : definition().retained_lanes)
    {
        EXPECT_EQ(
            retained.second, result().document->strings().lookup(result().document->lanes()[retained.first].id()));
    }
}

INSTANTIATE_TEST_SUITE_P(CameraDiagnostics, CameraDiagnosticFixtureTest, testing::ValuesIn(DIAGNOSTIC_FIXTURE_CASES),
    case_name<DiagnosticFixtureCase>);

TEST_P(CameraDiagnosticMessageTest, reportsSpecificFailureClass)
{
    ASSERT_TRUE(result().succeeded());
    ASSERT_GT(timeline::size_cast(result().diagnostics), definition().diagnostic);
    EXPECT_NE(std::string::npos, result().diagnostics[definition().diagnostic].find(definition().message));
}

INSTANTIATE_TEST_SUITE_P(CameraFailureClasses, CameraDiagnosticMessageTest, testing::ValuesIn(DIAGNOSTIC_MESSAGE_CASES),
    case_name<DiagnosticMessageCase>);

TEST(CameraImport, rejectsUnsupportedNestedSignalDocument)
{
    const JsonImportResult result = import_fixture("fixtures/invalid-camera2d-nested.json");

    EXPECT_FALSE(result.succeeded());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_NE(std::string::npos, result.diagnostics.front().find("ParAnimator format"));
}

TEST(CameraImport, normalizesTinyAuthoredViewUp)
{
    const timeline::Document document = import_clean_document("fixtures/camera2d-tiny-view-up.json");
    const timeline::FrameGrid &grid = *document.frame_grid();

    const double up = curve(document, 9).sample(grid.frame_start(2));

    EXPECT_DOUBLE_EQ(1, up);
}

TEST(CameraImport, derivesRotationAfterNormalizingTinyViewUp)
{
    const timeline::Document document = import_clean_document("fixtures/camera2d-tiny-view-up.json");
    const timeline::FrameGrid &grid = *document.frame_grid();

    const double rotation = curve(document, 4).sample(grid.frame_start(2));

    EXPECT_DOUBLE_EQ(0, rotation);
}

TEST_F(AnalyticEyeTest, importsAnalyticEyeDocumentShape)
{
    EXPECT_EQ(13, document().lane_count());
    EXPECT_EQ(4, document().keyframe_count());
}

TEST_F(AnalyticEyeTest, preservesAnalyticEyeRecipe)
{
    const timeline::Curve &eye = curve(document(), 8);

    const ResolvedAttributes attributes = resolved_attributes(document(), eye.attributes());

    EXPECT_NE(std::string::npos, attributes.at("path").find("circle"));
}

TEST_F(AnalyticEyeTest, retainsAnalyticEyeDefinition)
{
    const timeline::Curve &eye = curve(document(), 8);

    EXPECT_TRUE(eye.samples().empty());
}

TEST_F(AnalyticEyeTest, preservesAnalyticEyeIdentity)
{
    const std::string_view identity = document().strings().lookup(document().lanes()[8].id());

    EXPECT_EQ("animation-0-eye[0]", identity);
}

TEST_F(AnalyticEyeTest, evaluatesAnalyticEyeBetweenFrames)
{
    const double eye = curve(document(), 8).sample(subdivided_frame(frame_grid(), 1, 2));

    EXPECT_NEAR(-std::sqrt(0.5), eye, 1e-12);
}

TEST_F(AnalyticEyeTest, derivesViewUpFromAnalyticEye)
{
    const timeline::Curve &up = curve(document(), 11);

    const double value = up.sample(frame_grid().frame_start(1));

    EXPECT_EQ("eye-look-at", resolved_attributes(document(), up.attributes()).at("derived-from"));
    EXPECT_DOUBLE_EQ(-1, value);
}

TEST_F(AnalyticEyeLayoutTest, hitTestsAnalyticEyePath)
{
    const std::optional<timeline::HitResult> hit = layout().hit_test({180, 286}, 2);

    ASSERT_TRUE(hit);
    EXPECT_EQ("animation-0-eye[0]-path", document().strings().lookup(hit->id.item_id));
}

TEST_F(AnalyticEyeLayoutTest, rendersDerivedViewUp)
{
    const std::string snapshot = timeline::render_snapshot(layout().display_list());

    EXPECT_NE(std::string::npos, snapshot.find("derived-view-up"));
}

TEST_F(EyeVariantsTest, preservesStaticEyePrecedence)
{
    const timeline::Curve &eye = curve(document(), 8);

    const double rotation = curve(document(), 4).sample(frame_grid().frame_start(1));
    const double eye_value = eye.sample(frame_grid().frame_start(1));

    EXPECT_DOUBLE_EQ(90, rotation);
    EXPECT_DOUBLE_EQ(4, eye_value);
}

TEST_F(EyeVariantsTest, importsEyeVariantsDocumentShape)
{
    EXPECT_EQ(28, document().lane_count());
}

TEST_F(EyeVariantsTest, preservesStaticEyeSourceRecipe)
{
    const ResolvedAttributes attributes = resolved_attributes(document(), curve(document(), 8).attributes());

    EXPECT_EQ("orbit.eye", attributes.at("parameter"));
}

TEST_F(EyeVariantsTest, preservesAuthoredViewUpSourceRecipe)
{
    const ResolvedAttributes attributes = resolved_attributes(document(), keyframe(document(), 13).attributes());

    EXPECT_EQ("orbit.view-up", attributes.at("parameter"));
}

TEST_F(EyeVariantsTest, evaluatesAuthoredViewUpWithStaticEye)
{
    const double x = *document().lanes()[13].evaluate_keyframes(frame_grid().frame_start(1));
    const double y = *document().lanes()[14].evaluate_keyframes(frame_grid().frame_start(1));

    EXPECT_DOUBLE_EQ(0, x);
    EXPECT_DOUBLE_EQ(2, y);
}

TEST_F(EyeVariantsCompositionTest, composesEyeVariantsWithMusicDocument)
{
    EXPECT_EQ(32, combined_document().lane_count());
}

TEST_F(EyeVariantsCompositionTest, preservesEyeValuesAfterDocumentComposition)
{
    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(combined_document(), 1);

    ASSERT_TRUE(inspection);
    EXPECT_DOUBLE_EQ(90, *inspection->lanes[8].items.front().value);
}

TEST_F(KeyedVariantsTest, importsKeyedVariantsDocumentShape)
{
    EXPECT_EQ(22, document().lane_count());
}

TEST_F(EyeVariantsTest, preservesMovingEyePrecedence)
{
    const double rotation = curve(document(), 19).sample(frame_grid().frame_start(2));
    const double up = curve(document(), 26).sample(frame_grid().frame_start(2));
    const double look = *document().lanes()[21].evaluate_keyframes(frame_grid().frame_start(2));
    const double eye = *document().lanes()[23].evaluate_keyframes(frame_grid().frame_start(2));

    EXPECT_DOUBLE_EQ(0, rotation);
    EXPECT_DOUBLE_EQ(0, up);
    EXPECT_DOUBLE_EQ(2, look);
    EXPECT_DOUBLE_EQ(2, eye);
}

TEST_F(CenterMagnificationTest, importsCenterMagnificationDocumentShape)
{
    EXPECT_EQ(11, document().lane_count());
    EXPECT_EQ(1, document().track_count());
    EXPECT_EQ(6, document().keyframe_count());
}

TEST_F(CenterMagnificationTest, evaluatesCenterMagnificationLikeParAnimator)
{
    const std::vector<std::array<double, 6>> expected = read_camera_frames("fixtures/gold-camera2d-center-mag.par");

    for (int frame = 0; frame < timeline::size_cast(expected); ++frame)
    {
        const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(document(), frame);
        ASSERT_TRUE(inspection);
        for (int component = 0; component < 3; ++component)
        {
            ASSERT_EQ(1, timeline::size_cast(inspection->lanes[component].items));
            ASSERT_TRUE(inspection->lanes[component].items.front().value);
            EXPECT_NEAR(expected[frame][component], *inspection->lanes[component].items.front().value, 1e-11);
        }
    }
    EXPECT_EQ(3, timeline::size_cast(expected));
}

TEST_F(CenterMagnificationTest, evaluatesNestedHeightLikeParAnimator)
{
    for (int frame = 0; frame < 3; ++frame)
    {
        const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(document(), frame);
        ASSERT_TRUE(inspection);
        ASSERT_TRUE(inspection->lanes[10].value);
        EXPECT_NEAR(3 * std::pow(0.5, frame / 2.0), *inspection->lanes[10].value, 1e-12);
    }
}

TEST_F(CenterMagnificationTest, preservesCenterMagnificationAnalyticDefinition)
{
    const timeline::Curve &magnification = curve(document(), 2);

    EXPECT_EQ(timeline::CurveInterpolation::ANALYTIC, magnification.interpolation());
    EXPECT_TRUE(magnification.samples().empty());
    EXPECT_EQ(frame_grid().frame_start(2), magnification.end());
}

TEST_F(CenterMagnificationTest, preservesCenterMagnificationSourceMetadata)
{
    const ResolvedAttributes attributes = resolved_attributes(document(), curve(document(), 2).attributes());

    EXPECT_EQ("center-mag", attributes.at("parameter"));
    EXPECT_EQ("magnification", attributes.at("component"));
    EXPECT_EQ("Mandel_Demo", attributes.at("source-entry"));
    EXPECT_NE(std::string::npos, attributes.at("camera2d").find("geometric"));
}

TEST_F(CenterMagnificationTest, preservesCenterMagnificationLookAtIdentity)
{
    const std::string_view identity = document().strings().lookup(document().lanes()[6].id());

    EXPECT_EQ("animation-0-look-at[0]", identity);
}

TEST_F(CenterMagnificationLayoutTest, hitTestsCenterMagnificationCurve)
{
    const std::optional<timeline::HitResult> hit = layout().hit_test({233, 35}, 2);

    ASSERT_TRUE(hit);
    EXPECT_EQ("animation-0-center-mag[0]-camera", document().strings().lookup(hit->id.item_id));
}

TEST_F(CenterMagnificationLayoutTest, rendersCenterMagnificationHeight)
{
    const std::string snapshot = timeline::render_snapshot(layout().display_list());

    EXPECT_NE(std::string::npos, snapshot.find("camera / height"));
}

TEST_F(KeyedVariantsTest, normalizesInterpolatedViewUp)
{
    const double x = curve(document(), 8).sample(frame_grid().frame_start(1));
    const double y = curve(document(), 9).sample(frame_grid().frame_start(1));

    EXPECT_NEAR(std::sqrt(0.5), x, 1e-12);
    EXPECT_NEAR(std::sqrt(0.5), y, 1e-12);
}

TEST_F(KeyedVariantsTest, evaluatesInterpolatedRotationAtFrameBoundary)
{
    const double rotation = curve(document(), 4).sample(frame_grid().frame_start(1));

    EXPECT_NEAR(45, rotation, 1e-12);
}

TEST_F(KeyedVariantsTest, evaluatesKeyedRotationAtFrameBoundary)
{
    const double rotation = curve(document(), 4).sample(frame_grid().frame_start(2));

    EXPECT_NEAR(90, rotation, 1e-12);
}

TEST_F(KeyedVariantsTest, evaluatesGeometricMagnification)
{
    const timeline::Curve &magnification = curve(document(), 2);

    const double first = magnification.sample(frame_grid().offset());
    const double middle = magnification.sample(frame_grid().frame_start(1));
    const double last = magnification.sample(frame_grid().frame_start(2));

    EXPECT_DOUBLE_EQ(2, first);
    EXPECT_NEAR(4.0 / 3.0, middle, 1e-12);
    EXPECT_DOUBLE_EQ(1, last);
}

TEST_F(KeyedVariantsTest, preservesNegativeSourceStretch)
{
    const double stretch = curve(document(), 3).sample(frame_grid().offset());

    EXPECT_DOUBLE_EQ(-2, stretch);
}

TEST_F(KeyedVariantsTest, evaluatesHeldMagnification)
{
    const double before = curve(document(), 13).sample(frame_grid().frame_start(1));
    const double after = curve(document(), 13).sample(frame_grid().frame_start(2));

    EXPECT_DOUBLE_EQ(2, before);
    EXPECT_DOUBLE_EQ(1, after);
}

TEST_F(KeyedVariantsTest, evaluatesGeometricRotationBetweenFrames)
{
    const double rotation = curve(document(), 4).sample(subdivided_frame(frame_grid(), 1, 2));

    EXPECT_NEAR(18.43494882292201, rotation, 1e-12);
}

TEST_F(KeyedVariantsTest, preservesViewUpSourceRecipe)
{
    const ResolvedAttributes attributes = resolved_attributes(document(), curve(document(), 8).attributes());

    EXPECT_NE(std::string::npos, attributes.at("signal").find("0/2"));
}

TEST_F(KeyedVariantsCompositionTest, composesKeyedVariantsWithMusicDocument)
{
    EXPECT_EQ(26, combined_document().lane_count());
}

TEST_F(KeyedVariantsCompositionTest, preservesKeyedCameraValuesAfterDocumentComposition)
{
    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(combined_document(), 1);

    ASSERT_TRUE(inspection);
    ASSERT_TRUE(inspection->lanes[8].items.front().value);
    EXPECT_NEAR(45, *inspection->lanes[8].items.front().value, 1e-12);
}

TEST_F(LayerSourceCameraTest, importsLayerCameraDocumentShape)
{
    EXPECT_EQ(22, document().lane_count());
}

TEST_P(LayerCameraDiagnosticTest, identifiesUnusableLayerSource)
{
    const LayerDiagnosticCase &definition = GetParam();

    ASSERT_GT(timeline::size_cast(result().diagnostics), definition.diagnostic);
    EXPECT_NE(std::string::npos, result().diagnostics[definition.diagnostic].find(definition.source));
}

INSTANTIATE_TEST_SUITE_P(LayerCameraFailures, LayerCameraDiagnosticTest, testing::ValuesIn(LAYER_DIAGNOSTIC_CASES),
    case_name<LayerDiagnosticCase>);

TEST_F(LayerSourceCameraTest, evaluatesSelectedLayerStretch)
{
    const timeline::Curve &stretch = curve(document(), 3);

    const double value = stretch.sample(frame_grid().offset());

    EXPECT_DOUBLE_EQ(-2, value);
}

TEST_F(LayerSourceCameraTest, preservesSelectedLayerIdentity)
{
    const ResolvedAttributes attributes = resolved_attributes(result(), curve(document(), 3).attributes());

    EXPECT_EQ("camera-0", attributes.at("layer"));
}

TEST_F(LayerSourceCameraTest, usesDefaultStretchForLayerView)
{
    const double stretch = curve(document(), 14).sample(frame_grid().offset());

    EXPECT_DOUBLE_EQ(1, stretch);
}

TEST_F(LayerSourceCameraTest, evaluatesLayerSourceMagnification)
{
    const timeline::Curve &magnification = curve(document(), 13);

    const double value = magnification.sample(frame_grid().frame_start(1));

    EXPECT_NEAR(std::sqrt(2.0), value, 1e-12);
}

TEST_F(LayerSourceCameraTest, preservesLayerSourceEntry)
{
    const ResolvedAttributes attributes = resolved_attributes(result(), curve(document(), 13).attributes());

    EXPECT_EQ("Zero_Demo", attributes.at("source-entry"));
}
