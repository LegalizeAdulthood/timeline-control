// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/Lane.h>
#include <timeline/Time.h>

#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace timeline_par_animator::detail
{

void color_map_lanes(const nlohmann::json &track, const std::filesystem::path &source_path, const std::string &id,
    const std::string &layer, const timeline::FrameGrid &grid, timeline::StringTableBuilder &strings,
    std::vector<timeline::Lane> &lanes);

} // namespace timeline_par_animator::detail
