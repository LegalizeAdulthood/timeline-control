// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/Event.h>

#include <functional>
#include <optional>
#include <string>

namespace timeline
{

/// Policy for evaluating the segment after a keyframe.
enum class KeyframeInterpolation
{
    HOLD,
    LINEAR
};

/// Numeric authored value at one exact timeline time.
///
/// A keyframe carries stable identity, a finite numeric value, the
/// interpolation policy for its outgoing segment, and application-defined
/// attributes.
///
class Keyframe
{
public:
    Keyframe(std::string id, Time time, double value);
    Keyframe(std::string id, Time time, double value, KeyframeInterpolation interpolation, Attributes attributes);

    const std::string &id() const
    {
        return m_id;
    }
    Time time() const
    {
        return m_time;
    }
    double value() const
    {
        return m_value;
    }
    KeyframeInterpolation interpolation() const
    {
        return m_interpolation;
    }
    const Attributes &attributes() const
    {
        return m_attributes;
    }

private:
    std::string m_id;
    Time m_time;
    double m_value;
    KeyframeInterpolation m_interpolation;
    Attributes m_attributes;
};

/// Keyframes immediately surrounding a timeline time.
///
/// The before and after references include an exact keyframe at the query
/// time. References remain valid until their lane is modified.
///
class KeyframeNeighbors
{
public:
    using Reference = std::reference_wrapper<const Keyframe>;

    KeyframeNeighbors(std::optional<Reference> before, std::optional<Reference> after) :
        m_before(before),
        m_after(after)
    {
    }

    const std::optional<Reference> &before() const
    {
        return m_before;
    }
    const std::optional<Reference> &after() const
    {
        return m_after;
    }

private:
    std::optional<Reference> m_before;
    std::optional<Reference> m_after;
};

} // namespace timeline
