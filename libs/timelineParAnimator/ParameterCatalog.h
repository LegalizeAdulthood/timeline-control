// Copyright (c) 2026 Richard Thomson

#pragma once

#include <nlohmann/json_fwd.hpp>

namespace timeline_par_animator
{

/// Validates a complete source catalog and appends its distinct named entries.
void append_parameter_catalog(nlohmann::json &target, const nlohmann::json &catalog);

} // namespace timeline_par_animator
