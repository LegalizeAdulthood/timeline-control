// Copyright (c) 2026 Richard Thomson

#include <timeline/Curve.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace timeline
{
namespace
{

void validate_curve_metadata(
    StringId id, StringId kind, const std::optional<double> &minimum, const std::optional<double> &maximum)
{
    if (id.empty() || kind.empty())
    {
        throw std::invalid_argument("timeline curves require an id and kind");
    }
    if ((minimum && !std::isfinite(*minimum)) || (maximum && !std::isfinite(*maximum)) ||
        (minimum && maximum && *maximum < *minimum))
    {
        throw std::invalid_argument("timeline curve bounds are invalid");
    }
}

void validate_curve_value(double value, const std::optional<double> &minimum, const std::optional<double> &maximum)
{
    if (!std::isfinite(value))
    {
        throw std::invalid_argument("timeline curve values must be finite");
    }
    if ((minimum && value < *minimum) || (maximum && *maximum < value))
    {
        throw std::out_of_range("timeline curve sample is outside its declared bounds");
    }
}

} // namespace

std::string_view to_string(CurveInterpolation value)
{
    switch (value)
    {
    case CurveInterpolation::LINEAR:
        return "linear";
    case CurveInterpolation::STEP:
        return "step";
    case CurveInterpolation::ANALYTIC:
        return "analytic";
    }
    throw std::invalid_argument("unknown curve interpolation policy");
}

CurveSample::CurveSample(Time time, double value) :
    m_time(time),
    m_value(value)
{
    if (!std::isfinite(m_value))
    {
        throw std::invalid_argument("timeline curve samples require finite values");
    }
}

Curve::Curve(StringId id, StringId kind, std::vector<CurveSample> samples) :
    Curve(id, kind, std::move(samples), {}, CurveInterpolation::LINEAR, std::nullopt, std::nullopt, {})
{
}

Curve::Curve(StringId id, StringId kind, std::vector<CurveSample> samples, StringId label,
    CurveInterpolation interpolation, std::optional<double> minimum, std::optional<double> maximum,
    Attributes attributes) :
    m_id(id),
    m_kind(kind),
    m_label(label),
    m_interpolation(interpolation),
    m_minimum(minimum),
    m_maximum(maximum),
    m_attributes(std::move(attributes)),
    m_samples(std::move(samples))
{
    validate_curve_metadata(m_id, m_kind, m_minimum, m_maximum);
    if (sample_count() < 2)
    {
        throw std::invalid_argument("timeline curves require at least two samples");
    }
    if (m_interpolation != CurveInterpolation::LINEAR && m_interpolation != CurveInterpolation::STEP)
    {
        throw std::invalid_argument("sample-defined curves require linear or step interpolation");
    }

    for (int index = 0; index < sample_count(); ++index)
    {
        const CurveSample &sample = m_samples[index];
        if (index > 0 && sample.time() <= m_samples[index - 1].time())
        {
            throw std::invalid_argument("timeline curve sample times must be strictly increasing");
        }
        validate_curve_value(sample.value(), m_minimum, m_maximum);
    }
    m_start = m_samples.front().time();
    m_end = m_samples.back().time();
}

Curve::Curve(StringId id, StringId kind, Time start, Time end, CurveEvaluator evaluator) :
    Curve(id, kind, start, end, std::move(evaluator), {}, std::nullopt, std::nullopt, {})
{
}

Curve::Curve(StringId id, StringId kind, Time start, Time end, CurveEvaluator evaluator, StringId label,
    std::optional<double> minimum, std::optional<double> maximum, Attributes attributes) :
    m_id(id),
    m_kind(kind),
    m_label(label),
    m_interpolation(CurveInterpolation::ANALYTIC),
    m_minimum(minimum),
    m_maximum(maximum),
    m_attributes(std::move(attributes)),
    m_start(start),
    m_end(end),
    m_evaluator(std::move(evaluator))
{
    validate_curve_metadata(m_id, m_kind, m_minimum, m_maximum);
    if (!m_evaluator || m_end <= m_start)
    {
        throw std::invalid_argument("analytic curves require an evaluator and a positive time domain");
    }
    static_cast<void>(sample(start));
    static_cast<void>(sample(end));
}

Curve Curve::with_id(StringId id) const
{
    if (id.empty())
    {
        throw std::invalid_argument("timeline curve identity cannot be empty");
    }
    Curve result(*this);
    result.m_id = id;
    return result;
}

Curve Curve::with_strings(StringId kind, StringId label) const
{
    if (kind.empty())
    {
        throw std::invalid_argument("timeline curve kind cannot be empty");
    }
    Curve result(*this);
    result.m_kind = kind;
    result.m_label = label;
    return result;
}

double Curve::sample(Time time) const
{
    if (m_evaluator)
    {
        const double value = m_evaluator(std::clamp(time, start(), end()));
        validate_curve_value(value, m_minimum, m_maximum);
        return value;
    }
    if (time <= start())
    {
        return m_samples.front().value();
    }
    if (end() <= time)
    {
        return m_samples.back().value();
    }

    const std::vector<CurveSample>::const_iterator right = std::lower_bound(m_samples.begin(), m_samples.end(), time,
        [](const CurveSample &sample, Time value) { return sample.time() < value; });
    if (right->time() == time)
    {
        return right->value();
    }
    const CurveSample &left = *(right - 1);
    if (m_interpolation == CurveInterpolation::STEP)
    {
        return left.value();
    }

    const double elapsed = static_cast<double>((time - left.time()).ticks());
    const double duration = static_cast<double>((right->time() - left.time()).ticks());
    return left.value() + (right->value() - left.value()) * elapsed / duration;
}

std::vector<CurveSample> Curve::sample(const FrameGrid &frame_grid) const
{
    std::vector<CurveSample> result{};
    for (Ticks frame = 0; frame < frame_grid.frame_count(); ++frame)
    {
        const Time time = frame_grid.frame_start(frame);
        result.emplace_back(time, sample(time));
    }
    return result;
}

} // namespace timeline
