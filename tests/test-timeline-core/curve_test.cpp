// Copyright (c) 2026 Richard Thomson

#include <timeline/Curve.h>
#include <timeline/size_cast.h>

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

using namespace timeline;

namespace
{

constexpr StringId SIGNAL_KIND{2};
constexpr StringId RMS_KIND{3};
constexpr StringId RMS_LABEL{4};

Time at(Ticks ticks)
{
    return Time::from_ticks(ticks);
}

Curve quadratic_curve()
{
    const double scale = 0.01;
    return Curve(StringId{1}, SIGNAL_KIND, at(0), at(20),
        [scale](Time time) { return scale * static_cast<double>(time.ticks() * time.ticks()); });
}

double tick_value(Time time)
{
    return static_cast<double>(time.ticks());
}

double infinity_at_ten(Time time)
{
    return time == at(10) ? std::numeric_limits<double>::infinity() : 0.0;
}

double two_at_ten(Time time)
{
    return time == at(10) ? 2.0 : 0.0;
}

} // namespace

TEST(CurveInterpolation, convertsEveryValueToString)
{
    EXPECT_EQ("linear", to_string(CurveInterpolation::LINEAR));
    EXPECT_EQ("step", to_string(CurveInterpolation::STEP));
    EXPECT_EQ("analytic", to_string(CurveInterpolation::ANALYTIC));
}

TEST(Curve, ownsAnalyticDefinition)
{
    const Curve curve = quadratic_curve();

    EXPECT_EQ(CurveInterpolation::ANALYTIC, curve.interpolation());
    EXPECT_EQ(0, curve.sample_count());
    EXPECT_EQ(at(0), curve.start());
    EXPECT_EQ(at(20), curve.end());
}

TEST(Curve, samplesAnalyticEvaluator)
{
    const Curve curve = quadratic_curve();

    const double value = curve.sample(at(5));

    EXPECT_DOUBLE_EQ(0.25, value);
}

TEST(Curve, clampsAnalyticSamplingToDomain)
{
    const Curve curve = quadratic_curve();

    const double before = curve.sample(at(-1));
    const double after = curve.sample(at(21));

    EXPECT_DOUBLE_EQ(0.0, before);
    EXPECT_DOUBLE_EQ(4.0, after);
}

TEST(Curve, copiesAnalyticEvaluator)
{
    const Curve curve = quadratic_curve();

    const Curve copy = curve;

    EXPECT_DOUBLE_EQ(1.0, copy.sample(at(10)));
    EXPECT_EQ(0, copy.sample_count());
}

TEST(Curve, samplesAnalyticDefinitionAtFrameGridBoundaries)
{
    const Curve curve = quadratic_curve();
    const FrameGrid grid(Timebase(10), 3, 1, 1);

    const std::vector<CurveSample> samples = curve.sample(grid);

    ASSERT_EQ(3, size_cast(samples));
    EXPECT_EQ(at(0), samples[0].time());
    EXPECT_DOUBLE_EQ(0.0, samples[0].value());
    EXPECT_EQ(at(10), samples[1].time());
    EXPECT_DOUBLE_EQ(1.0, samples[1].value());
    EXPECT_EQ(at(20), samples[2].time());
    EXPECT_DOUBLE_EQ(4.0, samples[2].value());
    EXPECT_EQ(0, curve.sample_count());
}

TEST(Curve, rejectsEmptyAnalyticIdentity)
{
    EXPECT_THROW(Curve(StringId{}, SIGNAL_KIND, at(0), at(20), tick_value), std::invalid_argument);
}

TEST(Curve, rejectsNonpositiveAnalyticDomains)
{
    EXPECT_THROW(Curve(StringId{1}, SIGNAL_KIND, at(0), at(0), tick_value), std::invalid_argument);
    EXPECT_THROW(Curve(StringId{1}, SIGNAL_KIND, at(20), at(0), tick_value), std::invalid_argument);
}

TEST(Curve, rejectsMissingAnalyticEvaluator)
{
    EXPECT_THROW(Curve(StringId{1}, SIGNAL_KIND, at(0), at(20), CurveEvaluator{}), std::invalid_argument);
}

TEST(Curve, rejectsInvalidAnalyticBounds)
{
    EXPECT_THROW(Curve(StringId{1}, SIGNAL_KIND, at(0), at(20), tick_value, {}, 20.0, 0.0, {}), std::invalid_argument);
}

TEST(Curve, rejectsOutOfBoundsAnalyticEndpoint)
{
    EXPECT_THROW(Curve(StringId{1}, SIGNAL_KIND, at(0), at(20), tick_value, {}, 0.0, 1.0, {}), std::out_of_range);
}

TEST(Curve, rejectsNonfiniteAnalyticValueDuringSampling)
{
    const Curve curve(StringId{1}, SIGNAL_KIND, at(0), at(20), infinity_at_ten);

    EXPECT_THROW(curve.sample(at(10)), std::invalid_argument);
}

