// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timelineParAnimator/TimelineJson.h>

#include <timeline/Interaction.h>
#include <timeline/Query.h>

#include <optional>
#include <string>
#include <vector>

namespace timeline_viewer
{

/// Assembles component-provided text for presentation by any viewer toolkit.
std::string format_inspector(const std::optional<timeline::Document> &document,
    const std::optional<timeline::HitResult> &hit, const std::optional<timeline::Interaction> &interaction,
    const std::optional<timeline::FrameInspection> &inspection,
    const std::vector<timeline_par_animator::BeatKeysMapping> &mappings);

} // namespace timeline_viewer
