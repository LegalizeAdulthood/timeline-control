// Copyright (c) 2026 Richard Thomson

#include <timeline/Interval.h>

#include <cmath>
#include <stdexcept>
#include <utility>

namespace timeline
{

Interval::Interval(StringId id, StringId kind, Time start, Time end) :
    Interval(id, kind, start, end, {}, std::nullopt, {})
{
}

Interval::Interval(StringId id, StringId kind, Time start, Time end, StringId label, std::optional<double> strength,
    Attributes attributes) :
    m_id(id),
    m_kind(kind),
    m_start(start),
    m_end(end),
    m_label(label),
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

Interval Interval::with_id(StringId id) const
{
    if (id.empty())
    {
        throw std::invalid_argument("timeline interval identity cannot be empty");
    }
    Interval result(*this);
    result.m_id = id;
    return result;
}

Interval Interval::with_strings(StringId kind, StringId label) const
{
    if (kind.empty())
    {
        throw std::invalid_argument("timeline interval kind cannot be empty");
    }
    Interval result(*this);
    result.m_kind = kind;
    result.m_label = label;
    return result;
}

Interval Interval::with_attributes(Attributes attributes) const
{
    Interval result(*this);
    result.m_attributes = std::move(attributes);
    return result;
}

} // namespace timeline
