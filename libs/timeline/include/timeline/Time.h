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
/// A duration has no origin until it is combined with a TimelineTime. Its value
/// is an exact count of ticks in the associated timebase.
///
class TimelineDuration
{
public:
    constexpr TimelineDuration() = default;

    static TimelineDuration from_ticks(Ticks ticks)
    {
        return TimelineDuration(ticks);
    }

    Ticks ticks() const
    {
        return m_ticks;
    }

private:
    explicit TimelineDuration(Ticks ticks) :
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
class TimelineTime
{
public:
    constexpr TimelineTime() = default;

    static TimelineTime from_ticks(Ticks ticks)
    {
        return TimelineTime(ticks);
    }

    Ticks ticks() const
    {
        return m_ticks;
    }

private:
    explicit TimelineTime(Ticks ticks) :
        m_ticks(ticks)
    {
    }

    Ticks m_ticks{0};
};

inline bool operator==(TimelineDuration lhs, TimelineDuration rhs)
{
    return lhs.ticks() == rhs.ticks();
}
inline bool operator!=(TimelineDuration lhs, TimelineDuration rhs)
{
    return !(lhs == rhs);
}
inline bool operator==(TimelineTime lhs, TimelineTime rhs)
{
    return lhs.ticks() == rhs.ticks();
}
inline bool operator!=(TimelineTime lhs, TimelineTime rhs)
{
    return !(lhs == rhs);
}
inline bool operator<(TimelineTime lhs, TimelineTime rhs)
{
    return lhs.ticks() < rhs.ticks();
}
inline bool operator<=(TimelineTime lhs, TimelineTime rhs)
{
    return lhs.ticks() <= rhs.ticks();
}
inline TimelineTime operator+(TimelineTime lhs, TimelineDuration rhs)
{
    return TimelineTime::from_ticks(lhs.ticks() + rhs.ticks());
}
inline TimelineDuration operator-(TimelineTime lhs, TimelineTime rhs)
{
    return TimelineDuration::from_ticks(lhs.ticks() - rhs.ticks());
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
    TimelineDuration duration_from_seconds_ratio(Ticks numerator, Ticks denominator, TimeRounding rounding) const;
    TimelineTime time_from_seconds_ratio(Ticks numerator, Ticks denominator, TimeRounding rounding) const
    {
        return TimelineTime::from_ticks(duration_from_seconds_ratio(numerator, denominator, rounding).ticks());
    }
    TimelineDuration duration_from_seconds(double seconds, TimeRounding rounding) const;
    TimelineTime time_from_seconds(double seconds, TimeRounding rounding) const
    {
        return TimelineTime::from_ticks(duration_from_seconds(seconds, rounding).ticks());
    }
    double seconds(TimelineDuration duration) const
    {
        return static_cast<double>(duration.ticks()) / static_cast<double>(m_ticks_per_second);
    }
    double seconds(TimelineTime time) const
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
    FrameGrid(Timebase timebase, Ticks frame_count, Ticks frames_per_second_numerator,
        Ticks frames_per_second_denominator, TimelineTime offset = TimelineTime{});

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
    TimelineDuration frame_duration() const
    {
        return m_frame_duration;
    }
    TimelineTime offset() const
    {
        return m_offset;
    }
    TimelineDuration duration() const;
    TimelineTime end_time() const
    {
        return m_offset + duration();
    }
    TimelineTime frame_start(Ticks frame_index) const;
    std::optional<Ticks> frame_at_or_before(TimelineTime time) const;
    std::optional<Ticks> nearest_frame(TimelineTime time) const;

private:
    Timebase m_timebase;
    Ticks m_frame_count;
    Ticks m_frames_per_second_numerator;
    Ticks m_frames_per_second_denominator;
    TimelineDuration m_frame_duration;
    TimelineTime m_offset;
};

} // namespace timeline
