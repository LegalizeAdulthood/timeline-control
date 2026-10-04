// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/Event.h>

namespace timeline
{

/// Timeline interval divided into optional attack, sustain, and decay phases.
///
/// An envelope derives its end from the sum of its present nonnegative phase
/// durations and carries the same generic metadata as other timeline items.
///
class Envelope
{
public:
    Envelope(StringId id, std::string kind, Time start);
    Envelope(StringId id, std::string kind, Time start, std::optional<Duration> attack, std::optional<Duration> sustain,
        std::optional<Duration> decay, std::string label, std::optional<double> strength, Attributes attributes);

    /// Return a copy with the supplied identity.
    Envelope with_id(StringId id) const;
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
    Time end() const;
    const std::optional<Duration> &attack() const
    {
        return m_attack;
    }
    const std::optional<Duration> &sustain() const
    {
        return m_sustain;
    }
    const std::optional<Duration> &decay() const
    {
        return m_decay;
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
    std::optional<Duration> m_attack;
    std::optional<Duration> m_sustain;
    std::optional<Duration> m_decay;
    std::string m_label;
    std::optional<double> m_strength;
    Attributes m_attributes;
};

} // namespace timeline
