#pragma once

#include <cstdint>
#include <optional>

namespace timeline
{

/// Exact integer storage unit for timeline coordinates and durations.
using Ticks = std::int64_t;

/// Rounding policy for converting fractional time values to integral ticks.
enum class TimeRounding
{
    /// Round toward negative infinity.
    FLOOR,
    /// Round to the nearest tick.
    NEAREST,
    /// Round toward positive infinity.
    CEIL
};

/// Signed elapsed interval on a timeline.
///
/// A duration has no origin until it is combined with a Time. Its value
/// is an exact count of ticks in the associated timebase.
///
class Duration
{
public:
    constexpr Duration() = default;

    static Duration from_ticks(Ticks ticks)
    {
        return Duration(ticks);
    }

    Ticks ticks() const
    {
        return m_ticks;
    }

private:
    explicit Duration(Ticks ticks) :
        m_ticks(ticks)
    {
    }

    Ticks m_ticks{0};
};

/// Absolute position on a timeline.
///
/// A timeline time is an exact tick coordinate. Applications choose the meaning
/// of zero by placing content relative to a timebase and optional offsets.
///
class Time
{
public:
    constexpr Time() = default;

    static Time from_ticks(Ticks ticks)
    {
        return Time(ticks);
    }

    Ticks ticks() const
    {
        return m_ticks;
    }

private:
    explicit Time(Ticks ticks) :
        m_ticks(ticks)
    {
    }

    Ticks m_ticks{0};
};

inline bool operator==(Duration lhs, Duration rhs)
{
    return lhs.ticks() == rhs.ticks();
}
inline bool operator!=(Duration lhs, Duration rhs)
{
    return !(lhs == rhs);
}
inline bool operator==(Time lhs, Time rhs)
{
    return lhs.ticks() == rhs.ticks();
}
inline bool operator!=(Time lhs, Time rhs)
{
    return !(lhs == rhs);
}
inline bool operator<(Time lhs, Time rhs)
{
    return lhs.ticks() < rhs.ticks();
}
inline bool operator<=(Time lhs, Time rhs)
{
    return lhs.ticks() <= rhs.ticks();
}
inline Time operator+(Time lhs, Duration rhs)
{
    return Time::from_ticks(lhs.ticks() + rhs.ticks());
}
inline Duration operator-(Time lhs, Time rhs)
{
    return Duration::from_ticks(lhs.ticks() - rhs.ticks());
}

/// Tick rate that maps exact timeline ticks to seconds.
///
/// A timebase defines how many ticks represent one second and centralizes
/// conversions between exact ticks and second-based facades.
///
class Timebase
{
public:
    explicit Timebase(Ticks ticks_per_second);

    Ticks ticks_per_second() const
    {
        return m_ticks_per_second;
    }
    Duration duration_from_seconds_ratio(Ticks numerator, Ticks denominator, TimeRounding rounding) const;
    Time time_from_seconds_ratio(Ticks numerator, Ticks denominator, TimeRounding rounding) const
    {
        return Time::from_ticks(duration_from_seconds_ratio(numerator, denominator, rounding).ticks());
    }
    Duration duration_from_seconds(double seconds, TimeRounding rounding) const;
    Time time_from_seconds(double seconds, TimeRounding rounding) const
    {
        return Time::from_ticks(duration_from_seconds(seconds, rounding).ticks());
    }
    double seconds(Duration duration) const
    {
        return static_cast<double>(duration.ticks()) / static_cast<double>(m_ticks_per_second);
    }
    double seconds(Time time) const
    {
        return static_cast<double>(time.ticks()) / static_cast<double>(m_ticks_per_second);
    }

private:
    Ticks m_ticks_per_second;
};

/// Finite frame sequence projected onto exact timeline coordinates.
///
/// A frame grid defines frame count, frame rate, exact frame duration, and an
/// optional timeline offset. It maps frame indices to times and times back to
/// frame indices without making frames the timeline's fundamental unit.
///
class FrameGrid
{
public:
    FrameGrid(
        Timebase timebase, Ticks frame_count, Ticks frames_per_second_numerator, Ticks frames_per_second_denominator);
    FrameGrid(Timebase timebase, Ticks frame_count, Ticks frames_per_second_numerator,
        Ticks frames_per_second_denominator, Time offset);

    const Timebase &timebase() const
    {
        return m_timebase;
    }
    Ticks frame_count() const
    {
        return m_frame_count;
    }
    Ticks frames_per_second_numerator() const
    {
        return m_frames_per_second_numerator;
    }
    Ticks frames_per_second_denominator() const
    {
        return m_frames_per_second_denominator;
    }
    Duration frame_duration() const
    {
        return m_frame_duration;
    }
    Time offset() const
    {
        return m_offset;
    }
    Duration duration() const;
    Time end_time() const
    {
        return m_offset + duration();
    }
    Time frame_start(Ticks frame_index) const;
    std::optional<Ticks> frame_at_or_before(Time time) const;
    std::optional<Ticks> nearest_frame(Time time) const;

private:
    Timebase m_timebase;
    Ticks m_frame_count;
    Ticks m_frames_per_second_numerator;
    Ticks m_frames_per_second_denominator;
    Duration m_frame_duration;
    Time m_offset;
};

} // namespace timeline
