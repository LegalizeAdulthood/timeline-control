// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/Document.h>

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace timeline_par_animator
{

/// Source-truth transformation of a measured feature or counted event pulse.
/// Operation describes how the output is applied to an animation parameter;
/// it does not alter the standalone signal displayed by the timeline.
struct MappingRecipe
{
    std::string source;
    std::string target;
    std::string operation;
    double scale{1.0};
    double offset{0.0};
    double decay_seconds{0.0};
    std::optional<std::pair<double, double>> clamp;
};

/// Original frame-addressed measurement or one occurrence of a music event.
/// Event inputs have value one; simultaneous inputs are counted, not merged.
struct MappingInput
{
    std::string source;
    timeline::Ticks frame;
    double value;
};

/// Source output policy retained without merging into authored animation.
struct MappingOutput
{
    std::string mode;
    std::string namespace_name;
};

/// Adapter-owned recipes and measured inputs, separate from display caches.
///
/// Materialization recomputes disposable generic keyframe lanes alongside the
/// unmodified source music lanes. Generated pulses follow beat-keys' finite
/// exponential decay, frame rounding, overlap summation, and zero-return rules.
///
class BeatKeysMapping
{
public:
    BeatKeysMapping(timeline::Document source_document, std::vector<MappingRecipe> recipes,
        std::vector<MappingInput> inputs, MappingOutput output, std::filesystem::path config_path);

    const timeline::Document &source_document() const
    {
        return m_source_document;
    }
    const std::vector<MappingRecipe> &recipes() const
    {
        return m_recipes;
    }
    const std::vector<MappingInput> &inputs() const
    {
        return m_inputs;
    }
    const std::filesystem::path &config_path() const
    {
        return m_config_path;
    }
    const MappingOutput &output() const
    {
        return m_output;
    }

    timeline::Document materialize() const;

private:
    timeline::Document m_source_document;
    std::vector<MappingRecipe> m_recipes;
    std::vector<MappingInput> m_inputs;
    MappingOutput m_output;
    std::filesystem::path m_config_path;
};

} // namespace timeline_par_animator
