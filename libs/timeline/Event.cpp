// Copyright (c) 2026 Richard Thomson

#include <timeline/Event.h>

#include <cmath>
#include <stdexcept>
#include <utility>

namespace timeline
{

Instant::Instant(StringId id, std::string kind, Time time) :
    Instant(id, std::move(kind), time, {}, std::nullopt, {})
{
}

Instant::Instant(StringId id, std::string kind, Time time, std::string label, std::optional<double> strength,
    Attributes attributes) :
    m_id(id),
    m_kind(std::move(kind)),
    m_time(time),
    m_label(std::move(label)),
    m_strength(strength),
    m_attributes(std::move(attributes))
{
    if (m_id.empty() || m_kind.empty())
    {
        throw std::invalid_argument("timeline instants require an id and kind");
    }
    if (m_strength && !std::isfinite(*m_strength))
    {
        throw std::invalid_argument("timeline instant strength must be finite");
    }
}

Instant Instant::with_id(StringId id) const
{
    if (id.empty())
    {
        throw std::invalid_argument("timeline instant identity cannot be empty");
    }
    Instant result(*this);
    result.m_id = id;
    return result;
}

} // namespace timeline
