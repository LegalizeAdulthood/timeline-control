// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/Event.h>

namespace timeline
{

/// Generic timeline content spanning a nonempty time interval.
///
/// An interval owns exact start and end times together with stable identity,
/// descriptive metadata, optional strength, and application attributes.
///
class Interval
{
public:
    Interval(StringId id, std::string kind, Time start, Time end);
    Interval(StringId id, std::string kind, Time start, Time end, std::string label, std::optional<double> strength,
        Attributes attributes);

    StringId id() const
    {
        return m_id;
    }
    const std::string &kind() const
    {
        return m_kind;
    }
    Time start() const
    {
        return m_start;
    }
    Time end() const
    {
        return m_end;
    }
    Duration duration() const
    {
        return m_end - m_start;
    }
    const std::string &label() const
    {
        return m_label;
    }
    const std::optional<double> &strength() const
    {
        return m_strength;
    }
    const Attributes &attributes() const
    {
        return m_attributes;
    }

private:
    StringId m_id;
    std::string m_kind;
    Time m_start;
    Time m_end;
    std::string m_label;
    std::optional<double> m_strength;
    Attributes m_attributes;

    friend class DocumentBuilder;
};

} // namespace timeline
