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
    Interval(StringId id, StringId kind, Time start, Time end);
    Interval(StringId id, StringId kind, Time start, Time end, StringId label, std::optional<double> strength,
        Attributes attributes);

    /// Return a copy with the supplied identity.
    Interval with_id(StringId id) const;
    /// Return a copy with the supplied kind and label identities.
    Interval with_strings(StringId kind, StringId label) const;
    /// Return a copy with the supplied attributes.
    Interval with_attributes(Attributes attributes) const;
    StringId id() const
    {
        return m_id;
    }
    StringId kind() const
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
    StringId label() const
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
    StringId m_kind;
    Time m_start;
    Time m_end;
    StringId m_label;
    std::optional<double> m_strength;
    Attributes m_attributes;
};

} // namespace timeline
