// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/StringTable.h>
#include <timeline/Time.h>

#include <map>
#include <optional>
#include <string>

namespace timeline
{

/// String-valued metadata attached to generic timeline content.
///
using Attributes = std::map<std::string, std::string>;

/// An event occurring at one exact timeline time.
///
/// An instant carries stable identity, generic kind and label text, optional
/// strength or confidence, and application-defined attributes.
///
class Instant
{
public:
    Instant(StringId id, std::string kind, Time time);
    Instant(StringId id, std::string kind, Time time, std::string label, std::optional<double> strength,
        Attributes attributes);

    /// Return a copy with the supplied identity.
    Instant with_id(StringId id) const;
    StringId id() const
    {
        return m_id;
    }
    const std::string &kind() const
    {
        return m_kind;
    }
    Time time() const
    {
        return m_time;
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
    Time m_time;
    std::string m_label;
    std::optional<double> m_strength;
    Attributes m_attributes;
};

} // namespace timeline
