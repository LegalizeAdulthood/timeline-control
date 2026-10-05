// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/StringTable.h>

#include <optional>
#include <vector>

namespace timeline
{

/// One immutable key/value pair in a document string table.
///
class Attribute
{
public:
    Attribute(StringId key, StringId value) :
        m_key(key),
        m_value(value)
    {
    }

    StringId key() const
    {
        return m_key;
    }
    StringId value() const
    {
        return m_value;
    }

private:
    StringId m_key;
    StringId m_value;
};

inline bool operator==(Attribute left, Attribute right)
{
    return left.key() == right.key() && left.value() == right.value();
}

/// Immutable attributes sorted by key identity for compact deterministic lookup.
///
class Attributes
{
public:
    Attributes() = default;
    explicit Attributes(std::vector<Attribute> values);

    std::optional<StringId> find(StringId key) const;
    const std::vector<Attribute> &values() const
    {
        return m_values;
    }
    bool empty() const
    {
        return m_values.empty();
    }
    int size() const;

private:
    std::vector<Attribute> m_values;
};

inline bool operator==(const Attributes &left, const Attributes &right)
{
    return left.values() == right.values();
}

} // namespace timeline
