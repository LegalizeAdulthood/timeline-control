// Copyright (c) 2026 Richard Thomson

#include <timeline/Attributes.h>
#include <timeline/size_cast.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace timeline
{

Attributes::Attributes(std::vector<Attribute> values) :
    m_values(std::move(values))
{
    std::sort(
        m_values.begin(), m_values.end(), [](Attribute left, Attribute right) { return left.key() < right.key(); });
    for (int index = 0; index < size_cast(m_values); ++index)
    {
        if (m_values[index].key().empty())
        {
            throw std::invalid_argument("timeline attribute key cannot be empty");
        }
        if (index > 0 && m_values[index - 1].key() == m_values[index].key())
        {
            throw std::invalid_argument("timeline attribute keys must be unique");
        }
    }
}

std::optional<StringId> Attributes::find(StringId key) const
{
    const std::vector<Attribute>::const_iterator found = std::lower_bound(m_values.begin(), m_values.end(), key,
        [](Attribute attribute, StringId candidate) { return attribute.key() < candidate; });
    return found != m_values.end() && found->key() == key ? std::optional<StringId>{found->value()} : std::nullopt;
}

int Attributes::size() const
{
    return size_cast(m_values);
}

} // namespace timeline
