// Copyright (c) 2026 Richard Thomson

#include <timelineParAnimator/BeatKeysMapping.h>

#include <AttributeStrings.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string_view>

namespace timeline_par_animator
{
namespace
{

bool continuous_source(std::string_view source)
{
    return source == "music.rms" || source == "music.peak";
}

void validate_recipe(const MappingRecipe &recipe)
{
    if (!continuous_source(recipe.source) && recipe.source != "music.note_pulse" &&
        recipe.source != "music.effect_pulse" && recipe.source != "music.row_pulse")
    {
        throw std::invalid_argument("unknown source reference: " + recipe.source);
    }
    if (recipe.target.empty() ||
        (recipe.operation != "replace" && recipe.operation != "add" && recipe.operation != "multiply"))
    {
        throw std::invalid_argument("binding requires a target and replace, add, or multiply operation");
    }
    if (!std::isfinite(recipe.scale) || !std::isfinite(recipe.offset) || !std::isfinite(recipe.decay_seconds) ||
        recipe.decay_seconds < 0.0)
    {
        throw std::invalid_argument("binding scale, offset, and nonnegative decay must be finite");
    }
    if (recipe.clamp &&
        (!std::isfinite(recipe.clamp->first) || !std::isfinite(recipe.clamp->second) ||
            recipe.clamp->second < recipe.clamp->first))
    {
        throw std::invalid_argument("binding clamp requires finite ordered bounds");
    }
}

double transform(double input, const MappingRecipe &recipe)
{
    double value = input * recipe.scale + recipe.offset;
    if (recipe.clamp)
    {
        value = std::clamp(value, recipe.clamp->first, recipe.clamp->second);
    }
    return std::round(value * 1000000.0) / 1000000.0;
}

std::map<timeline::Ticks, double> recipe_values(
    const MappingRecipe &recipe, const std::vector<MappingInput> &inputs, const timeline::FrameGrid &grid)
{
    std::map<timeline::Ticks, double> values;
    if (continuous_source(recipe.source))
    {
        for (const MappingInput &input : inputs)
        {
            if (input.source == recipe.source)
            {
                values.emplace(input.frame, transform(input.value, recipe));
            }
        }
        return values;
    }

    const double fps = static_cast<double>(grid.frames_per_second_numerator()) /
        static_cast<double>(grid.frames_per_second_denominator());
    const double decay_count = std::max(1.0, std::ceil(recipe.decay_seconds * fps));
    if (!std::isfinite(decay_count) || decay_count >= static_cast<double>(std::numeric_limits<timeline::Ticks>::max()))
    {
        throw std::overflow_error("binding decay frame count is too large");
    }
    const timeline::Ticks decay_frames = static_cast<timeline::Ticks>(decay_count);
    std::map<timeline::Ticks, int> counts;
    for (const MappingInput &input : inputs)
    {
        if (input.source == recipe.source)
        {
            ++counts[input.frame];
        }
    }
    std::set<timeline::Ticks> returns;
    for (const auto &[frame, count] : counts)
    {
        if (decay_frames >= std::numeric_limits<timeline::Ticks>::max() - frame)
        {
            throw std::overflow_error("binding pulse extent is too large");
        }
        for (timeline::Ticks step = 0; step < decay_frames; ++step)
        {
            const double amplitude = step == 0
                ? static_cast<double>(count)
                : static_cast<double>(count) * std::exp(-static_cast<double>(step) / fps / recipe.decay_seconds);
            values[frame + step] += amplitude;
        }
        returns.insert(frame + decay_frames);
    }
    for (auto &[frame, value] : values)
    {
        value = transform(value, recipe);
    }
    for (const timeline::Ticks frame : returns)
    {
        values.emplace(frame, 0.0);
    }
    return values;
}

} // namespace

BeatKeysMapping::BeatKeysMapping(timeline::Document source_document, std::vector<MappingRecipe> recipes,
    std::vector<MappingInput> inputs, MappingOutput output, std::filesystem::path config_path) :
    m_source_document(std::move(source_document)),
    m_recipes(std::move(recipes)),
    m_inputs(std::move(inputs)),
    m_output(std::move(output)),
    m_config_path(std::move(config_path))
{
    if (!m_source_document.frame_grid())
    {
        throw std::invalid_argument("beat-keys mapping requires a source frame grid");
    }
    if ((m_output.mode != "overlay" && m_output.mode != "merge") || m_output.namespace_name.empty())
    {
        throw std::invalid_argument("mapping output requires overlay or merge mode and a namespace");
    }
    int index = 0;
    for (const MappingRecipe &recipe : m_recipes)
    {
        try
        {
            validate_recipe(recipe);
        }
        catch (const std::exception &error)
        {
            throw std::invalid_argument("bindings[" + std::to_string(index) + "]: " + error.what());
        }
        ++index;
    }
    std::set<std::pair<std::string, timeline::Ticks>> feature_frames;
    for (const MappingInput &input : m_inputs)
    {
        if (input.frame < 0 || input.frame >= m_source_document.frame_grid()->frame_count() ||
            !std::isfinite(input.value))
        {
            throw std::invalid_argument("mapping input requires a valid source frame and finite value");
        }
        if (continuous_source(input.source) && !feature_frames.emplace(input.source, input.frame).second)
        {
            throw std::invalid_argument("mapping feature has a duplicate frame");
        }
    }
}

timeline::Document BeatKeysMapping::materialize() const
{
    const timeline::FrameGrid &source_grid = *m_source_document.frame_grid();
    std::vector<std::map<timeline::Ticks, double>> outputs;
    timeline::Ticks frame_count = source_grid.frame_count();
    int key_count = 0;
    for (const MappingRecipe &recipe : m_recipes)
    {
        outputs.push_back(recipe_values(recipe, m_inputs, source_grid));
        if (!outputs.back().empty())
        {
            frame_count = std::max(frame_count, outputs.back().rbegin()->first + 1);
            key_count += timeline::size_cast(outputs.back());
        }
    }
    timeline::FrameGrid grid(source_grid.timebase(), frame_count, source_grid.frames_per_second_numerator(),
        source_grid.frames_per_second_denominator(), source_grid.offset());
    timeline::StringTableBuilder strings(m_source_document.strings());
    const timeline::Metadata metadata(
        strings.intern(m_config_path.filename().string()), strings.intern(m_config_path.string()));
    timeline::Document document = m_source_document.source_summary()
        ? timeline::Document(
              grid, *m_source_document.source_summary(), timeline::size_cast(m_recipes), key_count, metadata)
        : timeline::Document(grid, timeline::size_cast(m_recipes), key_count, metadata);
    timeline::DocumentBuilder builder(std::move(document), std::move(strings).build());
    for (const timeline::Lane &lane : m_source_document.lanes())
    {
        builder.add_lane(lane);
    }
    for (int index = 0; index < timeline::size_cast(m_recipes); ++index)
    {
        if (outputs[index].empty())
        {
            continue;
        }
        const MappingRecipe &recipe = m_recipes[index];
        const std::string id = "mapping-" + std::to_string(index);
        timeline::Lane lane(builder.intern(id), builder.intern(recipe.target), builder.intern("keyframes"),
            grid.offset(), grid.end_time());
        for (const auto &[frame, value] : outputs[index])
        {
            lane.add(timeline::Keyframe(builder.intern(id + "-" + std::to_string(frame)), grid.frame_start(frame),
                value, timeline::KeyframeInterpolation::HOLD,
                detail::intern_attributes(
                    {{"source", recipe.source}, {"target", recipe.target}, {"op", recipe.operation}}, builder)));
        }
        builder.add_lane(std::move(lane));
    }
    return std::move(builder).build();
}

} // namespace timeline_par_animator
