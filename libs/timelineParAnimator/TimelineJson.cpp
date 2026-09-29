// Copyright (c) 2026 Richard Thomson

#include <timelineParAnimator/TimelineJson.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace timeline_par_animator
{
namespace
{

using Json = nlohmann::json;

constexpr auto SUPPORTED_PAR_ANIMATOR_ROOT_FIELDS = std::array<std::string_view, 8>{
    "parameter-catalogs",
    "source",
    "output",
    "parallel",
    "video",
    "num-frames",
    "tracks",
    "layers",
};
constexpr std::string_view TRACKER_TIMELINE_SCHEMA = "par-beatdown.tracker-timeline";
constexpr std::string_view BEAT_KEYS_SCHEMA = "par-beatdown.beat-keys";

/// Frame-rate and synchronization policy selected for a tracker timeline.
struct TrackerTiming
{
    timeline::Ticks frames_per_second_numerator;
    timeline::Ticks frames_per_second_denominator;
    double offset_seconds;
};

/// Optional frame and time boundaries summarized from tracker timeline JSON.
struct TrackerExtent
{
    std::optional<timeline::Ticks> first_frame;
    std::optional<timeline::Ticks> last_frame;
    std::optional<double> first_seconds;
    std::optional<double> last_seconds;
    timeline::Ticks frame_count{0};
};

bool read_json_file(
    const std::filesystem::path &path, const char *label, Json &json, std::vector<std::string> &diagnostics)
{
    auto input = std::ifstream(path);
    if (!input)
    {
        diagnostics.emplace_back("Unable to open " + std::string(label) + " '" + path.string() + "'.");
        return false;
    }

    try
    {
        input >> json;
    }
    catch (const Json::parse_error &error)
    {
        diagnostics.emplace_back("Unable to parse " + std::string(label) + ": " + error.what());
        return false;
    }
    return true;
}

bool has_supported_par_animator_root_field(const std::string &name)
{
    return std::find(SUPPORTED_PAR_ANIMATOR_ROOT_FIELDS.begin(), SUPPORTED_PAR_ANIMATOR_ROOT_FIELDS.end(), name) !=
        SUPPORTED_PAR_ANIMATOR_ROOT_FIELDS.end();
}

bool validate_par_animator_root(const Json &config, std::vector<std::string> &diagnostics)
{
    if (!config.is_object())
    {
        diagnostics.emplace_back("ParAnimator config must be a JSON object.");
        return false;
    }

    auto valid = true;
    for (auto field = config.begin(); field != config.end(); ++field)
    {
        if (!has_supported_par_animator_root_field(field.key()))
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

std::pair<std::size_t, std::size_t> summarize_par_animator_content(const Json &config)
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

void import_par_animator(const std::filesystem::path &source_path, const Json &config,
    const TimelineJsonImportOptions &options, TimelineJsonImportResult &result)
{
    if (!validate_par_animator_root(config, result.diagnostics))
    {
        return;
    }

    try
    {
        const auto [track_count, keyframe_count] = summarize_par_animator_content(config);
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
}

bool validate_tracker_timeline(const Json &config, std::vector<std::string> &diagnostics)
{
    if (!config.is_object())
    {
        diagnostics.emplace_back("ParBeatdown timeline must be a JSON object.");
        return false;
    }
    if (!config.contains("schema") || !config.at("schema").is_string() ||
        config.at("schema").get<std::string>() != TRACKER_TIMELINE_SCHEMA)
    {
        diagnostics.emplace_back("Unsupported ParBeatdown timeline schema.");
        return false;
    }
    if (!config.contains("version") || !config.at("version").is_number_integer() ||
        config.at("version").get<timeline::Ticks>() != 1)
    {
        diagnostics.emplace_back("Unsupported ParBeatdown timeline version.");
        return false;
    }
    if (!config.contains("features") || !config.at("features").is_array())
    {
        diagnostics.emplace_back("ParBeatdown timeline requires a features array.");
        return false;
    }
    if (!config.contains("events") || !config.at("events").is_array())
    {
        diagnostics.emplace_back("ParBeatdown timeline requires an events array.");
        return false;
    }
    if (config.contains("timeline") && !config.at("timeline").is_object())
    {
        diagnostics.emplace_back("ParBeatdown timeline property 'timeline' must be an object.");
        return false;
    }
    if (config.contains("render") && !config.at("render").is_object())
    {
        diagnostics.emplace_back("ParBeatdown timeline property 'render' must be an object.");
        return false;
    }
    if (config.contains("diagnostics") && !config.at("diagnostics").is_object())
    {
        diagnostics.emplace_back("ParBeatdown timeline property 'diagnostics' must be an object.");
        return false;
    }
    return true;
}

std::pair<timeline::Ticks, timeline::Ticks> rational_frame_rate(double frames_per_second)
{
    if (!std::isfinite(frames_per_second) || frames_per_second <= 0.0)
    {
        throw std::invalid_argument("frame rate must be finite and positive");
    }

    const auto integer_rate = static_cast<timeline::Ticks>(std::llround(frames_per_second));
    if (std::abs(frames_per_second - static_cast<double>(integer_rate)) < 0.000000001)
    {
        return {integer_rate, 1};
    }

    constexpr auto COMMON_RATES = std::array<std::pair<timeline::Ticks, timeline::Ticks>, 4>{
        std::pair<timeline::Ticks, timeline::Ticks>{24000, 1001},
        std::pair<timeline::Ticks, timeline::Ticks>{30000, 1001},
        std::pair<timeline::Ticks, timeline::Ticks>{60000, 1001},
        std::pair<timeline::Ticks, timeline::Ticks>{120000, 1001},
    };
    for (const auto &rate : COMMON_RATES)
    {
        if (std::abs(frames_per_second - static_cast<double>(rate.first) / static_cast<double>(rate.second)) <
            0.000000001)
        {
            return rate;
        }
    }

    constexpr timeline::Ticks SCALE = 1000000;
    if (frames_per_second > static_cast<double>(std::numeric_limits<timeline::Ticks>::max() / SCALE))
    {
        throw std::overflow_error("frame rate is too large");
    }
    auto numerator = static_cast<timeline::Ticks>(std::llround(frames_per_second * static_cast<double>(SCALE)));
    const auto divisor = std::gcd(numerator, SCALE);
    return {numerator / divisor, SCALE / divisor};
}

TrackerTiming timing_from_json(const Json &source, const char *label)
{
    if (!source.is_object() || !source.contains("fps") || !source.at("fps").is_number() ||
        !source.contains("offset_seconds") || !source.at("offset_seconds").is_number())
    {
        throw std::invalid_argument(std::string(label) + " requires numeric fps and offset_seconds.");
    }
    const auto frames_per_second = source.at("fps").get<double>();
    const auto offset_seconds = source.at("offset_seconds").get<double>();
    if (!std::isfinite(offset_seconds))
    {
        throw std::invalid_argument(std::string(label) + " offset_seconds must be finite.");
    }
    const auto [numerator, denominator] = rational_frame_rate(frames_per_second);
    return TrackerTiming{numerator, denominator, offset_seconds};
}

std::optional<TrackerTiming> tracker_timing(
    const Json &config, const TimelineJsonImportOptions &options, std::vector<std::string> &diagnostics)
{
    if (!options.beat_keys_config_path.empty())
    {
        auto beat_keys = Json{};
        if (!read_json_file(options.beat_keys_config_path, "beat-keys config", beat_keys, diagnostics))
        {
            throw std::invalid_argument("beat-keys config could not be read");
        }
        if (!beat_keys.is_object() || !beat_keys.contains("schema") || !beat_keys.at("schema").is_string() ||
            beat_keys.at("schema").get<std::string>() != BEAT_KEYS_SCHEMA)
        {
            throw std::invalid_argument("unsupported beat-keys config schema");
        }
        if (!beat_keys.contains("version") || !beat_keys.at("version").is_number_integer() ||
            beat_keys.at("version").get<timeline::Ticks>() != 1)
        {
            throw std::invalid_argument("unsupported beat-keys config version");
        }
        if (!beat_keys.contains("source") || !beat_keys.at("source").is_object() ||
            !beat_keys.at("source").contains("timeline") || !beat_keys.at("source").at("timeline").is_string())
        {
            throw std::invalid_argument("beat-keys config requires source timeline metadata");
        }
        return timing_from_json(beat_keys.at("source"), "beat-keys source");
    }
    if (config.contains("render"))
    {
        return timing_from_json(config.at("render"), "ParBeatdown render metadata");
    }
    return std::nullopt;
}

void expand_extent(const Json &items, TrackerExtent &extent)
{
    for (const auto &item : items)
    {
        if (!item.is_object())
        {
            throw std::invalid_argument("ParBeatdown events and features must be objects.");
        }
        if (item.contains("frame"))
        {
            if (!item.at("frame").is_number_integer())
            {
                throw std::invalid_argument("ParBeatdown frame values must be integers.");
            }
            const auto frame = item.at("frame").get<timeline::Ticks>();
            extent.first_frame = extent.first_frame ? std::min(*extent.first_frame, frame) : frame;
            extent.last_frame = extent.last_frame ? std::max(*extent.last_frame, frame) : frame;
        }
        if (item.contains("time_seconds"))
        {
            if (!item.at("time_seconds").is_number())
            {
                throw std::invalid_argument("ParBeatdown time_seconds values must be numeric.");
            }
            const auto seconds = item.at("time_seconds").get<double>();
            if (!std::isfinite(seconds))
            {
                throw std::invalid_argument("ParBeatdown time_seconds values must be finite.");
            }
            extent.first_seconds = extent.first_seconds ? std::min(*extent.first_seconds, seconds) : seconds;
            extent.last_seconds = extent.last_seconds ? std::max(*extent.last_seconds, seconds) : seconds;
        }
    }
}

TrackerExtent tracker_extent(const Json &config)
{
    auto extent = TrackerExtent{};
    expand_extent(config.at("events"), extent);
    expand_extent(config.at("features"), extent);

    if (config.contains("timeline"))
    {
        const auto &timeline = config.at("timeline");
        if (!timeline.contains("duration_seconds") || !timeline.at("duration_seconds").is_number() ||
            !timeline.contains("frames") || !timeline.at("frames").is_number_integer() ||
            !timeline.contains("first_frame") || !timeline.at("first_frame").is_number_integer() ||
            !timeline.contains("last_frame") || !timeline.at("last_frame").is_number_integer())
        {
            throw std::invalid_argument("ParBeatdown timeline extent is incomplete.");
        }

        const auto duration_seconds = timeline.at("duration_seconds").get<double>();
        extent.frame_count = timeline.at("frames").get<timeline::Ticks>();
        const auto first_frame = timeline.at("first_frame").get<timeline::Ticks>();
        const auto last_frame = timeline.at("last_frame").get<timeline::Ticks>();
        if (!std::isfinite(duration_seconds) || duration_seconds < 0.0 || extent.frame_count < 0)
        {
            throw std::invalid_argument("ParBeatdown timeline extent is invalid.");
        }
        if (extent.frame_count > 0)
        {
            if (first_frame > last_frame || last_frame >= extent.frame_count)
            {
                throw std::invalid_argument("ParBeatdown frame extent is invalid.");
            }
            extent.first_frame = first_frame;
            extent.last_frame = last_frame;
        }
        else
        {
            extent.first_frame.reset();
            extent.last_frame.reset();
        }
        extent.first_seconds = 0.0;
        extent.last_seconds = duration_seconds;
    }
    else if (extent.last_frame)
    {
        if (*extent.first_frame < 0 || *extent.last_frame == std::numeric_limits<timeline::Ticks>::max())
        {
            throw std::invalid_argument("ParBeatdown frame extent cannot form a frame grid.");
        }
        extent.frame_count = *extent.last_frame + 1;
    }
    return extent;
}

void append_diagnostic_array(
    const Json &diagnostics, const char *field, const char *prefix, std::vector<std::string> &result)
{
    if (!diagnostics.contains(field))
    {
        return;
    }
    if (!diagnostics.at(field).is_array())
    {
        throw std::invalid_argument("ParBeatdown diagnostics fields must be arrays.");
    }
    for (const auto &message : diagnostics.at(field))
    {
        if (!message.is_string())
        {
            throw std::invalid_argument("ParBeatdown diagnostics entries must be strings.");
        }
        result.emplace_back(std::string(prefix) + message.get<std::string>());
    }
}

void append_tracker_diagnostics(const Json &config, std::vector<std::string> &diagnostics)
{
    if (!config.contains("diagnostics"))
    {
        return;
    }
    const auto &source = config.at("diagnostics");
    append_diagnostic_array(source, "warnings", "Warning: ", diagnostics);
    append_diagnostic_array(source, "unsupported", "Unsupported: ", diagnostics);
    append_diagnostic_array(source, "log", "Log: ", diagnostics);
}

void import_tracker_timeline(const std::filesystem::path &source_path, const Json &config,
    const TimelineJsonImportOptions &options, TimelineJsonImportResult &result)
{
    if (!validate_tracker_timeline(config, result.diagnostics))
    {
        return;
    }

    try
    {
        const auto timebase = timeline::Timebase(options.ticks_per_second);
        const auto timing = tracker_timing(config, options, result.diagnostics);
        const auto extent = tracker_extent(config);
        auto first_time = extent.first_seconds ? std::optional<timeline::TimelineTime>{timebase.time_from_seconds(
                                                     *extent.first_seconds, timeline::TimeRounding::NEAREST)}
                                               : std::nullopt;
        auto last_time = extent.last_seconds ? std::optional<timeline::TimelineTime>{timebase.time_from_seconds(
                                                   *extent.last_seconds, timeline::TimeRounding::NEAREST)}
                                             : std::nullopt;
        auto frame_offset = std::optional<timeline::TimelineDuration>{};
        auto frame_grid = std::optional<timeline::FrameGrid>{};
        if (timing)
        {
            frame_offset = timebase.duration_from_seconds(timing->offset_seconds, timeline::TimeRounding::NEAREST);
            const auto grid_origin = timeline::TimelineTime::from_ticks(-frame_offset->ticks());
            frame_grid.emplace(timebase, extent.frame_count, timing->frames_per_second_numerator,
                timing->frames_per_second_denominator, grid_origin);
            if (!first_time && extent.first_frame)
            {
                first_time = frame_grid->frame_start(*extent.first_frame);
                last_time = frame_grid->frame_start(*extent.last_frame);
            }
        }

        append_tracker_diagnostics(config, result.diagnostics);
        auto source_summary = timeline::TimelineSourceSummary(config.at("schema").get<std::string>(),
            static_cast<std::size_t>(config.at("version").get<timeline::Ticks>()), config.at("features").size(),
            config.at("events").size(), extent.first_frame, extent.last_frame, first_time, last_time, frame_offset);
        auto metadata = timeline::TimelineMetadata(source_path.filename().string(), source_path.string());
        if (frame_grid)
        {
            result.document.emplace(std::move(*frame_grid), std::move(source_summary), std::move(metadata));
        }
        else
        {
            result.document.emplace(timebase, std::move(source_summary), std::move(metadata));
        }
    }
    catch (const std::exception &error)
    {
        result.document.reset();
        result.diagnostics.emplace_back("Unable to import ParBeatdown timeline: " + std::string(error.what()));
    }
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

    auto config = Json{};
    if (!read_json_file(source_path, "timeline JSON", config, result.diagnostics))
    {
        return result;
    }

    if (config.is_object() && config.contains("schema"))
    {
        if (!config.at("schema").is_string() || config.at("schema").get<std::string>() != TRACKER_TIMELINE_SCHEMA)
        {
            result.diagnostics.emplace_back("Unsupported timeline JSON schema.");
            return result;
        }
        import_tracker_timeline(source_path, config, options, result);
    }
    else
    {
        import_par_animator(source_path, config, options, result);
    }
    return result;
}

} // namespace timeline_par_animator
