// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timelineParAnimator/TimelineJson.h>

#include <timeline/Attributes.h>
#include <timeline/Document.h>

#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

/// Test-facing view that resolves compact attributes through their document.
///
class ResolvedAttributes
{
public:
    ResolvedAttributes(const timeline::Document &document, const timeline::Attributes &attributes) :
        m_strings(document.strings()),
        m_attributes(attributes)
    {
    }

    std::string_view at(std::string_view key) const
    {
        const std::optional<timeline::StringId> key_id = m_strings.find(key);
        if (!key_id)
        {
            throw std::out_of_range("timeline attribute key is not interned");
        }
        const std::optional<timeline::StringId> value_id = m_attributes.find(*key_id);
        if (!value_id)
        {
            throw std::out_of_range("timeline attribute key is not present");
        }
        return m_strings.lookup(*value_id);
    }

    int count(std::string_view key) const
    {
        const std::optional<timeline::StringId> key_id = m_strings.find(key);
        return key_id && m_attributes.find(*key_id) ? 1 : 0;
    }

    std::map<std::string, std::string> values() const
    {
        std::map<std::string, std::string> result;
        for (const timeline::Attribute &attribute : m_attributes.values())
        {
            result.emplace(m_strings.lookup(attribute.key()), m_strings.lookup(attribute.value()));
        }
        return result;
    }

private:
    const timeline::StringTable &m_strings;
    const timeline::Attributes &m_attributes;
};

inline ResolvedAttributes resolved_attributes(
    const timeline::Document &document, const timeline::Attributes &attributes)
{
    return ResolvedAttributes(document, attributes);
}

inline ResolvedAttributes resolved_attributes(
    const timeline_par_animator::JsonImportResult &result, const timeline::Attributes &attributes)
{
    if (!result.document)
    {
        throw std::invalid_argument("cannot resolve attributes without an imported document");
    }
    return ResolvedAttributes(*result.document, attributes);
}
