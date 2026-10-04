// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/Event.h>

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace timeline
{

/// Policy for evaluating the segment after a keyframe.
///
enum class KeyframeInterpolation
{
    HOLD,
    LINEAR,
    /// Interpolate positive endpoint values in logarithmic space.
    GEOMETRIC
};

/// Returns the stable name of a keyframe interpolation policy.
std::string_view to_string(KeyframeInterpolation value);

/// Numeric authored value at one exact timeline time.
///
/// A keyframe carries stable identity, a finite numeric value, the
/// interpolation policy for its outgoing segment, and application-defined
/// attributes.
///
class Keyframe
{
public:
    Keyframe(StringId id, Time time, double value);
    Keyframe(StringId id, Time time, double value, KeyframeInterpolation interpolation, Attributes attributes);

    StringId id() const
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
    StringId m_id;
    Time m_time;
    double m_value;
    KeyframeInterpolation m_interpolation;
    Attributes m_attributes;

    friend class DocumentBuilder;
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
