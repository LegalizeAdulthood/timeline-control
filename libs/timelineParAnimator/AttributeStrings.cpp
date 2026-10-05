// Copyright (c) 2026 Richard Thomson

#include <AttributeStrings.h>

#include <timeline/Document.h>

#include <utility>
#include <vector>

namespace timeline_par_animator::detail
{

timeline::Attributes intern_attributes(const AttributeStrings &attributes, timeline::StringTableBuilder &strings)
{
    std::vector<timeline::Attribute> result;
    for (const auto &[key, value] : attributes)
    {
        result.emplace_back(strings.intern(key), strings.intern(value));
    }
    return timeline::Attributes(std::move(result));
}

timeline::Attributes intern_attributes(const AttributeStrings &attributes, timeline::DocumentBuilder &builder)
{
    std::vector<timeline::Attribute> result;
    for (const auto &[key, value] : attributes)
    {
        result.emplace_back(builder.intern(key), builder.intern(value));
    }
    return timeline::Attributes(std::move(result));
}

AttributeStrings resolve_attributes(const timeline::Attributes &attributes, const timeline::StringTableBuilder &strings)
{
    AttributeStrings result;
    for (const timeline::Attribute &attribute : attributes.values())
    {
        result.emplace(strings.lookup(attribute.key()), strings.lookup(attribute.value()));
    }
    return result;
}

} // namespace timeline_par_animator::detail
