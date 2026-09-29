// Copyright (c) 2026 Richard Thomson

#include <timelineParAnimator/TimelineJson.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace timeline_par_animator
{
namespace
{

using Json = nlohmann::json;

constexpr auto SUPPORTED_ROOT_FIELDS = std::array<std::string_view, 8>{
    "parameter-catalogs",
    "source",
    "output",
    "parallel",
    "video",
    "num-frames",
    "tracks",
    "layers",
};

bool has_supported_root_field(const std::string &name)
{
    return std::find(SUPPORTED_ROOT_FIELDS.begin(), SUPPORTED_ROOT_FIELDS.end(), name) != SUPPORTED_ROOT_FIELDS.end();
}

bool validate_root(const Json &config, std::vector<std::string> &diagnostics)
{
    if (!config.is_object())
    {
        diagnostics.emplace_back("ParAnimator config must be a JSON object.");
        return false;
    }

    auto valid = true;
    for (auto field = config.begin(); field != config.end(); ++field)
    {
        if (!has_supported_root_field(field.key()))
        {
            diagnostics.emplace_back("Unsupported ParAnimator property '" + field.key() + "'.");
            valid = false;
        }
    }

    const auto require_type = [&config, &diagnostics, &valid](const char *name, bool type_matches, const char *type)
    {
        if (!config.contains(name))
        {
            diagnostics.emplace_back("ParAnimator config requires '" + std::string(name) + "'.");
            valid = false;
        }
        else if (!type_matches)
        {
            diagnostics.emplace_back("ParAnimator property '" + std::string(name) + "' must be " + type + ".");
            valid = false;
        }
    };

    require_type("parameter-catalogs",
        config.contains("parameter-catalogs") && config.at("parameter-catalogs").is_array(), "an array");
    require_type("output", config.contains("output") && config.at("output").is_object(), "an object");
    require_type("video", config.contains("video") && config.at("video").is_string(), "a string");
    require_type(
        "num-frames", config.contains("num-frames") && config.at("num-frames").is_number_integer(), "an integer");

    const auto has_tracks = config.contains("tracks");
    const auto has_layers = config.contains("layers");
    if (has_tracks == has_layers)
    {
        diagnostics.emplace_back("ParAnimator config must contain either 'tracks' or 'layers'.");
        valid = false;
    }
    if (has_tracks)
    {
        if (!config.at("tracks").is_array())
        {
            diagnostics.emplace_back("ParAnimator property 'tracks' must be an array.");
            valid = false;
        }
        if (!config.contains("source") || !config.at("source").is_object())
        {
            diagnostics.emplace_back("ParAnimator config with tracks requires object property 'source'.");
            valid = false;
        }
    }
    if (has_layers && !config.at("layers").is_array())
    {
        diagnostics.emplace_back("ParAnimator property 'layers' must be an array.");
        valid = false;
    }

    if (config.contains("num-frames") && config.at("num-frames").is_number_integer() &&
        config.at("num-frames").get<timeline::Ticks>() < 0)
    {
        diagnostics.emplace_back("ParAnimator property 'num-frames' cannot be negative.");
        valid = false;
    }

    return valid;
}

std::size_t count_keyframes(const Json &value)
{
    auto result = std::size_t{0};
    if (value.is_array())
    {
        for (const auto &element : value)
        {
            result += count_keyframes(element);
        }
    }
    else if (value.is_object())
    {
        for (auto field = value.begin(); field != value.end(); ++field)
        {
            if (field.key() == "keys")
            {
                if (!field->is_array())
                {
                    throw std::invalid_argument("ParAnimator track property 'keys' must be an array.");
                }
                result += field->size();
            }
            else
            {
                result += count_keyframes(*field);
            }
        }
    }
    return result;
}

void summarize_tracks(const Json &tracks, std::size_t &track_count, std::size_t &keyframe_count)
{
    if (!tracks.is_array())
    {
        throw std::invalid_argument("ParAnimator tracks must be an array.");
    }
    for (const auto &track : tracks)
    {
        if (!track.is_object())
        {
            throw std::invalid_argument("Each ParAnimator track must be an object.");
        }
        ++track_count;
        keyframe_count += count_keyframes(track);
    }
}

std::pair<std::size_t, std::size_t> summarize_content(const Json &config)
{
    auto track_count = std::size_t{0};
    auto keyframe_count = std::size_t{0};
    if (config.contains("tracks"))
    {
        summarize_tracks(config.at("tracks"), track_count, keyframe_count);
    }
    else
    {
        for (const auto &layer : config.at("layers"))
        {
            if (!layer.is_object() || !layer.contains("tracks"))
            {
                throw std::invalid_argument("Each ParAnimator layer must contain a tracks array.");
            }
            summarize_tracks(layer.at("tracks"), track_count, keyframe_count);
        }
    }
    return {track_count, keyframe_count};
}

} // namespace

TimelineJsonImportResult import_timeline_json(
    const std::filesystem::path &source_path, const TimelineJsonImportOptions &options)
{
    auto result = TimelineJsonImportResult{};
    if (source_path.empty())
    {
        result.diagnostics.emplace_back("No JSON file was selected.");
        return result;
    }

    auto input = std::ifstream(source_path);
    if (!input)
    {
        result.diagnostics.emplace_back("Unable to open ParAnimator config '" + source_path.string() + "'.");
        return result;
    }

    auto config = Json{};
    try
    {
        input >> config;
    }
    catch (const Json::parse_error &error)
    {
        result.diagnostics.emplace_back("Unable to parse ParAnimator config: " + std::string(error.what()));
        return result;
    }

    if (!validate_root(config, result.diagnostics))
    {
        return result;
    }

    try
    {
        const auto [track_count, keyframe_count] = summarize_content(config);
        auto metadata = timeline::TimelineMetadata(source_path.filename().string(), source_path.string());
        auto frame_grid = timeline::FrameGrid(timeline::Timebase(options.ticks_per_second),
            config.at("num-frames").get<timeline::Ticks>(), options.frames_per_second_numerator,
            options.frames_per_second_denominator);
        result.document.emplace(std::move(frame_grid), track_count, keyframe_count, std::move(metadata));
    }
    catch (const std::exception &error)
    {
        result.diagnostics.emplace_back("Unable to import ParAnimator config: " + std::string(error.what()));
    }
    return result;
}

} // namespace timeline_par_animator
