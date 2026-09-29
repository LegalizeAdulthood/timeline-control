// Copyright (c) 2026 Richard Thomson

#include <timeline/Envelope.h>

#include <cmath>
#include <stdexcept>
#include <utility>

namespace timeline
{
namespace
{

Ticks phase_ticks(const std::optional<Duration> &phase)
{
    return phase ? phase->ticks() : 0;
}

} // namespace

Envelope::Envelope(std::string id, std::string kind, Time start, std::optional<Duration> attack,
    std::optional<Duration> sustain, std::optional<Duration> decay, std::string label, std::optional<double> strength,
    Attributes attributes) :
    m_id(std::move(id)),
    m_kind(std::move(kind)),
    m_start(start),
    m_attack(attack),
    m_sustain(sustain),
    m_decay(decay),
    m_label(std::move(label)),
    m_strength(strength),
    m_attributes(std::move(attributes))
{
    if (m_id.empty() || m_kind.empty())
    {
        throw std::invalid_argument("timeline envelopes require an id and kind");
    }
    if (phase_ticks(m_attack) < 0 || phase_ticks(m_sustain) < 0 || phase_ticks(m_decay) < 0)
    {
        throw std::invalid_argument("timeline envelope phases cannot be negative");
    }
    if (phase_ticks(m_attack) + phase_ticks(m_sustain) + phase_ticks(m_decay) == 0)
    {
        throw std::invalid_argument("timeline envelopes require a positive duration");
    }
    if (m_strength && !std::isfinite(*m_strength))
    {
        throw std::invalid_argument("timeline envelope strength must be finite");
    }
}

Time Envelope::end() const
{
    return m_start + Duration::from_ticks(phase_ticks(m_attack) + phase_ticks(m_sustain) + phase_ticks(m_decay));
}

} // namespace timeline
