#include <timeline/Time.h>

#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace timeline
{
namespace
{

Ticks checked_multiply(Ticks lhs, Ticks rhs)
{
    if (lhs > 0 && rhs > 0 && lhs > std::numeric_limits<Ticks>::max() / rhs)
    {
        throw std::overflow_error("timeline time conversion overflow");
    }
    if (lhs < 0 && rhs > 0 && lhs < std::numeric_limits<Ticks>::min() / rhs)
    {
        throw std::overflow_error("timeline time conversion overflow");
    }

    return lhs * rhs;
}

Ticks round_ratio(Ticks numerator, Ticks denominator, TimeRounding rounding)
{
    auto whole = numerator / denominator;
    auto remainder = numerator % denominator;

    if (remainder == 0)
    {
        return whole;
    }

    switch (rounding)
    {
    case TimeRounding::FLOOR:
        return numerator < 0 ? whole - 1 : whole;
    case TimeRounding::CEIL:
        return numerator < 0 ? whole : whole + 1;
    case TimeRounding::NEAREST:
        if (std::abs(remainder) * 2 >= denominator)
        {
            return numerator < 0 ? whole - 1 : whole + 1;
        }
        return whole;
    }

    throw std::invalid_argument("unknown timeline rounding mode");
}

Duration exact_duration(const Timebase &timebase, Ticks numerator, Ticks denominator)
{
    const auto floor_duration = timebase.duration_from_seconds_ratio(numerator, denominator, TimeRounding::FLOOR);
    const auto ceil_duration = timebase.duration_from_seconds_ratio(numerator, denominator, TimeRounding::CEIL);

    if (floor_duration != ceil_duration)
    {
        throw std::invalid_argument("ratio cannot be represented exactly in timeline ticks");
    }

    return floor_duration;
}

Duration validate_frame_grid_arguments(
    const Timebase &timebase, Ticks frame_count, Ticks frames_per_second_numerator, Ticks frames_per_second_denominator)
{
    if (frame_count < 0)
    {
        throw std::invalid_argument("timeline frame count cannot be negative");
    }
    if (frames_per_second_numerator <= 0)
    {
        throw std::invalid_argument("timeline frames per second numerator must be positive");
    }
    if (frames_per_second_denominator <= 0)
    {
        throw std::invalid_argument("timeline frames per second denominator must be positive");
    }

    return exact_duration(timebase, frames_per_second_denominator, frames_per_second_numerator);
}

} // namespace

Timebase::Timebase(Ticks ticks_per_second) :
    m_ticks_per_second(ticks_per_second)
{
    if (ticks_per_second <= 0)
    {
        throw std::invalid_argument("timeline ticks per second must be positive");
    }
}

Duration Timebase::duration_from_seconds_ratio(Ticks numerator, Ticks denominator, TimeRounding rounding) const
{
    if (denominator <= 0)
    {
        throw std::invalid_argument("timeline ratio denominator must be positive");
    }

    const auto common = std::gcd(m_ticks_per_second, denominator);
    const auto ticks_factor = m_ticks_per_second / common;
    const auto reduced_denominator = denominator / common;
    const auto scaled_numerator = checked_multiply(numerator, ticks_factor);

    return Duration::from_ticks(round_ratio(scaled_numerator, reduced_denominator, rounding));
}

Duration Timebase::duration_from_seconds(double seconds, TimeRounding rounding) const
{
    const auto ticks = seconds * static_cast<double>(m_ticks_per_second);

    switch (rounding)
    {
    case TimeRounding::FLOOR:
        return Duration::from_ticks(static_cast<Ticks>(std::floor(ticks)));
    case TimeRounding::NEAREST:
        return Duration::from_ticks(static_cast<Ticks>(std::round(ticks)));
    case TimeRounding::CEIL:
        return Duration::from_ticks(static_cast<Ticks>(std::ceil(ticks)));
    }

    throw std::invalid_argument("unknown timeline rounding mode");
}

FrameGrid::FrameGrid(
    Timebase timebase, Ticks frame_count, Ticks frames_per_second_numerator, Ticks frames_per_second_denominator) :
    FrameGrid(timebase, frame_count, frames_per_second_numerator, frames_per_second_denominator, Time{})
{
}

FrameGrid::FrameGrid(Timebase timebase, Ticks frame_count, Ticks frames_per_second_numerator,
    Ticks frames_per_second_denominator, Time offset) :
    m_timebase(timebase),
    m_frame_count(frame_count),
    m_frames_per_second_numerator(frames_per_second_numerator),
    m_frames_per_second_denominator(frames_per_second_denominator),
    m_frame_duration(validate_frame_grid_arguments(
        timebase, frame_count, frames_per_second_numerator, frames_per_second_denominator)),
    m_offset(offset)
{
}

Duration FrameGrid::duration() const
{
    return Duration::from_ticks(checked_multiply(m_frame_duration.ticks(), m_frame_count));
}

Time FrameGrid::frame_start(Ticks frame_index) const
{
    if (frame_index < 0 || frame_index >= m_frame_count)
    {
        throw std::out_of_range("timeline frame index is out of range");
    }

    return m_offset + Duration::from_ticks(checked_multiply(m_frame_duration.ticks(), frame_index));
}

std::optional<Ticks> FrameGrid::frame_at_or_before(Time time) const
{
    if (m_frame_count == 0 || time < m_offset)
    {
        return std::nullopt;
    }

    const auto elapsed = time - m_offset;
    const auto frame_index = elapsed.ticks() / m_frame_duration.ticks();

    if (frame_index >= m_frame_count)
    {
        return m_frame_count - 1;
    }

    return frame_index;
}

std::optional<Ticks> FrameGrid::nearest_frame(Time time) const
{
    if (m_frame_count == 0)
    {
        return std::nullopt;
    }
    if (time <= m_offset)
    {
        return 0;
    }

    const auto elapsed = time - m_offset;
    const auto frame_index = (elapsed.ticks() + m_frame_duration.ticks() / 2) / m_frame_duration.ticks();

    if (frame_index >= m_frame_count)
    {
        return m_frame_count - 1;
    }

    return frame_index;
}

} // namespace timeline
