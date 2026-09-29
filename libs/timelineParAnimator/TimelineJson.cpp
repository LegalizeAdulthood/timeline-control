// Copyright (c) 2026 Richard Thomson

#include <timelineParAnimator/TimelineJson.h>

#include <utility>

namespace timeline_par_animator
{
namespace
{

constexpr timeline::Ticks TICKS_PER_SECOND = 120000;

} // namespace

TimelineJsonImportResult import_timeline_json(const std::filesystem::path &source_path)
{
    auto result = TimelineJsonImportResult{};
    if (source_path.empty())
    {
        result.diagnostics.emplace_back("No JSON file was selected.");
        return result;
    }

    auto metadata = timeline::TimelineMetadata(source_path.filename().string(), source_path.string());
    result.document.emplace(TICKS_PER_SECOND, std::move(metadata));
    return result;
}

} // namespace timeline_par_animator
