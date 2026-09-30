// Copyright (c) 2026 Richard Thomson

#include <timeline/Keyframe.h>

#include <cmath>
#include <stdexcept>
#include <utility>

namespace timeline
{

Keyframe::Keyframe(std::string id, Time time, double value) :
    Keyframe(std::move(id), time, value, KeyframeInterpolation::HOLD, {})
{
}

Keyframe::Keyframe(
    std::string id, Time time, double value, KeyframeInterpolation interpolation, Attributes attributes) :
    m_id(std::move(id)),
    m_time(time),
    m_value(value),
    m_interpolation(interpolation),
    m_attributes(std::move(attributes))
{
    if (m_id.empty())
    {
        throw std::invalid_argument("timeline keyframes require an id");
    }
    if (!std::isfinite(m_value))
    {
        throw std::invalid_argument("timeline keyframes require finite values");
    }
    if (m_interpolation == KeyframeInterpolation::GEOMETRIC && m_value <= 0.0)
    {
        throw std::invalid_argument("geometric keyframes require positive values");
    }
}

} // namespace timeline
