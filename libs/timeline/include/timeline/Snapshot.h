// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/DisplayList.h>

#include <string>

namespace timeline
{

/// Renders primitives in paint order as deterministic, locale-independent text.
///
/// Records integral geometry, semantic styles, source IDs, and escaped text.
/// Output uses ASCII and LF separators without depending on a GUI toolkit.
std::string render_snapshot(const DisplayList &display_list);

} // namespace timeline
