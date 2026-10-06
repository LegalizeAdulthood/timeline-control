// Copyright (c) 2026 Richard Thomson

#include <timelineViewer/write_snapshot.h>

#include <fstream>

namespace timeline_viewer
{

SnapshotWriteResult write_snapshot(const std::filesystem::path &path, std::string_view snapshot)
{
    SnapshotWriteResult result;
    std::ofstream output(path, std::ios::binary);
    output << snapshot;
    output.close();
    if (!output)
    {
        result.diagnostics.emplace_back("Unable to write the timeline snapshot.");
        return result;
    }
    result.written = true;
    return result;
}

} // namespace timeline_viewer
