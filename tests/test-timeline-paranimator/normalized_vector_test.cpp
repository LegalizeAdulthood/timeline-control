// Copyright (c) 2026 Richard Thomson

#include <ResolvedAttributes.h>

#include <timelineParAnimator/TimelineJson.h>

#include <timeline/Layout.h>
#include <timeline/Query.h>
#include <timeline/size_cast.h>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace timeline_par_animator;

namespace
{

const std::array<std::string_view, 17> KEYED_PARAMETERS{"direction2", "direction2", "direction3", "direction3",
    "direction3", "hull2", "hull2", "line3", "line3", "line3", "tiny2", "tiny2", "raw3", "raw3", "raw3", "raw2",
    "raw2"};
const std::array<int, 17> KEYED_COMPONENTS{0, 1, 0, 1, 2, 0, 1, 0, 1, 2, 0, 1, 0, 1, 2, 0, 1};
const std::array<std::string_view, 12> CONTROL_POINT_PARAMETERS{"direction2", "direction2", "direction3", "direction3",
    "direction3", "raw3", "raw3", "raw3", "tiny2", "tiny2", "hull2", "hull2"};
const std::array<int, 12> CONTROL_POINT_COMPONENTS{0, 1, 0, 1, 2, 0, 1, 2, 0, 1, 0, 1};

/// Control-point fixture compared with source ParAnimator output.
///
struct ControlPointCase
{
    std::string name;
    std::string fixture;
    std::string golden;
    int lane_count;
};

/// Kind of authored value retained by an import.
///
enum class AuthoredValueKind
{
    KEY,
    FRAME
};

/// One independently verified authored component value.
///
struct AuthoredValueCase
{
    std::string name;
    std::string fixture;
    int lane;
    AuthoredValueKind kind;
    timeline::Ticks frame;
    double expected;
};

/// Recipe metadata retained on a normalized component.
///
struct RecipeCase
{
    std::string name;
    std::string fixture;
    int lane;
    std::string attribute;
    std::string expected;
    bool exact;
};

/// Consecutive normalized lanes expected to use unit component bounds.
///
struct BoundsCase
{
    std::string name;
    std::string fixture;
    int first_lane;
    int lane_count;
};

/// Imported document copied through document combination.
///
struct CopyCase
{
    std::string name;
    std::string fixture;
};

/// One normalized lane rendered and hit-tested after document combination.
///
struct LayoutCase
{
    std::string name;
    std::string fixture;
    int height;
    std::string lane_id;
    int point_count;
    int hit_point;
    std::string item_id;
};

/// One rejected normalized-vector track and its diagnostic reason.
///
struct InvalidTrackCase
{
    std::string name;
    std::string fixture;
    int diagnostic;
    int track;
    std::string reason;
};

/// Partially valid import retaining complete component sets.
///
struct PartialImportCase
{
    std::string name;
    std::string fixture;
    int diagnostic_count;
    int lane_count;
};

/// Lane retained from one partially valid normalized-vector import.
///
struct RetainedLaneCase
{
    std::string name;
    std::string fixture;
    int lane;
    std::string lane_id;
};

/// Location of a retained value in a partial import inspection.
///
enum class InspectedValueKind
{
    OUTPUT,
    ITEM
};

/// Cleaned value retained by a partially valid import.
///
struct PartialValueCase
{
    std::string name;
    std::string fixture;
    timeline::Ticks frame;
    int lane;
    InspectedValueKind kind;
    double expected;
};

/// Invalid document that cannot retain a usable timeline.
///
struct InvalidDocumentCase
{
    std::string name;
    std::string fixture;
};

void PrintTo(const ControlPointCase &value, std::ostream *output)
{
    *output << value.name;
}

void PrintTo(const AuthoredValueCase &value, std::ostream *output)
{
    *output << value.name;
}

void PrintTo(const RecipeCase &value, std::ostream *output)
{
    *output << value.name;
}

void PrintTo(const BoundsCase &value, std::ostream *output)
{
    *output << value.name;
}

void PrintTo(const CopyCase &value, std::ostream *output)
{
    *output << value.name;
}

void PrintTo(const LayoutCase &value, std::ostream *output)
{
    *output << value.name;
}

void PrintTo(const InvalidTrackCase &value, std::ostream *output)
{
    *output << value.name;
}

void PrintTo(const PartialImportCase &value, std::ostream *output)
{
    *output << value.name;
}

void PrintTo(const RetainedLaneCase &value, std::ostream *output)
{
    *output << value.name;
}

void PrintTo(const PartialValueCase &value, std::ostream *output)
{
    *output << value.name;
}

void PrintTo(const InvalidDocumentCase &value, std::ostream *output)
{
    *output << value.name;
}

template <typename Case>
std::string case_name(const testing::TestParamInfo<Case> &information)
{
    return information.param.name;
}

std::filesystem::path fixture(std::string_view name)
{
    return std::filesystem::path("fixtures") / std::string(name);
}

std::string read_text(const std::filesystem::path &path)
{
    std::ifstream input(path);
    if (!input)
    {
        throw std::runtime_error("Unable to open fixture: " + path.string());
    }
    return std::string{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

timeline::Document import_clean_document(std::string_view name)
{
    JsonImportResult result = import_timeline_json(fixture(name));
    if (!result.succeeded() || !result.diagnostics.empty())
    {
        throw std::runtime_error("Unable to import normalized-vector fixture: " + std::string(name));
    }
    return std::move(*result.document);
}

timeline::Document combine_with_music(const timeline::Document &document)
{
    const timeline::Document music = import_clean_document("beat-keys/rms.beat-keys.json");
    return timeline::combine_documents(document, music);
}

std::string vector_golden_entry(const std::string &golden, int frame)
{
    const std::string name = "frame-000" + std::to_string(frame + 1) + " {";
    const std::size_t start = golden.find(name);
    return start == std::string::npos ? std::string{} : golden.substr(start, golden.find('}', start) - start);
}

double vector_golden_component(const std::string &entry, std::string_view parameter, int component)
{
    const std::string name = "\n    " + std::string(parameter) + "=";
    std::size_t start = entry.find(name);
    if (start == std::string::npos)
    {
        throw std::runtime_error("missing source golden parameter");
    }
    start += name.size();
    for (int index = 0; index < component; ++index)
    {
        start = entry.find('/', start) + 1;
    }
    return std::stod(entry.substr(start));
}

const timeline::Attributes &lane_attributes(const timeline::Lane &lane)
{
    if (std::holds_alternative<timeline::Keyframe>(lane.items()[0]))
    {
        return std::get<timeline::Keyframe>(lane.items()[0]).attributes();
    }
    return std::get<timeline::Curve>(lane.items()[0]).attributes();
}

double sample_lane(const timeline::Lane &lane, timeline::Time time)
{
    if (std::holds_alternative<timeline::Keyframe>(lane.items()[0]))
    {
        const std::optional<double> output = lane.evaluate_keyframe_output(time);
        if (output)
        {
            return *output;
        }
        const std::optional<double> authored = lane.evaluate_keyframes(time);
        if (!authored)
        {
            throw std::runtime_error("normalized-vector lane has no value at the sampled time");
        }
        return *authored;
    }
    return std::get<timeline::Curve>(lane.items()[0]).sample(time);
}

const timeline::Polyline &find_polyline(const timeline::Layout &layout, std::string_view lane_id)
{
    for (const timeline::Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<timeline::Polyline>(primitive))
        {
            const timeline::Polyline &line = std::get<timeline::Polyline>(primitive);
            if (layout.display_list().strings().lookup(line.id.lane_id) == lane_id)
            {
                return line;
            }
        }
    }
    throw std::runtime_error("normalized-vector polyline is missing");
}

double authored_value(const timeline::Document &document, const AuthoredValueCase &test_case)
{
    const timeline::Lane &lane = document.lanes()[test_case.lane];
    if (test_case.kind == AuthoredValueKind::KEY)
    {
        return std::get<timeline::Keyframe>(lane.items()[0]).value();
    }
    return *lane.evaluate_keyframes(document.frame_grid()->frame_start(test_case.frame));
}

/// Parameterized normalized-vector control-point source fixture.
///
class NormalizedVectorControlPointTest : public testing::TestWithParam<ControlPointCase>
{
protected:
    void SetUp() override
    {
        m_document.emplace(import_clean_document(GetParam().fixture));
        m_golden = read_text(fixture(GetParam().golden));
    }

    std::optional<timeline::Document> m_document;
    std::string m_golden;
};

/// Parameterized normalized-vector authored-value fixture.
///
class NormalizedVectorAuthoredValueTest : public testing::TestWithParam<AuthoredValueCase>
{
protected:
    void SetUp() override
    {
        m_document.emplace(import_clean_document(GetParam().fixture));
    }

    std::optional<timeline::Document> m_document;
};

/// Parameterized normalized-vector recipe fixture.
///
class NormalizedVectorRecipeTest : public testing::TestWithParam<RecipeCase>
{
protected:
    void SetUp() override
    {
        m_document.emplace(import_clean_document(GetParam().fixture));
    }

    std::optional<timeline::Document> m_document;
};

/// Parameterized normalized-vector bounds fixture.
///
class NormalizedVectorBoundsTest : public testing::TestWithParam<BoundsCase>
{
protected:
    void SetUp() override
    {
        m_document.emplace(import_clean_document(GetParam().fixture));
    }

    std::optional<timeline::Document> m_document;
};

/// Parameterized normalized-vector document-combination fixture.
///
class NormalizedVectorCopyTest : public testing::TestWithParam<CopyCase>
{
protected:
    void SetUp() override
    {
        m_document.emplace(import_clean_document(GetParam().fixture));
        m_combined.emplace(combine_with_music(*m_document));
    }

    std::optional<timeline::Document> m_document;
    std::optional<timeline::Document> m_combined;
};

/// Parameterized normalized-vector composed-layout fixture.
///
class NormalizedVectorLayoutTest : public testing::TestWithParam<LayoutCase>
{
protected:
    void SetUp() override
    {
        m_document.emplace(import_clean_document(GetParam().fixture));
        m_combined.emplace(combine_with_music(*m_document));
        const timeline::FrameGrid &grid = *m_combined->frame_grid();
        m_layout.emplace(*m_combined, timeline::Viewport(600, GetParam().height, grid.offset(), grid.end_time()),
            timeline::LayoutMetrics(100, 20, 40, 4));
        m_line.emplace(find_polyline(*m_layout, GetParam().lane_id));
    }

    std::optional<timeline::Document> m_document;
    std::optional<timeline::Document> m_combined;
    std::optional<timeline::Layout> m_layout;
    std::optional<std::reference_wrapper<const timeline::Polyline>> m_line;
};

/// Parameterized rejected normalized-vector track fixture.
///
class NormalizedVectorInvalidTrackTest : public testing::TestWithParam<InvalidTrackCase>
{
protected:
    void SetUp() override
    {
        m_imported = import_timeline_json(fixture(GetParam().fixture));
    }

    JsonImportResult m_imported;
};

/// Parameterized partially valid normalized-vector import fixture.
///
class NormalizedVectorPartialImportTest : public testing::TestWithParam<PartialImportCase>
{
protected:
    void SetUp() override
    {
        m_imported = import_timeline_json(fixture(GetParam().fixture));
    }

    JsonImportResult m_imported;
};

/// Parameterized retained normalized-vector lane fixture.
///
class NormalizedVectorRetainedLaneTest : public testing::TestWithParam<RetainedLaneCase>
{
protected:
    void SetUp() override
    {
        m_imported = import_timeline_json(fixture(GetParam().fixture));
    }

    JsonImportResult m_imported;
};

/// Parameterized partial normalized-vector value fixture.
///
class NormalizedVectorPartialValueTest : public testing::TestWithParam<PartialValueCase>
{
protected:
    void SetUp() override
    {
        m_imported = import_timeline_json(fixture(GetParam().fixture));
    }

    JsonImportResult m_imported;
};

class NormalizedVectorInvalidDocumentTest : public testing::TestWithParam<InvalidDocumentCase>
{
};

/// Imported extreme normalized-vector document and its source oracle.
///
class ExtremeNormalizedVectorTest : public testing::Test
{
protected:
    const timeline::Document m_document{import_clean_document("extreme-normalized-vectors.json")};
    const timeline::FrameGrid m_grid{*m_document.frame_grid()};
    const std::string m_golden{read_text(fixture("gold-extreme-normalized-vectors.par"))};
};

/// Imported keyed normalized-vector document and its source oracle.
///
class KeyedNormalizedVectorTest : public testing::Test
{
protected:
    const timeline::Document m_document{import_clean_document("normalized-keyed-vectors.json")};
    const timeline::FrameGrid m_grid{*m_document.frame_grid()};
    const std::string m_golden{read_text(fixture("gold-normalized-keyed-vectors.par"))};
};

/// Imported Bezier normalized-vector document.
///
class BezierNormalizedVectorTest : public testing::Test
{
protected:
    const timeline::Document m_document{import_clean_document("normalized-bezier-vectors.json")};
    const timeline::FrameGrid m_grid{*m_document.frame_grid()};
};

/// Extreme normalized-vector document composed with music and laid out.
///
class ExtremeNormalizedVectorLayoutTest : public ExtremeNormalizedVectorTest
{
protected:
    const timeline::Document m_combined{combine_with_music(m_document)};
    const timeline::FrameGrid m_combined_grid{*m_combined.frame_grid()};
    const timeline::Layout m_layout{m_combined,
        timeline::Viewport(600, 1300, m_combined_grid.offset(), m_combined_grid.end_time()),
        timeline::LayoutMetrics(100, 20, 40, 4)};
};

TEST_F(ExtremeNormalizedVectorTest, matchesExtremeOutputAtFramesAndBetweenFrames)
{
    for (int sample = 0; sample < 9; ++sample)
    {
        SCOPED_TRACE(sample);
        const timeline::Time time =
            m_grid.offset() + timeline::Duration::from_ticks(sample * m_grid.frame_duration().ticks() / 2);
        const std::string entry = vector_golden_entry(m_golden, sample);
        ASSERT_FALSE(entry.empty());
        for (int lane_index = 0; lane_index < m_document.lane_count(); ++lane_index)
        {
            SCOPED_TRACE(lane_index);
            const timeline::Lane &lane = m_document.lanes()[lane_index];
            const ResolvedAttributes attributes = resolved_attributes(m_document, lane_attributes(lane));
            const double expected = vector_golden_component(
                entry, attributes.at("parameter"), std::stoi(std::string(attributes.at("component"))));
            const double actual = sample_lane(lane, time);
            EXPECT_NEAR(expected, actual, std::abs(expected) * 1e-11);
        }
    }
}

TEST_F(ExtremeNormalizedVectorTest, preservesExtremeSignedZero)
{
    int zero_count = 0;

    for (int sample = 0; sample < 9; ++sample)
    {
        const timeline::Time time =
            m_grid.offset() + timeline::Duration::from_ticks(sample * m_grid.frame_duration().ticks() / 2);
        const std::string entry = vector_golden_entry(m_golden, sample);
        for (int lane_index = 0; lane_index < m_document.lane_count(); ++lane_index)
        {
            const timeline::Lane &lane = m_document.lanes()[lane_index];
            const ResolvedAttributes attributes = resolved_attributes(m_document, lane_attributes(lane));
            const double expected = vector_golden_component(
                entry, attributes.at("parameter"), std::stoi(std::string(attributes.at("component"))));
            if (expected == 0)
            {
                EXPECT_EQ(std::signbit(expected), std::signbit(sample_lane(lane, time)));
                ++zero_count;
            }
        }
    }
    EXPECT_GT(zero_count, 0);
}

TEST_F(ExtremeNormalizedVectorTest, matchesExtremeLaneOutput)
{
    for (timeline::Ticks frame = 0; frame < m_grid.frame_count(); ++frame)
    {
        SCOPED_TRACE(frame);
        const timeline::FrameInspection inspection = *timeline::inspect_frame(m_document, frame);
        for (int lane_index = 0; lane_index < m_document.lane_count(); ++lane_index)
        {
            const timeline::Lane &lane = m_document.lanes()[lane_index];
            const double expected = sample_lane(lane, m_grid.frame_start(frame));
            const std::optional<double> actual = std::holds_alternative<timeline::Keyframe>(lane.items()[0])
                ? inspection.lanes[lane_index].output_value
                : inspection.lanes[lane_index].items[0].value;
            ASSERT_TRUE(actual);
            EXPECT_DOUBLE_EQ(expected, *actual);
        }
    }
}

TEST_F(KeyedNormalizedVectorTest, matchesKeyedOutputAtEveryFrame)
{
    for (timeline::Ticks frame = 0; frame < m_grid.frame_count(); ++frame)
    {
        SCOPED_TRACE(frame);
        const std::string entry = vector_golden_entry(m_golden, static_cast<int>(frame));
        const timeline::FrameInspection inspection = *timeline::inspect_frame(m_document, frame);
        for (int lane = 0; lane < m_document.lane_count(); ++lane)
        {
            SCOPED_TRACE(lane);
            const std::optional<double> value =
                lane < 12 ? inspection.lanes[lane].output_value : inspection.lanes[lane].value;
            ASSERT_TRUE(value);
            EXPECT_NEAR(vector_golden_component(entry, KEYED_PARAMETERS[lane], KEYED_COMPONENTS[lane]), *value, 1e-11);
        }
    }
}

TEST_F(KeyedNormalizedVectorTest, distinguishesNormalizedAndRawKeyedLanes)
{
    const timeline::FrameInspection inspection = *timeline::inspect_frame(m_document, 0);

    for (int lane = 0; lane < m_document.lane_count(); ++lane)
    {
        EXPECT_EQ(lane < 12, inspection.lanes[lane].output_value.has_value());
    }
}

TEST_P(NormalizedVectorControlPointTest, matchesSourceOutputAtEveryFrame)
{
    ASSERT_EQ(GetParam().lane_count, m_document->lane_count());
    for (timeline::Ticks frame = 0; frame < m_document->frame_grid()->frame_count(); ++frame)
    {
        SCOPED_TRACE(frame);
        const std::string entry = vector_golden_entry(m_golden, static_cast<int>(frame));
        const timeline::FrameInspection inspection = *timeline::inspect_frame(*m_document, frame);
        for (int lane = 0; lane < m_document->lane_count(); ++lane)
        {
            SCOPED_TRACE(lane);
            ASSERT_EQ(1, timeline::size_cast(inspection.lanes[lane].items));
            ASSERT_TRUE(inspection.lanes[lane].items[0].value);
            EXPECT_NEAR(vector_golden_component(entry, CONTROL_POINT_PARAMETERS[lane], CONTROL_POINT_COMPONENTS[lane]),
                *inspection.lanes[lane].items[0].value, 1e-11);
        }
    }
}

TEST_P(NormalizedVectorAuthoredValueTest, preservesValue)
{
    const double actual = authored_value(*m_document, GetParam());

    EXPECT_DOUBLE_EQ(GetParam().expected, actual);
}

TEST_P(NormalizedVectorRecipeTest, preservesMetadata)
{
    const ResolvedAttributes attributes =
        resolved_attributes(*m_document, lane_attributes(m_document->lanes()[GetParam().lane]));

    const std::string_view actual = attributes.at(GetParam().attribute);

    if (GetParam().exact)
    {
        EXPECT_EQ(GetParam().expected, actual);
    }
    else
    {
        EXPECT_NE(std::string_view::npos, actual.find(GetParam().expected));
    }
}

TEST_P(NormalizedVectorBoundsTest, usesUnitComponentBounds)
{
    std::vector<std::pair<double, double>> bounds;
    for (int lane = GetParam().first_lane; lane < GetParam().first_lane + GetParam().lane_count; ++lane)
    {
        const timeline::Curve &curve = std::get<timeline::Curve>(m_document->lanes()[lane].items()[0]);
        bounds.emplace_back(*curve.minimum(), *curve.maximum());
    }

    ASSERT_EQ(GetParam().lane_count, timeline::size_cast(bounds));
    for (const std::pair<double, double> &bound : bounds)
    {
        EXPECT_DOUBLE_EQ(-1, bound.first);
        EXPECT_DOUBLE_EQ(1, bound.second);
    }
}

TEST_P(NormalizedVectorCopyTest, preservesLaneIdentity)
{
    ASSERT_GE(m_combined->lane_count(), m_document->lane_count());
    for (int lane = 0; lane < m_document->lane_count(); ++lane)
    {
        EXPECT_EQ(m_document->lanes()[lane].id(), m_combined->lanes()[lane].id());
    }
}

TEST_P(NormalizedVectorCopyTest, preservesItemAttributes)
{
    ASSERT_GE(m_combined->lane_count(), m_document->lane_count());
    for (int lane = 0; lane < m_document->lane_count(); ++lane)
    {
        EXPECT_EQ(lane_attributes(m_document->lanes()[lane]), lane_attributes(m_combined->lanes()[lane]));
    }
}

TEST_P(NormalizedVectorCopyTest, preservesComparedOutput)
{
    const timeline::Time time = m_document->frame_grid()->frame_start(2);

    ASSERT_GE(m_combined->lane_count(), m_document->lane_count());
    for (int lane = 0; lane < m_document->lane_count(); ++lane)
    {
        EXPECT_DOUBLE_EQ(sample_lane(m_document->lanes()[lane], time), sample_lane(m_combined->lanes()[lane], time));
    }
}

TEST_F(ExtremeNormalizedVectorLayoutTest, drawsEveryExtremeComponent)
{
    int found = 0;

    for (const timeline::Primitive &primitive : m_layout.display_list().primitives())
    {
        if (std::holds_alternative<timeline::Polyline>(primitive))
        {
            const timeline::Polyline &line = std::get<timeline::Polyline>(primitive);
            if (m_layout.display_list().strings().lookup(line.id.lane_id).substr(0, 10) == "animation-")
            {
                ++found;
            }
        }
    }

    EXPECT_EQ(25, found);
}

TEST_F(ExtremeNormalizedVectorLayoutTest, producesFiniteExtremePoints)
{
    int point_count = 0;

    for (const timeline::Primitive &primitive : m_layout.display_list().primitives())
    {
        if (std::holds_alternative<timeline::Polyline>(primitive))
        {
            const timeline::Polyline &line = std::get<timeline::Polyline>(primitive);
            if (m_layout.display_list().strings().lookup(line.id.lane_id).substr(0, 10) == "animation-")
            {
                for (const timeline::Point &point : line.points)
                {
                    EXPECT_TRUE(std::isfinite(point.x));
                    EXPECT_TRUE(std::isfinite(point.y));
                    ++point_count;
                }
            }
        }
    }

    EXPECT_GT(point_count, 0);
}

TEST_P(NormalizedVectorLayoutTest, drawsExpectedPolyline)
{
    const int point_count = timeline::size_cast(m_line->get().points);

    EXPECT_EQ(GetParam().point_count, point_count);
}

TEST_P(NormalizedVectorLayoutTest, preservesHitIdentity)
{
    const std::optional<timeline::HitResult> hit = m_layout->hit_test(m_line->get().points[GetParam().hit_point], 0);

    ASSERT_TRUE(hit);
    EXPECT_EQ(GetParam().lane_id, m_layout->display_list().strings().lookup(hit->id.lane_id));
    EXPECT_EQ(GetParam().item_id, m_layout->display_list().strings().lookup(hit->id.item_id));
}

TEST_F(ExtremeNormalizedVectorLayoutTest, preservesExtremeLaneIdentity)
{
    int hit_count = 0;

    for (const timeline::Primitive &primitive : m_layout.display_list().primitives())
    {
        if (std::holds_alternative<timeline::Polyline>(primitive))
        {
            const timeline::Polyline &line = std::get<timeline::Polyline>(primitive);
            if (m_layout.display_list().strings().lookup(line.id.lane_id).substr(0, 10) == "animation-")
            {
                const std::optional<timeline::HitResult> hit = m_layout.hit_test(line.points.front(), 0);
                ASSERT_TRUE(hit);
                EXPECT_EQ(line.id.lane_id, hit->id.lane_id);
                ++hit_count;
            }
        }
    }

    EXPECT_EQ(25, hit_count);
}

TEST_F(ExtremeNormalizedVectorLayoutTest, assignsExtremeItemIdentity)
{
    int hit_count = 0;

    for (const timeline::Primitive &primitive : m_layout.display_list().primitives())
    {
        if (std::holds_alternative<timeline::Polyline>(primitive))
        {
            const timeline::Polyline &line = std::get<timeline::Polyline>(primitive);
            if (m_layout.display_list().strings().lookup(line.id.lane_id).substr(0, 10) == "animation-")
            {
                const std::optional<timeline::HitResult> hit = m_layout.hit_test(line.points.front(), 0);
                ASSERT_TRUE(hit);
                EXPECT_FALSE(hit->id.item_id.empty());
                ++hit_count;
            }
        }
    }

    EXPECT_EQ(25, hit_count);
}

TEST_F(KeyedNormalizedVectorTest, normalizesKeyedValuesAfterInterpolation)
{
    const timeline::Time half = m_grid.offset() + timeline::Duration::from_ticks(m_grid.frame_duration().ticks() / 2);
    const timeline::Lane &lane = m_document.lanes()[0];

    const double authored = *lane.evaluate_keyframes(half);
    const double normalized = *lane.evaluate_keyframe_output(half);

    EXPECT_DOUBLE_EQ(8.75, authored);
    EXPECT_NEAR(8.75 / std::sqrt(8.75 * 8.75 + 2.5 * 2.5), normalized, 1e-14);
}

TEST_F(BezierNormalizedVectorTest, normalizesCurvesAfterInterpolation)
{
    const timeline::Time half = m_grid.offset() + timeline::Duration::from_ticks(m_grid.frame_duration().ticks() / 2);
    const timeline::Curve &x = std::get<timeline::Curve>(m_document.lanes()[0].items()[0]);
    const timeline::Curve &y = std::get<timeline::Curve>(m_document.lanes()[1].items()[0]);
    const double length = std::sqrt(8.75 * 8.75 + 2.5 * 2.5);

    const double normalized_x = x.sample(half);
    const double normalized_y = y.sample(half);

    EXPECT_NEAR(8.75 / length, normalized_x, 1e-14);
    EXPECT_NEAR(2.5 / length, normalized_y, 1e-14);
}

TEST_F(BezierNormalizedVectorTest, cleansTinyCurveComponentsAtFrameBoundaries)
{
    const timeline::Curve &zero = std::get<timeline::Curve>(m_document.lanes()[8].items()[0]);
    const timeline::Curve &one = std::get<timeline::Curve>(m_document.lanes()[9].items()[0]);

    std::vector<std::pair<double, double>> values;
    for (timeline::Ticks frame = 0; frame < m_grid.frame_count(); ++frame)
    {
        values.emplace_back(zero.sample(m_grid.frame_start(frame)), one.sample(m_grid.frame_start(frame)));
    }

    for (const std::pair<double, double> &value : values)
    {
        EXPECT_DOUBLE_EQ(0, value.first);
        EXPECT_DOUBLE_EQ(1, value.second);
    }
}

TEST_F(BezierNormalizedVectorTest, leavesPointTargetUnnormalized)
{
    const timeline::Curve &curve = std::get<timeline::Curve>(m_document.lanes()[5].items()[0]);

    const double value = curve.sample(m_grid.frame_start(2));
    const ResolvedAttributes attributes = resolved_attributes(m_document, curve.attributes());

    EXPECT_DOUBLE_EQ(1.5, value);
    EXPECT_EQ(0, attributes.count("normalize"));
}

TEST_F(ExtremeNormalizedVectorTest, normalizesExtremeKeyedValue)
{
    const double output = *m_document.lanes()[0].evaluate_keyframe_output(m_grid.frame_start(2));

    EXPECT_DOUBLE_EQ(0, output);
}

TEST_F(ExtremeNormalizedVectorTest, normalizesTinyExtremeValue)
{
    const double output = *m_document.lanes()[6].evaluate_keyframe_output(m_grid.frame_start(2));

    EXPECT_GT(output, 0);
}

TEST_F(KeyedNormalizedVectorTest, normalizesTinyKeyedValue)
{
    const double output = *m_document.lanes()[10].evaluate_keyframe_output(m_grid.frame_start(2));

    EXPECT_NEAR(1 / std::sqrt(17.0), output, 1e-14);
}

TEST_P(NormalizedVectorInvalidTrackTest, reportsRejectedTrackAndReason)
{
    const std::vector<std::string> &diagnostics = m_imported.diagnostics;

    ASSERT_GT(timeline::size_cast(diagnostics), GetParam().diagnostic);
    EXPECT_NE(std::string::npos,
        diagnostics[GetParam().diagnostic].find("animation-" + std::to_string(GetParam().track) + ":"));
    EXPECT_NE(std::string::npos, diagnostics[GetParam().diagnostic].find(GetParam().reason));
}

TEST_P(NormalizedVectorPartialImportTest, reportsExpectedDiagnosticCount)
{
    const int diagnostic_count = timeline::size_cast(m_imported.diagnostics);

    EXPECT_EQ(GetParam().diagnostic_count, diagnostic_count);
}

TEST_P(NormalizedVectorPartialImportTest, retainsOnlyCompleteLanes)
{
    const int lane_count = m_imported.document ? m_imported.document->lane_count() : 0;

    ASSERT_TRUE(m_imported.succeeded());
    EXPECT_EQ(GetParam().lane_count, lane_count);
}

TEST_P(NormalizedVectorRetainedLaneTest, preservesLaneIdentity)
{
    const std::string_view lane_id = m_imported.document
        ? m_imported.document->strings().lookup(m_imported.document->lanes()[GetParam().lane].id())
        : std::string_view{};

    ASSERT_TRUE(m_imported.succeeded());
    EXPECT_EQ(GetParam().lane_id, lane_id);
}

TEST_P(NormalizedVectorPartialValueTest, preservesCleanedOutput)
{
    const std::optional<timeline::FrameInspection> inspection = m_imported.document
        ? timeline::inspect_frame(*m_imported.document, GetParam().frame)
        : std::optional<timeline::FrameInspection>{};
    const std::optional<double> value = !inspection     ? std::optional<double>{}
        : GetParam().kind == InspectedValueKind::OUTPUT ? inspection->lanes[GetParam().lane].output_value
                                                        : inspection->lanes[GetParam().lane].items[0].value;

    const double actual = value.value_or(std::numeric_limits<double>::quiet_NaN());

    ASSERT_TRUE(m_imported.succeeded());
    ASSERT_TRUE(value);
    EXPECT_DOUBLE_EQ(GetParam().expected, actual);
}

TEST_P(NormalizedVectorInvalidDocumentTest, rejectsDocument)
{
    const JsonImportResult imported = import_timeline_json(fixture(GetParam().fixture));

    const bool succeeded = imported.succeeded();

    EXPECT_FALSE(succeeded);
}

INSTANTIATE_TEST_SUITE_P(ControlPoints, NormalizedVectorControlPointTest,
    testing::Values(
        ControlPointCase{"Bezier", "normalized-bezier-vectors.json", "gold-normalized-bezier-vectors.par", 12},
        ControlPointCase{
            "CatmullRom", "normalized-catmull-rom-vectors.json", "gold-normalized-catmull-rom-vectors.par", 8}),
    case_name<ControlPointCase>);

INSTANTIATE_TEST_SUITE_P(AuthoredValues, NormalizedVectorAuthoredValueTest,
    testing::Values(
        AuthoredValueCase{"ExtremeFirstKey", "extreme-normalized-vectors.json", 0, AuthoredValueKind::KEY, 0, 1e200},
        AuthoredValueCase{
            "ExtremeInterpolatedKey", "extreme-normalized-vectors.json", 0, AuthoredValueKind::FRAME, 2, 1.5e200},
        AuthoredValueCase{"ExtremeTinyKey", "extreme-normalized-vectors.json", 6, AuthoredValueKind::FRAME, 2, 5e-13},
        AuthoredValueCase{"KeyedFirstComponent", "normalized-keyed-vectors.json", 0, AuthoredValueKind::FRAME, 2, 5},
        AuthoredValueCase{"KeyedSecondComponent", "normalized-keyed-vectors.json", 1, AuthoredValueKind::FRAME, 2, 10},
        AuthoredValueCase{
            "KeyedTinyComponent", "normalized-keyed-vectors.json", 10, AuthoredValueKind::FRAME, 2, 5e-13},
        AuthoredValueCase{"KeyedFirstKey", "normalized-keyed-vectors.json", 0, AuthoredValueKind::KEY, 0, 10}),
    case_name<AuthoredValueCase>);

INSTANTIATE_TEST_SUITE_P(Recipes, NormalizedVectorRecipeTest,
    testing::Values(
        RecipeCase{"ExtremeKeyTrack", "extreme-normalized-vectors.json", 0, "track-definition", "value", false},
        RecipeCase{
            "ExtremeCurveTrack", "extreme-normalized-vectors.json", 11, "track-definition", "control-points", false},
        RecipeCase{"ExtremeCurvePath", "extreme-normalized-vectors.json", 11, "path", "1e200/1", false},
        RecipeCase{
            "ExtremeCurveCatalog", "extreme-normalized-vectors.json", 11, "catalog-definition", "vector2", false},
        RecipeCase{"KeyedTrack", "normalized-keyed-vectors.json", 0, "track-definition", "direction2", false},
        RecipeCase{"KeyedCatalog", "normalized-keyed-vectors.json", 0, "catalog-definition", "vector2", false},
        RecipeCase{"KeyedNormalize", "normalized-keyed-vectors.json", 0, "normalize", "true", true},
        RecipeCase{"KeyedComponent", "normalized-keyed-vectors.json", 0, "component", "0", true},
        RecipeCase{"CurveNormalize", "normalized-bezier-vectors.json", 0, "normalize", "true", true},
        RecipeCase{"CurvePath", "normalized-bezier-vectors.json", 0, "path", "10/0", false},
        RecipeCase{"CurveCatalog", "normalized-bezier-vectors.json", 0, "catalog-definition", "vector2", false},
        RecipeCase{
            "CurveTrackDefinition", "normalized-bezier-vectors.json", 0, "track-definition", "direction2", false},
        RecipeCase{"CurveComponent", "normalized-bezier-vectors.json", 0, "component", "0", true},
        RecipeCase{"CurveTrack", "normalized-bezier-vectors.json", 0, "track", "animation-0", true}),
    case_name<RecipeCase>);

INSTANTIATE_TEST_SUITE_P(Bounds, NormalizedVectorBoundsTest,
    testing::Values(BoundsCase{"ExtremeCurves", "extreme-normalized-vectors.json", 11, 14},
        BoundsCase{"BezierCurve", "normalized-bezier-vectors.json", 0, 1}),
    case_name<BoundsCase>);

INSTANTIATE_TEST_SUITE_P(Copies, NormalizedVectorCopyTest,
    testing::Values(CopyCase{"Extreme", "extreme-normalized-vectors.json"},
        CopyCase{"Keyed", "normalized-keyed-vectors.json"}, CopyCase{"Bezier", "normalized-bezier-vectors.json"}),
    case_name<CopyCase>);

INSTANTIATE_TEST_SUITE_P(Layouts, NormalizedVectorLayoutTest,
    testing::Values(
        LayoutCase{"Keyed", "normalized-keyed-vectors.json", 900, "animation-0[0]", 2, 0, "animation-0-key-0"},
        LayoutCase{"Bezier", "normalized-bezier-vectors.json", 700, "animation-0[0]", 5, 2, "animation-0[0]-path"}),
    case_name<LayoutCase>);

INSTANTIATE_TEST_SUITE_P(InvalidTracks, NormalizedVectorInvalidTrackTest,
    testing::Values(InvalidTrackCase{"ExtremeZero", "extreme-normalized-invalid.json", 0, 0, "nonzero"},
        InvalidTrackCase{"ExtremeTiny", "extreme-normalized-invalid.json", 1, 1, "nonzero"},
        InvalidTrackCase{"ExtremeKeyedCrossing", "extreme-normalized-invalid.json", 2, 2, "singular"},
        InvalidTrackCase{"ExtremeBezierCrossing", "extreme-normalized-invalid.json", 3, 3, "singular"},
        InvalidTrackCase{"ExtremeCatmullCrossing", "extreme-normalized-invalid.json", 4, 4, "singular"},
        InvalidTrackCase{"ExtremeUnresolved", "extreme-normalized-invalid.json", 5, 5, "unresolved"},
        InvalidTrackCase{"ExtremeCleanedZero", "extreme-normalized-invalid.json", 6, 6, "nonzero"},
        InvalidTrackCase{"KeyedCrossing", "normalized-keyed-partial.json", 0, 0, "normalization"},
        InvalidTrackCase{"KeyedLinePath", "normalized-keyed-partial.json", 1, 1, "numeric array"},
        InvalidTrackCase{"KeyedConstantPath", "normalized-keyed-partial.json", 2, 2, "numeric array"},
        InvalidTrackCase{"KeyedExtreme", "normalized-keyed-partial.json", 3, 4, "normalization"},
        InvalidTrackCase{"KeyedArity", "normalized-keyed-partial.json", 4, 5, "arity"},
        InvalidTrackCase{"KeyedCurve", "normalized-keyed-partial.json", 5, 6, "curves"},
        InvalidTrackCase{"KeyedFrameRange", "normalized-keyed-partial.json", 6, 7, "full frame range"},
        InvalidTrackCase{"KeyedStringValue", "normalized-keyed-partial.json", 7, 8, "numeric array"},
        InvalidTrackCase{"KeyedBounds", "normalized-keyed-partial.json", 8, 9, "bounds"},
        InvalidTrackCase{"KeyedExtrapolationZero", "normalized-keyed-partial.json", 9, 10, "extrapolation"},
        InvalidTrackCase{"KeyedExtrapolationOne", "normalized-keyed-partial.json", 10, 11, "extrapolation"},
        InvalidTrackCase{"KeyedExtrapolationTwo", "normalized-keyed-partial.json", 11, 12, "extrapolation"},
        InvalidTrackCase{"KeyedExtrapolationThree", "normalized-keyed-partial.json", 12, 13, "extrapolation"},
        InvalidTrackCase{"CurveZero", "normalized-vector-partial.json", 0, 0, "normalization"},
        InvalidTrackCase{"CurveCrossing", "normalized-vector-partial.json", 1, 1, "normalization"},
        InvalidTrackCase{"CurveBezierCrossing", "normalized-vector-partial.json", 2, 2, "normalization"},
        InvalidTrackCase{"CurveCatmullCrossing", "normalized-vector-partial.json", 3, 3, "normalization"},
        InvalidTrackCase{"CurveArity", "normalized-vector-partial.json", 4, 5, "arity"},
        InvalidTrackCase{"CurveCleanedZero", "normalized-vector-partial.json", 5, 6, "normalization"},
        InvalidTrackCase{"CurveKeyedTarget", "normalized-vector-partial.json", 6, 7, "normalization"},
        InvalidTrackCase{"CurveConstantTarget", "normalized-vector-partial.json", 7, 8, "normalization"},
        InvalidTrackCase{"CurveLineTarget", "normalized-vector-partial.json", 8, 9, "normalization"}),
    case_name<InvalidTrackCase>);

INSTANTIATE_TEST_SUITE_P(PartialImports, NormalizedVectorPartialImportTest,
    testing::Values(PartialImportCase{"Keyed", "normalized-keyed-partial.json", 13, 4},
        PartialImportCase{"Curves", "normalized-vector-partial.json", 9, 4}),
    case_name<PartialImportCase>);

INSTANTIATE_TEST_SUITE_P(RetainedLanes, NormalizedVectorRetainedLaneTest,
    testing::Values(RetainedLaneCase{"KeyedFirst", "normalized-keyed-partial.json", 0, "animation-3[0]"},
        RetainedLaneCase{"KeyedSecond", "normalized-keyed-partial.json", 2, "animation-14[0]"},
        RetainedLaneCase{"CurveFirst", "normalized-vector-partial.json", 0, "animation-4[0]"},
        RetainedLaneCase{"CurveSecond", "normalized-vector-partial.json", 2, "animation-10[0]"}),
    case_name<RetainedLaneCase>);

INSTANTIATE_TEST_SUITE_P(PartialValues, NormalizedVectorPartialValueTest,
    testing::Values(PartialValueCase{"KeyedZero", "normalized-keyed-partial.json", 3, 0, InspectedValueKind::OUTPUT, 0},
        PartialValueCase{"KeyedPositive", "normalized-keyed-partial.json", 3, 2, InspectedValueKind::OUTPUT, 1},
        PartialValueCase{"KeyedNegative", "normalized-keyed-partial.json", 4, 2, InspectedValueKind::OUTPUT, -1},
        PartialValueCase{"CurveZero", "normalized-vector-partial.json", 2, 0, InspectedValueKind::ITEM, 0}),
    case_name<PartialValueCase>);

INSTANTIATE_TEST_SUITE_P(InvalidDocuments, NormalizedVectorInvalidDocumentTest,
    testing::Values(InvalidDocumentCase{"Extreme", "extreme-normalized-invalid.json"},
        InvalidDocumentCase{"Keyed", "normalized-keyed-invalid.json"},
        InvalidDocumentCase{"Curves", "normalized-vector-invalid.json"}),
    case_name<InvalidDocumentCase>);

} // namespace
