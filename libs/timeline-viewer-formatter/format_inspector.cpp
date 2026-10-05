// Copyright (c) 2026 Richard Thomson

#include <timelineViewer/format_inspector.h>

namespace timeline_viewer
{

std::string format_inspector(const std::optional<timeline::Document> &document,
    const std::optional<timeline::HitResult> &hit, const std::optional<timeline::Interaction> &interaction,
    const std::optional<timeline::FrameInspection> &inspection,
    const std::vector<timeline_par_animator::BeatKeysMapping> &mappings)
{
    if (!document)
    {
        return "No frame inspection.";
    }
    std::string text = hit ? timeline::to_string(*document, *hit) : "Hit: none";
    text += "\n\n" + timeline::to_string(*document);
    for (const timeline_par_animator::BeatKeysMapping &mapping : mappings)
    {
        text += "\n\n" + timeline_par_animator::to_string(mapping);
    }
    if (interaction)
    {
        const std::string interaction_text = timeline::to_string(*document, *interaction);
        if (!interaction_text.empty())
        {
            text += "\n\n" + interaction_text;
        }
    }
    if (!inspection)
    {
        text += "\n\nNo frame inspection.";
        return text;
    }
    text += "\n\n" + timeline::to_string(*document, *inspection);
    return text;
}

} // namespace timeline_viewer
