// Copyright (c) 2026 Richard Thomson

#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace timeline_viewer
{

/// Presentation-neutral result of writing a rendered timeline snapshot.
///
struct SnapshotWriteResult
{
    bool written{false};
    std::vector<std::string> diagnostics;

    bool succeeded() const
    {
        return written;
    }
};

/// Writes snapshot text without introducing GUI-specific error handling.
SnapshotWriteResult write_snapshot(const std::filesystem::path &path, std::string_view snapshot);

} // namespace timeline_viewer
