// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/Document.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace timeline_par_animator
{

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

/// Creates the initial empty timeline document for a selected JSON path.
TimelineJsonImportResult import_timeline_json(const std::filesystem::path &source_path);

} // namespace timeline_par_animator
