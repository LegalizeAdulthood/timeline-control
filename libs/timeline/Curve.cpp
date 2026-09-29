// Copyright (c) 2026 Richard Thomson

#include <timeline/Curve.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace timeline
{

CurveSample::CurveSample(Time time, double value) :
    m_time(time),
    m_value(value)
{
    if (!std::isfinite(m_value))
    {
        throw std::invalid_argument("timeline curve samples require finite values");
    }
}

Curve::Curve(std::string id, std::string kind, std::vector<CurveSample> samples) :
    Curve(std::move(id), std::move(kind), std::move(samples), {}, CurveInterpolation::LINEAR, std::nullopt,
        std::nullopt, {})
{
}

Curve::Curve(std::string id, std::string kind, std::vector<CurveSample> samples, std::string label,
    CurveInterpolation interpolation, std::optional<double> minimum, std::optional<double> maximum,
    Attributes attributes) :
    m_id(std::move(id)),
    m_kind(std::move(kind)),
    m_label(std::move(label)),
    m_interpolation(interpolation),
    m_minimum(minimum),
    m_maximum(maximum),
    m_attributes(std::move(attributes)),
    m_samples(std::move(samples))
{
    if (m_id.empty() || m_kind.empty())
    {
        throw std::invalid_argument("timeline curves require an id and kind");
    }
    if (sample_count() < 2)
    {
        throw std::invalid_argument("timeline curves require at least two samples");
    }
    if ((m_minimum && !std::isfinite(*m_minimum)) || (m_maximum && !std::isfinite(*m_maximum)) ||
        (m_minimum && m_maximum && *m_maximum < *m_minimum))
    {
        throw std::invalid_argument("timeline curve bounds are invalid");
    }

    for (auto index = 0; index < sample_count(); ++index)
    {
        const auto &sample = m_samples[index];
        if (index > 0 && sample.time() <= m_samples[index - 1].time())
        {
            throw std::invalid_argument("timeline curve sample times must be strictly increasing");
        }
        if ((m_minimum && sample.value() < *m_minimum) || (m_maximum && *m_maximum < sample.value()))
        {
            throw std::out_of_range("timeline curve sample is outside its declared bounds");
        }
    }
}

double Curve::sample(Time time) const
{
    if (time <= start())
    {
        return m_samples.front().value();
    }
    if (end() <= time)
    {
        return m_samples.back().value();
    }

    const auto right = std::lower_bound(m_samples.begin(), m_samples.end(), time,
        [](const CurveSample &sample, Time value) { return sample.time() < value; });
    if (right->time() == time)
    {
        return right->value();
    }
    const auto &left = *(right - 1);
    if (m_interpolation == CurveInterpolation::STEP)
    {
        return left.value();
    }

    const auto elapsed = static_cast<double>((time - left.time()).ticks());
    const auto duration = static_cast<double>((right->time() - left.time()).ticks());
    return left.value() + (right->value() - left.value()) * elapsed / duration;
}

std::vector<CurveSample> Curve::sample(const FrameGrid &frame_grid) const
{
    auto result = std::vector<CurveSample>{};
    for (Ticks frame = 0; frame < frame_grid.frame_count(); ++frame)
    {
        const auto time = frame_grid.frame_start(frame);
        result.emplace_back(time, sample(time));
    }
    return result;
}

} // namespace timeline
