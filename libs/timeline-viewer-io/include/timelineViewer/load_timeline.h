// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timelineParAnimator/BeatKeysMapping.h>

#include <timeline/Document.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace timeline_viewer
{

/// Presentation-neutral result category for a viewer timeline load.
///
enum class LoadOutcome
{
    LOADED,
    IMPORT_FAILED,
    COMPOSITION_FAILED
};

/// Candidate viewer state produced by importing or appending timeline JSON.
///
/// Successful results own the replacement document and complete mapping set.
/// Failed results omit replacement state and carry diagnostics without changing
/// the caller's displayed state.
///
struct LoadResult
{
    LoadOutcome outcome{LoadOutcome::IMPORT_FAILED};
    std::optional<timeline::Document> document;
    std::vector<timeline_par_animator::BeatKeysMapping> mappings;
    std::vector<std::string> diagnostics;

    bool succeeded() const
    {
        return outcome == LoadOutcome::LOADED && document.has_value();
    }
};

/// Loads replacement or appended timeline JSON for a presentation host.
LoadResult load_timeline(const std::filesystem::path &path, bool append,
    const std::optional<timeline::Document> &current_document,
    const std::vector<timeline_par_animator::BeatKeysMapping> &current_mappings);

} // namespace timeline_viewer
