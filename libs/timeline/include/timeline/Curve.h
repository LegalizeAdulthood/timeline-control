// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/Event.h>
#include <timeline/size_cast.h>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace timeline
{

/// Policy for evaluating a curve from samples or an analytic definition.
///
enum class CurveInterpolation
{
    LINEAR,
    STEP,
    ANALYTIC
};

/// Returns the stable name of a curve interpolation policy.
std::string_view to_string(CurveInterpolation value);

/// Owned callable that evaluates a numeric curve at an exact timeline time.
/// Captured recipe data must be owned by value and evaluation must be pure.
///
using CurveEvaluator = std::function<double(Time)>;

/// One numeric curve value stored at an exact timeline time.
///
/// Curve samples preserve their exact time coordinate and require a finite
/// numeric value.
///
class CurveSample
{
public:
    CurveSample(Time time, double value);

    Time time() const
    {
        return m_time;
    }
    double value() const
    {
        return m_value;
    }

private:
    Time m_time;
    double m_value;
};

/// Numeric curve defined by ordered samples or an owned analytic evaluator.
///
/// Sample-defined curves own at least two strictly ordered samples. Analytic
/// curves own an evaluator and a finite time domain, without storing sampled
/// caches. Both clamp queries to their domain. Bounds reject stored or evaluated
/// values outside the declared range; evaluated values must be finite.
///
class Curve
{
public:
    Curve(StringId id, std::string kind, std::vector<CurveSample> samples);
    Curve(StringId id, std::string kind, std::vector<CurveSample> samples, std::string label,
        CurveInterpolation interpolation, std::optional<double> minimum, std::optional<double> maximum,
        Attributes attributes);
    Curve(StringId id, std::string kind, Time start, Time end, CurveEvaluator evaluator);
    Curve(StringId id, std::string kind, Time start, Time end, CurveEvaluator evaluator, std::string label,
        std::optional<double> minimum, std::optional<double> maximum, Attributes attributes);

    /// Return a copy with the supplied identity.
    Curve with_id(StringId id) const;
    StringId id() const
    {
        return m_id;
    }
    const std::string &kind() const
    {
        return m_kind;
    }
    const std::string &label() const
    {
        return m_label;
    }
    CurveInterpolation interpolation() const
    {
        return m_interpolation;
    }
    const std::optional<double> &minimum() const
    {
        return m_minimum;
    }
    const std::optional<double> &maximum() const
    {
        return m_maximum;
    }
    const Attributes &attributes() const
    {
        return m_attributes;
    }
    const std::vector<CurveSample> &samples() const
    {
        return m_samples;
    }
    int sample_count() const
    {
        return size_cast(m_samples);
    }
    Time start() const
    {
        return m_start;
    }
    Time end() const
    {
        return m_end;
    }

    double sample(Time time) const;
    std::vector<CurveSample> sample(const FrameGrid &frame_grid) const;

private:
    StringId m_id;
    std::string m_kind;
    std::string m_label;
    CurveInterpolation m_interpolation;
    std::optional<double> m_minimum;
    std::optional<double> m_maximum;
    Attributes m_attributes;
    std::vector<CurveSample> m_samples;
    Time m_start;
    Time m_end;
    CurveEvaluator m_evaluator;
};

} // namespace timeline