TEST(Curve, rejectsOutOfBoundsAnalyticValueDuringSampling)
{
    const Curve curve(StringId{1}, SIGNAL_KIND, at(0), at(20), two_at_ten, {}, 0.0, 1.0, {});

    EXPECT_THROW(curve.sample(at(10)), std::out_of_range);
}

TEST(Curve, storesExactSamples)
{
    const Curve curve(StringId{1}, RMS_KIND, {{at(0), 0.0}, {at(10), 1.0}, {at(20), 0.0}}, RMS_LABEL,
        CurveInterpolation::LINEAR, 0.0, 1.0, {});

    ASSERT_EQ(3, curve.sample_count());
    EXPECT_EQ(at(10), curve.samples()[1].time());
    EXPECT_DOUBLE_EQ(1.0, curve.samples()[1].value());
}

TEST(Curve, samplesExactStoredValue)
{
    const Curve curve(StringId{1}, RMS_KIND, {{at(0), 0.0}, {at(10), 1.0}, {at(20), 0.0}});

    const double value = curve.sample(at(10));

    EXPECT_DOUBLE_EQ(1.0, value);
}

TEST(Curve, linearlyInterpolatesBetweenSamples)
{
    const Curve curve(StringId{1}, RMS_KIND, {{at(0), 0.0}, {at(10), 1.0}, {at(20), 0.0}});

    const double value = curve.sample(at(5));

    EXPECT_DOUBLE_EQ(0.5, value);
}

TEST(Curve, clampsSampledSamplingToDomain)
{
    const Curve curve(StringId{1}, RMS_KIND, {{at(0), 0.0}, {at(10), 1.0}, {at(20), 0.0}});

    const double before = curve.sample(at(-5));
    const double after = curve.sample(at(25));

    EXPECT_DOUBLE_EQ(0.0, before);
    EXPECT_DOUBLE_EQ(0.0, after);
}

TEST(Curve, supportsStepInterpolation)
{
    const Curve curve(StringId{1}, RMS_KIND, {{at(0), 0.25}, {at(10), 0.75}}, RMS_LABEL, CurveInterpolation::STEP,
        std::nullopt, std::nullopt, {});

    EXPECT_DOUBLE_EQ(0.25, curve.sample(at(5)));
    EXPECT_DOUBLE_EQ(0.75, curve.sample(at(10)));
}

TEST(Curve, samplesAtFrameGridBoundaries)
{
    const Curve curve(StringId{1}, RMS_KIND, {{at(0), 0.0}, {at(20), 1.0}}, RMS_LABEL, CurveInterpolation::LINEAR,
        std::nullopt, std::nullopt, {});
    const FrameGrid grid(Timebase(10), 3, 1, 1);

    const std::vector<CurveSample> samples = curve.sample(grid);

    ASSERT_EQ(3, size_cast(samples));
    EXPECT_EQ(0, samples[0].time().ticks());
    EXPECT_DOUBLE_EQ(0.0, samples[0].value());
    EXPECT_EQ(10, samples[1].time().ticks());
    EXPECT_DOUBLE_EQ(0.5, samples[1].value());
    EXPECT_EQ(20, samples[2].time().ticks());
    EXPECT_DOUBLE_EQ(1.0, samples[2].value());
}

TEST(Curve, rejectsInsufficientSamples)
{
    EXPECT_THROW(Curve(StringId{1}, RMS_KIND, {}), std::invalid_argument);
    EXPECT_THROW(Curve(StringId{1}, RMS_KIND, {{at(0), 0.0}}), std::invalid_argument);
}

TEST(Curve, rejectsSamplesWithoutStrictlyIncreasingTimes)
{
    EXPECT_THROW(Curve(StringId{1}, RMS_KIND, {{at(10), 0.0}, {at(0), 1.0}}), std::invalid_argument);
}

TEST(Curve, rejectsNonfiniteSamples)
{
    const double infinity = std::numeric_limits<double>::infinity();

    EXPECT_THROW(Curve(StringId{1}, RMS_KIND, {{at(0), 0.0}, {at(10), infinity}}), std::invalid_argument);
}

TEST(Curve, rejectsInvalidSampleBounds)
{
    EXPECT_THROW(Curve(StringId{1}, RMS_KIND, {{at(0), 0.0}, {at(10), 1.0}}, RMS_LABEL, CurveInterpolation::LINEAR, 1.0,
                     0.0, {}),
        std::invalid_argument);
}

TEST(Curve, rejectsSamplesOutsideBounds)
{
    EXPECT_THROW(Curve(StringId{1}, RMS_KIND, {{at(0), 0.0}, {at(10), 1.5}}, RMS_LABEL, CurveInterpolation::LINEAR, 0.0,
                     1.0, {}),
        std::out_of_range);
}
