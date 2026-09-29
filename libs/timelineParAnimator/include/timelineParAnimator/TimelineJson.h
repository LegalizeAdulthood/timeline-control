// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/Document.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace timeline_par_animator
{

/// Timing policy used when a ParAnimator config supplies frame numbers only.
///
/// The adapter projects source frames onto exact core timeline ticks using the
/// configured rational frame rate.
///
struct TimelineJsonImportOptions
{
    timeline::Ticks ticks_per_second{120000};
    timeline::Ticks frames_per_second_numerator{30};
    timeline::Ticks frames_per_second_denominator{1};
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

/// Imports ParAnimator frame and authored-content metadata from a JSON file.
TimelineJsonImportResult import_timeline_json(
    const std::filesystem::path &source_path, const TimelineJsonImportOptions &options = TimelineJsonImportOptions{});

} // namespace timeline_par_animator
