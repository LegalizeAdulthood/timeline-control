// Copyright (c) 2026 Richard Thomson

#include <timeline/Interval.h>

#include <cmath>
#include <stdexcept>
#include <utility>

namespace timeline
{

Interval::Interval(std::string id, std::string kind, Time start, Time end) :
    Interval(std::move(id), std::move(kind), start, end, {}, std::nullopt, {})
{
}

Interval::Interval(std::string id, std::string kind, Time start, Time end, std::string label,
    std::optional<double> strength, Attributes attributes) :
    m_id(std::move(id)),
    m_kind(std::move(kind)),
    m_start(start),
    m_end(end),
    m_label(std::move(label)),
    m_strength(strength),
    m_attributes(std::move(attributes))
{
    if (m_id.empty() || m_kind.empty())
    {
        throw std::invalid_argument("timeline intervals require an id and kind");
    }
    if (m_end <= m_start)
    {
        throw std::invalid_argument("timeline intervals require a positive duration");
    }
    if (m_strength && !std::isfinite(*m_strength))
    {
        throw std::invalid_argument("timeline interval strength must be finite");
    }
}

} // namespace timeline
