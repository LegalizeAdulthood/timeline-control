// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/Document.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace timeline_par_animator
{

/// Timing policy and companion inputs used by timeline JSON adapters.
///
/// ParAnimator files use the rational frame rate directly. ParBeatdown files
/// use an optional beat-keys config for source FPS and synchronization offset.
///
struct TimelineJsonImportOptions
{
    timeline::Ticks ticks_per_second{120000};
    timeline::Ticks frames_per_second_numerator{30};
    timeline::Ticks frames_per_second_denominator{1};
    std::filesystem::path beat_keys_config_path;
};

/// Outcome of importing timeline JSON through the ParAnimator adapters.
///
/// A successful result owns a timeline document. Diagnostics remain adapter
/// concerns and are never stored in the core document.
struct TimelineJsonImportResult
{
    std::optional<timeline::TimelineDocument> document;
    std::vector<std::string> diagnostics;

    bool succeeded() const
    {
        return document.has_value();
    }
};

/// Imports supported ParAnimator or ParBeatdown metadata from a JSON file.
TimelineJsonImportResult import_timeline_json(
    const std::filesystem::path &source_path, const TimelineJsonImportOptions &options = TimelineJsonImportOptions{});

} // namespace timeline_par_animator
