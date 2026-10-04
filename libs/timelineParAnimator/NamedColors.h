// Copyright (c) 2026 Richard Thomson

#pragma once

#include <array>
#include <string_view>

namespace timeline_par_animator::detail
{

/// Fixed CSS named color expressed in byte RGB components.
///
struct NamedColor
{
    std::string_view name;
    int red;
    int green;
    int blue;
};

// CSS named colors accepted by the ParAnimator color-spec parser.
extern const std::array<NamedColor, 148> NAMED_COLORS;

} // namespace timeline_par_animator::detail
