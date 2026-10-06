// Copyright (c) 2026 Richard Thomson

#include <timelineViewer/load_timeline.h>

#include <timelineParAnimator/TimelineJson.h>

#include <exception>
#include <system_error>
#include <utility>

namespace timeline_viewer
{

LoadResult load_timeline(const std::filesystem::path &path, bool append,
    const std::optional<timeline::Document> &current_document,
    const std::vector<timeline_par_animator::BeatKeysMapping> &current_mappings)
{
    LoadResult result;
    timeline_par_animator::JsonImportOptions options;
    if (append && current_document && current_document->frame_grid())
    {
        const timeline::FrameGrid &grid = *current_document->frame_grid();
        options.ticks_per_second = grid.timebase().ticks_per_second();
        options.frames_per_second_numerator = grid.frames_per_second_numerator();
        options.frames_per_second_denominator = grid.frames_per_second_denominator();
    }
    const std::filesystem::path companion = path.parent_path() / "adapter.beat-keys.json";
    std::error_code error;
    if (std::filesystem::is_regular_file(companion, error))
    {
        options.beat_keys_config_path = companion;
    }
    timeline_par_animator::JsonImportResult imported = timeline_par_animator::import_timeline_json(path, options);
    result.diagnostics = std::move(imported.diagnostics);
    if (!imported.succeeded())
    {
        return result;
    }
    if (append && current_document)
    {
        try
        {
            imported.document = timeline::combine_documents(*current_document, *imported.document);
        }
        catch (const std::exception &exception)
        {
            result.outcome = LoadOutcome::COMPOSITION_FAILED;
            result.diagnostics.emplace_back(exception.what());
            return result;
        }
        result.mappings = current_mappings;
    }
    if (imported.mapping)
    {
        result.mappings.push_back(std::move(*imported.mapping));
    }
    result.document = std::move(*imported.document);
    result.outcome = LoadOutcome::LOADED;
    return result;
}

} // namespace timeline_viewer
