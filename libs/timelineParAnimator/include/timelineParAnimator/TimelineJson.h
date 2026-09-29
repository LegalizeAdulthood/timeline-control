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
struct JsonImportOptions
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
struct JsonImportResult
{
    std::optional<timeline::Document> document;
    std::vector<std::string> diagnostics;

    bool succeeded() const
    {
        return document.has_value();
    }
};

/// Imports supported ParAnimator or ParBeatdown metadata from a JSON file.
JsonImportResult import_timeline_json(const std::filesystem::path &source_path, const JsonImportOptions &options);
inline JsonImportResult import_timeline_json(const std::filesystem::path &source_path)
{
    return import_timeline_json(source_path, JsonImportOptions{});
}

} // namespace timeline_par_animator
