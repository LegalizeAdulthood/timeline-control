// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/Event.h>
#include <timeline/size_cast.h>

#include <optional>
#include <string>
#include <vector>

namespace timeline
{

/// Policy for evaluating a curve between exact samples.
enum class CurveInterpolation
{
    LINEAR,
    STEP
};

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

/// Analytic numeric curve defined by ordered exact-time samples.
///
/// A curve owns at least two strictly ordered samples, evaluates according to
/// its interpolation policy, and clamps queries outside its sample domain to
/// the nearest endpoint. Optional bounds are metadata and reject samples that
/// lie outside the declared range.
///
class Curve
{
public:
    Curve(std::string id, std::string kind, std::vector<CurveSample> samples);
    Curve(std::string id, std::string kind, std::vector<CurveSample> samples, std::string label,
        CurveInterpolation interpolation, std::optional<double> minimum, std::optional<double> maximum,
        Attributes attributes);

    const std::string &id() const
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
        return m_samples.front().time();
    }
    Time end() const
    {
        return m_samples.back().time();
    }

    double sample(Time time) const;
    std::vector<CurveSample> sample(const FrameGrid &frame_grid) const;

private:
    std::string m_id;
    std::string m_kind;
    std::string m_label;
    CurveInterpolation m_interpolation;
    std::optional<double> m_minimum;
    std::optional<double> m_maximum;
    Attributes m_attributes;
    std::vector<CurveSample> m_samples;
};

} // namespace timeline
