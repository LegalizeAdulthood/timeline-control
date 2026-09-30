// Copyright (c) 2026 Richard Thomson

#include <timeline/Curve.h>

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

using namespace timeline;

namespace
{

Time at(Ticks ticks)
{
    return Time::from_ticks(ticks);
}

} // namespace

TEST(Curve, owns_an_analytic_definition_and_samples_without_a_cache)
{
    const Curve curve = []
    {
        const double scale = 0.01;
        return Curve("analytic", "signal", at(0), at(20),
            [scale](Time time) { return scale * static_cast<double>(time.ticks() * time.ticks()); });
    }();
    EXPECT_EQ(CurveInterpolation::ANALYTIC, curve.interpolation());
    EXPECT_EQ(0, curve.sample_count());
    EXPECT_EQ(at(0), curve.start());
    EXPECT_EQ(at(20), curve.end());
    EXPECT_DOUBLE_EQ(0.0, curve.sample(at(-1)));
    EXPECT_DOUBLE_EQ(0.25, curve.sample(at(5)));
    EXPECT_DOUBLE_EQ(4.0, curve.sample(at(21)));
    const Curve copy = curve;
    const std::vector<CurveSample> samples = copy.sample(FrameGrid(Timebase(10), 3, 1, 1));
    ASSERT_EQ(3, size_cast(samples));
    EXPECT_DOUBLE_EQ(1.0, samples[1].value());
    EXPECT_EQ(0, copy.sample_count());
}

TEST(Curve, rejects_invalid_analytic_definitions_and_evaluated_values)
{
    const auto identity = [](Time time)
    {
        return static_cast<double>(time.ticks());
    };
    EXPECT_THROW(Curve("", "signal", at(0), at(20), identity), std::invalid_argument);
    EXPECT_THROW(Curve("analytic", "signal", at(0), at(0), identity), std::invalid_argument);
    EXPECT_THROW(Curve("analytic", "signal", at(20), at(0), identity), std::invalid_argument);
    EXPECT_THROW(Curve("analytic", "signal", at(0), at(20), CurveEvaluator{}), std::invalid_argument);
    EXPECT_THROW(Curve("analytic", "signal", at(0), at(20), identity, "", 20.0, 0.0, {}), std::invalid_argument);
    EXPECT_THROW(Curve("analytic", "signal", at(0), at(20), identity, "", 0.0, 1.0, {}), std::out_of_range);
    const auto invalid_interior = [](Time time)
    {
        return time == at(10) ? std::numeric_limits<double>::infinity() : 0.0;
    };
    const Curve invalid("analytic", "signal", at(0), at(20), invalid_interior);
    EXPECT_THROW(invalid.sample(at(10)), std::invalid_argument);
    const auto outside_bounds = [](Time time)
    {
        return time == at(10) ? 2.0 : 0.0;
    };
    const Curve bounded("analytic", "signal", at(0), at(20), outside_bounds, "", 0.0, 1.0, {});
    EXPECT_THROW(bounded.sample(at(10)), std::out_of_range);
}

TEST(Curve, stores_exact_samples_and_interpolates)
{
    const Curve curve(
        "rms", "rms", {{at(0), 0.0}, {at(10), 1.0}, {at(20), 0.0}}, "RMS", CurveInterpolation::LINEAR, 0.0, 1.0, {});

    ASSERT_EQ(3, curve.sample_count());
    EXPECT_EQ(10, curve.samples()[1].time().ticks());
    EXPECT_DOUBLE_EQ(1.0, curve.sample(at(10)));
    EXPECT_DOUBLE_EQ(0.5, curve.sample(at(5)));
    EXPECT_DOUBLE_EQ(0.0, curve.sample(at(-5)));
    EXPECT_DOUBLE_EQ(0.0, curve.sample(at(25)));
}

TEST(Curve, supports_step_interpolation)
{
    const Curve curve(
        "rms", "rms", {{at(0), 0.25}, {at(10), 0.75}}, "RMS", CurveInterpolation::STEP, std::nullopt, std::nullopt, {});

    EXPECT_DOUBLE_EQ(0.25, curve.sample(at(5)));
    EXPECT_DOUBLE_EQ(0.75, curve.sample(at(10)));
}

TEST(Curve, samples_at_frame_grid_boundaries)
{
    const Curve curve(
        "rms", "rms", {{at(0), 0.0}, {at(20), 1.0}}, "RMS", CurveInterpolation::LINEAR, std::nullopt, std::nullopt, {});
    const FrameGrid grid(Timebase(10), 3, 1, 1);

    const std::vector<CurveSample> samples = curve.sample(grid);

    ASSERT_EQ(3U, samples.size());
    EXPECT_EQ(0, samples[0].time().ticks());
    EXPECT_DOUBLE_EQ(0.0, samples[0].value());
    EXPECT_EQ(10, samples[1].time().ticks());
    EXPECT_DOUBLE_EQ(0.5, samples[1].value());
    EXPECT_EQ(20, samples[2].time().ticks());
    EXPECT_DOUBLE_EQ(1.0, samples[2].value());
}

TEST(Curve, rejects_invalid_samples_and_bounds)
{
    const auto infinity = std::numeric_limits<double>::infinity();

    EXPECT_THROW(Curve("rms", "rms", {}), std::invalid_argument);
    EXPECT_THROW(Curve("rms", "rms", {{at(0), 0.0}}), std::invalid_argument);
    EXPECT_THROW(Curve("rms", "rms", {{at(10), 0.0}, {at(0), 1.0}}), std::invalid_argument);
    EXPECT_THROW(Curve("rms", "rms", {{at(0), 0.0}, {at(10), infinity}}), std::invalid_argument);
    EXPECT_THROW(Curve("rms", "rms", {{at(0), 0.0}, {at(10), 1.0}}, "RMS", CurveInterpolation::LINEAR, 1.0, 0.0, {}),
        std::invalid_argument);
    EXPECT_THROW(Curve("rms", "rms", {{at(0), 0.0}, {at(10), 1.5}}, "RMS", CurveInterpolation::LINEAR, 0.0, 1.0, {}),
        std::out_of_range);
}
