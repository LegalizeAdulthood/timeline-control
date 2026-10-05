// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/Attributes.h>

#include <map>
#include <string>

namespace timeline
{
class DocumentBuilder;
}

namespace timeline_par_animator::detail
{

/// Mutable source-text attributes used only while translating adapter data.
///
using AttributeStrings = std::map<std::string, std::string>;

timeline::Attributes intern_attributes(const AttributeStrings &attributes, timeline::StringTableBuilder &strings);
timeline::Attributes intern_attributes(const AttributeStrings &attributes, timeline::DocumentBuilder &builder);
AttributeStrings resolve_attributes(
    const timeline::Attributes &attributes, const timeline::StringTableBuilder &strings);

} // namespace timeline_par_animator::detail
