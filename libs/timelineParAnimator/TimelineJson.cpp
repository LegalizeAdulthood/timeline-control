// Copyright (c) 2026 Richard Thomson

#include <timelineParAnimator/TimelineJson.h>

#include "ColorMap.h"
#include "ParameterCatalog.h"

#include <timeline/size_cast.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

namespace timeline_par_animator
{
namespace
{

using Json = nlohmann::json;

constexpr std::array<std::string_view, 8> SUPPORTED_PAR_ANIMATOR_ROOT_FIELDS{
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
constexpr std::string_view BEAT_KEYS_OVERLAY_SCHEMA = "par-beatdown.beat-keys-overlay";

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
    std::ifstream input(path);
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

    bool valid = true;
    for (Json::const_iterator field = config.begin(); field != config.end(); ++field)
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

    const bool has_tracks = config.contains("tracks");
    const bool has_layers = config.contains("layers");
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

int count_keyframes(const Json &value)
{
    int result = 0;
    if (value.is_array())
    {
        for (const Json &element : value)
        {
            result += count_keyframes(element);
        }
    }
    else if (value.is_object())
    {
        for (Json::const_iterator field = value.begin(); field != value.end(); ++field)
        {
            if (field.key() == "keys")
            {
                if (!field->is_array())
                {
                    throw std::invalid_argument("ParAnimator track property 'keys' must be an array.");
                }
                result += timeline::size_cast(*field);
            }
            else
            {
                result += count_keyframes(*field);
            }
        }
    }
    return result;
}

void summarize_tracks(const Json &tracks, int &track_count, int &keyframe_count)
{
    if (!tracks.is_array())
    {
        throw std::invalid_argument("ParAnimator tracks must be an array.");
    }
    for (const Json &track : tracks)
    {
        if (!track.is_object())
        {
            throw std::invalid_argument("Each ParAnimator track must be an object.");
        }
        ++track_count;
        keyframe_count += count_keyframes(track);
    }
}

std::pair<int, int> summarize_par_animator_content(const Json &config)
{
    int track_count = 0;
    int keyframe_count = 0;
    if (config.contains("tracks"))
    {
        summarize_tracks(config.at("tracks"), track_count, keyframe_count);
    }
    else
    {
        for (const Json &layer : config.at("layers"))
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

timeline::Ticks source_frame(const Json &record);

Json animation_catalog(const std::filesystem::path &path, const Json &config, std::vector<std::string> &diagnostics)
{
    if (config.at("parameter-catalogs").empty())
    {
        throw std::invalid_argument("Expected at least one parameter catalog");
    }
    Json result{{"parameters", Json::object()}, {"fractal-types", Json::object()}, {"formula-entries", Json::object()}};
    for (const Json &location : config.at("parameter-catalogs"))
    {
        Json catalog;
        if (!read_json_file(
                path.parent_path() / location.get<std::string>(), "parameter catalog", catalog, diagnostics))
        {
            throw std::invalid_argument("unable to resolve parameter catalog");
        }
        append_parameter_catalog(result, catalog);
    }
    return result;
}

std::string trim_par_line(const std::string &line)
{
    const std::size_t first = line.find_first_not_of(" \t\r\n");
    return first == std::string::npos ? "" : line.substr(first, line.find_last_not_of(" \t\r\n") - first + 1);
}

std::map<std::string, std::string> animation_source(const std::filesystem::path &path, const Json &config)
{
    const Json &source = config.at("source");
    const std::filesystem::path file = path.parent_path() / source.at("file").get<std::string>();
    const std::string name = source.at("name").get<std::string>();
    std::ifstream input(file);
    if (!input)
    {
        throw std::invalid_argument("unable to open PAR source: " + file.string());
    }
    std::map<std::string, std::string> parameters;
    bool in_entry = false;
    bool selected = false;
    std::string line;
    while (std::getline(input, line))
    {
        line = trim_par_line(line);
        while (!line.empty() && line.back() == '\\')
        {
            std::string continuation;
            if (!std::getline(input, continuation))
            {
                throw std::invalid_argument("unterminated PAR source continuation");
            }
            line.pop_back();
            line += trim_par_line(continuation);
        }
        line = trim_par_line(line.substr(0, line.find(';')));
        if (line.empty())
        {
            continue;
        }
        if (!in_entry)
        {
            const std::size_t opening = line.find('{');
            if (opening == std::string::npos)
            {
                throw std::invalid_argument("PAR source entry requires an opening brace");
            }
            selected = trim_par_line(line.substr(0, opening)) == name;
            in_entry = true;
            line.erase(0, opening + 1);
        }
        const std::size_t closing = line.find('}');
        if (selected)
        {
            std::istringstream tokens(line.substr(0, closing));
            std::string token;
            while (tokens >> token)
            {
                const std::size_t equal = token.find('=');
                if (equal != std::string::npos)
                {
                    parameters.emplace(token.substr(0, equal), token.substr(equal + 1));
                }
            }
        }
        if (closing != std::string::npos)
        {
            if (selected)
            {
                return parameters;
            }
            in_entry = false;
        }
    }
    throw std::invalid_argument(
        selected ? "unterminated PAR source entry: " + name : "PAR source entry not found: " + name);
}

int function_slot(std::string_view parameter)
{
    constexpr std::string_view PREFIX = "function[";
    if (parameter.size() <= PREFIX.size() + 1 || parameter.substr(0, PREFIX.size()) != PREFIX ||
        parameter.back() != ']')
    {
        throw std::invalid_argument("invalid function slot");
    }
    const std::string_view text = parameter.substr(PREFIX.size(), parameter.size() - PREFIX.size() - 1);
    int slot = 0;
    const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), slot);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || slot < 0 ||
        slot == std::numeric_limits<int>::max())
    {
        throw std::invalid_argument("invalid function slot");
    }
    return slot;
}

void expand_function_values(Json &metadata)
{
    constexpr std::array<std::string_view, 31> ID_FUNCTIONS{"sin", "cos", "tan", "cotan", "sinh", "cosh", "tanh",
        "cotanh", "exp", "log", "sqr", "recip", "ident", "cosxx", "flip", "conj", "zero", "one", "asin", "asinh",
        "acos", "acosh", "atan", "atanh", "sqrt", "abs", "cabs", "floor", "ceil", "trunc", "round"};
    metadata["values"] = ID_FUNCTIONS;
}

Json function_slot_metadata(const Json &catalog, const std::map<std::string, std::string> &source, int slot)
{
    Json metadata;
    if (source.at("type") == "formula")
    {
        metadata = catalog.at("parameters").at("function");
        if (metadata.at("type") != "function-list")
        {
            throw std::invalid_argument("PWM function slots require function-list metadata");
        }
        metadata["type"] = "enum";
    }
    else
    {
        const Json &types = catalog.at("fractal-types");
        const std::string key = "fn" + std::to_string(slot + 1);
        const std::string &type = source.at("type");
        if (!types.contains(type) || !types.at(type).contains("functions") ||
            !types.at(type).at("functions").contains(key))
        {
            throw std::invalid_argument("PWM function slot is not declared for source type: " + type);
        }
        metadata = types.at(type).at("functions").at(key);
        if (metadata.at("type") != "enum")
        {
            throw std::invalid_argument("PWM function slot metadata must be enum");
        }
    }
    if (metadata.at("values") != "id-functions")
    {
        throw std::invalid_argument("PWM function slot requires id-functions values");
    }
    expand_function_values(metadata);
    metadata["default-curve"] = metadata.value("default-curve", std::string("hold"));
    metadata["extrapolate"] = metadata.value("extrapolate", std::string("clamp"));
    metadata["format"] = "raw";
    return metadata;
}

std::string function_pwm_value(const std::string &base, int slot, const std::string &value)
{
    std::vector<std::string> values;
    std::istringstream input(base);
    std::string component;
    while (std::getline(input, component, '/'))
    {
        values.push_back(component);
    }
    if (timeline::size_cast(values) <= slot)
    {
        values.resize(static_cast<std::size_t>(slot) + 1, "ident");
    }
    values[slot] = value;
    std::string result;
    for (const std::string &item : values)
    {
        if (!result.empty())
        {
            result += '/';
        }
        result += item;
    }
    return result;
}

std::vector<double> animation_value(const Json &value)
{
    if (value.is_number())
    {
        return {value.get<double>()};
    }
    if (value.is_array())
    {
        std::vector<double> components;
        for (const Json &component : value)
        {
            const std::vector<double> parsed = animation_value(component);
            if (timeline::size_cast(parsed) != 1)
            {
                throw std::invalid_argument("numeric array components must be scalar");
            }
            components.push_back(parsed.front());
        }
        return components;
    }
    const std::string text = value.get<std::string>();
    std::vector<double> components;
    std::size_t start = 0;
    do
    {
        const std::size_t end = text.find('/', start);
        const std::size_t length = end == std::string::npos ? text.size() - start : end - start;
        const char *first = text.data() + start;
        double number = 0.0;
        const std::from_chars_result parsed = std::from_chars(first, first + length, number);
        if (parsed.ec != std::errc{} || parsed.ptr != first + length || !std::isfinite(number))
        {
            throw std::invalid_argument("keyframe value is not a finite numeric scalar or tuple");
        }
        components.push_back(number);
        if (end == std::string::npos)
        {
            break;
        }
        start = end + 1;
    } while (start <= text.size());
    return components;
}

Json resolve_animation_target(const Json &catalog, const std::string &parameter,
    const std::map<std::string, std::string> &source, timeline::Attributes &attributes)
{
    Json metadata;
    std::string output = parameter;
    Json original_metadata;
    std::vector<int> slots;
    const std::string type = source.count("type") ? source.at("type") : "";
    const std::string formula = type == "formula" && source.count("formulaname") ? source.at("formulaname") : "";
    std::string member;
    if (!formula.empty() && parameter.substr(0, formula.size() + 1) == formula + ".")
    {
        member = parameter.substr(formula.size() + 1);
    }
    else if (!formula.empty() && parameter.substr(0, formula.size() + 2) == formula + "[\"" &&
        parameter.size() > formula.size() + 4 && parameter.substr(parameter.size() - 2) == "\"]")
    {
        member = parameter.substr(formula.size() + 2, parameter.size() - formula.size() - 4);
    }
    if (parameter.substr(0, 9) == "function[")
    {
        const int slot = function_slot(parameter);
        metadata = function_slot_metadata(catalog, source, slot);
        original_metadata = type == "formula"
            ? catalog.at("parameters").at("function")
            : catalog.at("fractal-types").at(type).at("functions").at("fn" + std::to_string(slot + 1));
        slots = {slot};
        output = "function";
    }
    else if (!member.empty())
    {
        const Json &entries = catalog.at("formula-entries");
        if (!entries.contains(formula))
        {
            throw std::invalid_argument("Unknown formula metadata: " + formula);
        }
        const Json &entry = entries.at(formula);
        if (member.size() == 3 && member.substr(0, 2) == "fn" && member[2] >= '1' && member[2] <= '4')
        {
            if (!entry.contains("functions") || !entry.at("functions").contains(member))
            {
                throw std::invalid_argument("Unknown formula function: " + parameter);
            }
            metadata = entry.at("functions").at(member);
            original_metadata = metadata;
            expand_function_values(metadata);
            metadata["default-curve"] = metadata.value("default-curve", std::string("hold"));
            metadata["extrapolate"] = metadata.value("extrapolate", std::string("clamp"));
            metadata["format"] = "raw";
            slots = {member[2] - '1'};
            output = "function";
        }
        else
        {
            if (!entry.contains("params") || !entry.at("params").contains("knobs") ||
                !entry.at("params").at("knobs").contains(member))
            {
                throw std::invalid_argument("Unknown formula knob: " + parameter);
            }
            metadata = entry.at("params").at("knobs").at(member);
            original_metadata = metadata;
            const std::string variable = metadata.at("variable").get<std::string>();
            const int slot = 2 * (variable[1] - '1');
            slots = metadata.at("type") == "complex" ? std::vector<int>{slot, slot + 1}
                                                     : std::vector<int>{slot + (variable.substr(2) == ".imag")};
            if (metadata.at("type") == "real")
            {
                metadata["type"] = "double";
            }
            output = "params";
        }
    }
    else if (parameter.substr(0, 7) == "params[" || parameter.substr(0, 7) == "params.")
    {
        const Json &types = catalog.at("fractal-types");
        if (!types.contains(type) || !types.at(type).contains("params"))
        {
            throw std::invalid_argument("Unknown params metadata for source type: " + type);
        }
        const Json &params = types.at(type).at("params");
        if (parameter[6] == '[')
        {
            const std::string text = parameter.substr(7, parameter.size() - 8);
            int slot = -1;
            const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), slot);
            if (parameter.back() != ']' || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
            {
                throw std::invalid_argument("invalid params slot: " + parameter);
            }
            if (params.contains("slots"))
            {
                for (const Json &entry : params.at("slots"))
                {
                    if (entry.at("index") == slot)
                    {
                        metadata = entry;
                        break;
                    }
                }
            }
            slots = {slot};
        }
        else
        {
            const std::string group = parameter.substr(7);
            if (params.contains("groups") && params.at("groups").contains(group))
            {
                metadata = params.at("groups").at(group);
                slots = metadata.at("slots").get<std::vector<int>>();
            }
        }
        if (metadata.is_null())
        {
            throw std::invalid_argument("Unknown params target: " + parameter);
        }
        output = "params";
    }
    else
    {
        const Json &parameters = catalog.at("parameters");
        if (!parameters.contains(parameter))
        {
            throw std::invalid_argument("Unknown animated parameter: " + parameter);
        }
        metadata = parameters.at(parameter);
        original_metadata = metadata;
        if (metadata.at("type") == "function-list")
        {
            expand_function_values(metadata);
        }
    }
    if ((output != "function" || slots.empty()) && source.count(output) == 0)
    {
        throw std::invalid_argument("missing source parameter: " + output);
    }
    const std::string base = source.count(output) ? source.at(output) : "ident";
    if (output == "params")
    {
        const std::vector<double> values = animation_value(Json(base));
        const std::string target = metadata.at("type").get<std::string>();
        if (target != "complex" && target != "double" && target != "integer")
        {
            throw std::invalid_argument("unsupported params target type: " + target);
        }
        const int expected = metadata.at("type") == "complex" ? 2 : 1;
        if (timeline::size_cast(slots) != expected)
        {
            throw std::invalid_argument("unsupported params target type or slot arity: " + parameter);
        }
        for (int slot : slots)
        {
            if (slot < 0 || slot >= timeline::size_cast(values))
            {
                throw std::invalid_argument("params slot exceeds source arity: " + parameter);
            }
        }
    }
    std::string canonical_base = base;
    std::transform(canonical_base.begin(), canonical_base.end(), canonical_base.begin(),
        [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c; });
    if (metadata.at("type") == "yes-no" && canonical_base != "yes" && canonical_base != "y" &&
        canonical_base != "true" && canonical_base != "no" && canonical_base != "n" && canonical_base != "false")
    {
        throw std::invalid_argument("invalid yes-no source value");
    }
    attributes["output-parameter"] = output;
    attributes["catalog-source-definition"] = original_metadata.is_null() ? metadata.dump() : original_metadata.dump();
    attributes["source-value"] = base;
    attributes["slots"] = Json(slots).dump();
    if (output == "function" && !slots.empty())
    {
        attributes["slot"] = std::to_string(slots.front());
    }
    return metadata;
}

timeline::KeyframeInterpolation animation_interpolation(std::string_view curve)
{
    if (curve == "linear")
    {
        return timeline::KeyframeInterpolation::LINEAR;
    }
    if (curve == "geometric")
    {
        return timeline::KeyframeInterpolation::GEOMETRIC;
    }
    if (curve == "hold" || curve == "step")
    {
        return timeline::KeyframeInterpolation::HOLD;
    }
    throw std::invalid_argument("unknown interpolation curve: " + std::string(curve));
}

double path_number(const Json &path, const char *name)
{
    const double value = path.at(name).get<double>();
    if (!std::isfinite(value))
    {
        throw std::invalid_argument("path numeric fields must be finite");
    }
    return value;
}

double clean_path_value(double value)
{
    return std::abs(value) < 1e-12 ? 0.0 : value;
}

double planar_path_value(
    double origin, double radius, double radius_change, double phase, double frequency, int component, double fraction)
{
    constexpr double PI = 3.141592653589793238462643383279502884;
    const double radians = (phase + 360.0 * frequency * fraction) * PI / 180.0;
    const double sampled_radius = radius + fraction * radius_change;
    return clean_path_value(origin + sampled_radius * (component == 0 ? std::cos(radians) : std::sin(radians)));
}

void animation_planar_lanes(const Json &path, const Json &metadata, const std::string &id, const std::string &label,
    const std::string &parameter, const std::string &layer, const timeline::FrameGrid &grid,
    const timeline::Attributes &source_attributes, std::vector<timeline::Lane> &lanes)
{
    if (grid.frame_count() < 2)
    {
        throw std::invalid_argument("path tracks require at least two frames");
    }
    const std::string type = metadata.value("type", std::string{});
    if (!type.empty() && type != "complex" && type != "point2")
    {
        throw std::invalid_argument("planar paths require a complex or point2 target");
    }
    const std::vector<double> center = animation_value(path.at("center").get<std::string>());
    if (timeline::size_cast(center) != 2)
    {
        throw std::invalid_argument("planar path center must have two components");
    }
    const std::string kind = path.at("kind").get<std::string>();
    const bool circle = kind == "circle";
    const bool lissajous = kind == "lissajous";
    const bool spiral = kind == "spiral";
    const double x_radius = path_number(path, spiral ? "from-radius" : circle ? "radius" : "x-radius");
    const double y_radius = circle || spiral ? x_radius : path_number(path, "y-radius");
    const double to_radius = spiral ? path_number(path, "to-radius") : x_radius;
    const double phase = path.contains("phase") ? path_number(path, "phase") : 0.0;
    const double turns = !lissajous && path.contains("turns") ? path_number(path, "turns") : 1.0;
    const double x_frequency = lissajous ? path_number(path, "x-frequency") : turns;
    const double y_frequency = lissajous ? path_number(path, "y-frequency") : turns;
    if (x_radius < 0.0 || y_radius < 0.0 || to_radius < 0.0)
    {
        throw std::invalid_argument("path radii must be nonnegative");
    }
    if (lissajous && (x_frequency <= 0.0 || y_frequency <= 0.0))
    {
        throw std::invalid_argument("Lissajous frequencies must be positive");
    }
    const timeline::Time start = grid.offset();
    const timeline::Time end = grid.frame_start(grid.frame_count() - 1);
    for (int component = 0; component < 2; ++component)
    {
        const double origin = center[component];
        const double radius = component == 0 ? x_radius : y_radius;
        const double radius_change = spiral ? to_radius - radius : 0.0;
        const double maximum_radius = spiral ? std::max(radius, to_radius) : radius;
        const double frequency = component == 0 ? x_frequency : y_frequency;
        const double component_phase = lissajous && component == 1 ? 0.0 : phase;
        if (!std::isfinite(360.0 * frequency) || !std::isfinite(component_phase + 360.0 * frequency))
        {
            throw std::invalid_argument("path angle range must be finite");
        }
        const std::string suffix = "[" + std::to_string(component) + "]";
        timeline::Attributes attributes{{"parameter", parameter}, {"layer", layer}, {"track", id},
            {"path", path.dump()}, {"component", std::to_string(component)}};
        attributes.insert(source_attributes.begin(), source_attributes.end());
        const auto evaluate = [origin, radius, radius_change, component_phase, frequency, component, start, end](
                                  timeline::Time time)
        {
            const double fraction =
                static_cast<double>((time - start).ticks()) / static_cast<double>((end - start).ticks());
            return planar_path_value(origin, radius, radius_change, component_phase, frequency, component, fraction);
        };
        timeline::Lane lane(id + suffix, label + suffix, "curve", start, grid.end_time());
        lane.add(timeline::Curve(id + suffix + "-path", "procedural-path", start, end, evaluate, label + suffix,
            clean_path_value(origin - maximum_radius), clean_path_value(origin + maximum_radius), attributes));
        lanes.push_back(std::move(lane));
    }
}

void animation_planar_lanes(const Json &path, const Json &metadata, const std::string &id, const std::string &label,
    const std::string &parameter, const std::string &layer, const timeline::FrameGrid &grid,
    std::vector<timeline::Lane> &lanes)
{
    animation_planar_lanes(path, metadata, id, label, parameter, layer, grid, {}, lanes);
}

int animation_path_arity(const Json &metadata, int inferred_arity)
{
    const std::string type = metadata.value("type", std::string{});
    if (type.empty())
    {
        return inferred_arity;
    }
    if (type == "complex")
    {
        return 2;
    }
    int arity = 0;
    if (type == "point2" || type == "vector2")
    {
        arity = 2;
    }
    else if (type == "point3" || type == "vector3")
    {
        arity = 3;
    }
    else if (type != "numeric-tuple")
    {
        throw std::invalid_argument("control point paths require a complex or numeric tuple target");
    }
    if (type == "numeric-tuple" || metadata.contains("arity"))
    {
        const Json &declared = metadata.at("arity");
        if (!declared.is_number_integer() || declared <= 0 || declared > std::numeric_limits<int>::max())
        {
            throw std::invalid_argument("path target arity must be a positive integer");
        }
        const int declared_arity = declared.get<int>();
        if (arity != 0 && arity != declared_arity)
        {
            throw std::invalid_argument("path target arity does not match its type");
        }
        arity = declared_arity;
    }
    return arity;
}

std::array<double, 4> catmull_rom_points(const std::vector<double> &values, int segment)
{
    const double p1 = values[segment];
    const double p2 = values[segment + 1];
    return {segment == 0 ? 2.0 * p1 - p2 : values[segment - 1], p1, p2,
        segment + 2 < timeline::size_cast(values) ? values[segment + 2] : 2.0 * p2 - p1};
}

double catmull_rom_value(const std::vector<double> &values, double fraction)
{
    const double position = fraction * (timeline::size_cast(values) - 1);
    const int segment = std::min(static_cast<int>(std::floor(position)), timeline::size_cast(values) - 2);
    const double local = position - segment;
    const double local2 = local * local;
    const double local3 = local2 * local;
    const std::array<double, 4> points = catmull_rom_points(values, segment);
    const double p0 = points[0];
    const double p1 = points[1];
    const double p2 = points[2];
    const double p3 = points[3];
    return 0.5 *
        ((2.0 * p1) + (-p0 + p2) * local + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * local2 +
            (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * local3);
}

std::array<double, 4> catmull_rom_hull(const std::vector<double> &values, int segment)
{
    const std::array<double, 4> points = catmull_rom_points(values, segment);
    return {points[1], points[1] + (points[2] - points[0]) / 6.0, points[2] - (points[3] - points[1]) / 6.0, points[2]};
}

std::pair<double, double> catmull_rom_bounds(const std::vector<double> &values)
{
    double minimum = values.front();
    double maximum = minimum;
    for (int segment = 0; segment < timeline::size_cast(values) - 1; ++segment)
    {
        // Equivalent cubic Bezier hulls contain the segment's overshoot.
        const std::array<double, 4> hull = catmull_rom_hull(values, segment);
        for (const double value : hull)
        {
            if (!std::isfinite(value))
            {
                throw std::invalid_argument("Catmull-Rom component bounds must be finite");
            }
            minimum = std::min(minimum, value);
            maximum = std::max(maximum, value);
        }
    }
    return {minimum, maximum};
}

double control_path_value(const std::vector<double> &values, bool catmull_rom, double fraction)
{
    if (catmull_rom)
    {
        return clean_path_value(catmull_rom_value(values, fraction));
    }
    std::vector<double> interpolated(values);
    // De Casteljau reduces the owned control points to the curve value.
    for (int order = timeline::size_cast(interpolated) - 1; order > 0; --order)
    {
        for (int point = 0; point < order; ++point)
        {
            interpolated[point] += fraction * (interpolated[point + 1] - interpolated[point]);
        }
    }
    return clean_path_value(interpolated.front());
}

std::vector<double> normalize_vector(std::vector<double> values)
{
    double squared_length = 0;
    for (double value : values)
    {
        if (!std::isfinite(value))
        {
            throw std::invalid_argument("vector normalization requires finite components");
        }
        squared_length += value * value;
    }
    if (squared_length == 0)
    {
        throw std::invalid_argument("vector normalization requires a nonzero squared length");
    }
    // Match ParAnimator: finite components divided by an overflowing length yield zeros.
    const double length = std::sqrt(squared_length);
    for (double &value : values)
    {
        value /= length;
    }
    return values;
}

std::vector<double> normalize_path_vector(std::vector<double> values)
{
    for (double &value : values)
    {
        value = clean_path_value(value);
    }
    values = normalize_vector(std::move(values));
    for (double &value : values)
    {
        value = clean_path_value(value);
    }
    return values;
}

void validate_vector_hull(const std::vector<std::vector<double>> &points, int depth, int &remaining)
{
    if (--remaining < 0)
    {
        throw std::invalid_argument("vector normalization exceeds the control-hull validation limit");
    }
    static_cast<void>(normalize_path_vector(points.front()));
    static_cast<void>(normalize_path_vector(points.back()));
    bool safe = false;
    const int arity = timeline::size_cast(points.front());
    for (int component = 0; component < arity; ++component)
    {
        double minimum = points.front()[component];
        double maximum = minimum;
        for (const std::vector<double> &point : points)
        {
            if (!std::isfinite(point[component]))
            {
                throw std::invalid_argument("vector normalization requires finite control hulls");
            }
            minimum = std::min(minimum, point[component]);
            maximum = std::max(maximum, point[component]);
        }
        const double magnitude = std::max(std::abs(minimum), std::abs(maximum));
        const double margin =
            1e-12 + 64 * std::numeric_limits<double>::epsilon() * magnitude * timeline::size_cast(points);
        safe = safe || minimum > margin || maximum < -margin;
    }
    if (safe)
    {
        return;
    }
    if (depth >= 52)
    {
        throw std::invalid_argument(
            "vector normalization cannot exclude a singular interval within the validation limit");
    }
    // Subdivision encloses the entire path, including zeros between frame times.
    std::vector<std::vector<double>> values(points);
    std::vector<std::vector<double>> left{points.front()};
    std::vector<std::vector<double>> right{points.back()};
    for (int order = timeline::size_cast(values) - 1; order > 0; --order)
    {
        for (int point = 0; point < order; ++point)
        {
            for (int component = 0; component < arity; ++component)
            {
                values[point][component] = 0.5 * values[point][component] + 0.5 * values[point + 1][component];
            }
        }
        left.push_back(values.front());
        right.push_back(values[order - 1]);
    }
    std::reverse(right.begin(), right.end());
    validate_vector_hull(left, depth + 1, remaining);
    validate_vector_hull(right, depth + 1, remaining);
}

void validate_vector_control_path(const std::vector<std::vector<double>> &points,
    const std::vector<std::vector<double>> &components, bool catmull_rom)
{
    int remaining = 100000;
    if (!catmull_rom)
    {
        validate_vector_hull(points, 0, remaining);
        return;
    }
    for (int segment = 0; segment < timeline::size_cast(points) - 1; ++segment)
    {
        std::vector<std::vector<double>> hull(4);
        for (const std::vector<double> &values : components)
        {
            const std::array<double, 4> bounds = catmull_rom_hull(values, segment);
            for (int point = 0; point < 4; ++point)
            {
                hull[point].push_back(bounds[point]);
            }
        }
        validate_vector_hull(hull, 0, remaining);
    }
}

void animation_control_point_lanes(const Json &track, const Json &metadata, const std::string &id,
    const std::string &label, const std::string &parameter, const std::string &layer, const timeline::FrameGrid &grid,
    const timeline::Attributes &source_attributes, std::vector<timeline::Lane> &lanes)
{
    if (grid.frame_count() < 2)
    {
        throw std::invalid_argument("path tracks require at least two frames");
    }
    const Json &path = track.at("path");
    const bool catmull_rom = path.at("kind").get<std::string>() == "catmull-rom";
    const std::string path_name = catmull_rom ? "Catmull-Rom" : "Bezier";
    const std::string type = metadata.value("type", std::string{});
    const bool normalize = (type == "vector2" || type == "vector3") && metadata.value("normalize", false);
    const Json &points = path.at("control-points");
    if (!points.is_array() || timeline::size_cast(points) < (catmull_rom ? 4 : 2))
    {
        throw std::invalid_argument(
            path_name + " paths require at least " + (catmull_rom ? "four" : "two") + " control points");
    }
    std::vector<std::vector<double>> control_points;
    for (const Json &point : points)
    {
        control_points.push_back(animation_value(point.get<std::string>()));
    }
    const int arity = animation_path_arity(metadata, timeline::size_cast(control_points.front()));
    for (const std::vector<double> &point : control_points)
    {
        if (timeline::size_cast(point) != arity)
        {
            throw std::invalid_argument(path_name + " control point arity does not match its target");
        }
    }
    const timeline::Time start = grid.offset();
    const timeline::Time end = grid.frame_start(grid.frame_count() - 1);
    std::vector<std::vector<double>> component_values(arity);
    for (const std::vector<double> &point : control_points)
    {
        for (int component = 0; component < arity; ++component)
        {
            component_values[component].push_back(point[component]);
        }
    }
    if (normalize)
    {
        validate_vector_control_path(control_points, component_values, catmull_rom);
    }
    for (int component = 0; component < arity; ++component)
    {
        const std::vector<double> &values = component_values[component];
        const auto [minimum, maximum] = std::minmax_element(values.begin(), values.end());
        const std::pair<double, double> bounds = normalize ? std::pair<double, double>{-1, 1}
            : catmull_rom                                  ? catmull_rom_bounds(values)
                                                           : std::pair<double, double>{*minimum, *maximum};
        const std::string suffix = arity == 1 ? "" : "[" + std::to_string(component) + "]";
        timeline::Attributes attributes{{"parameter", parameter}, {"layer", layer}, {"track", id},
            {"path", path.dump()}, {"component", std::to_string(component)}};
        attributes.insert(source_attributes.begin(), source_attributes.end());
        timeline::CurveEvaluator evaluate = [values, start, end, catmull_rom](timeline::Time time)
        {
            const double fraction =
                static_cast<double>((time - start).ticks()) / static_cast<double>((end - start).ticks());
            return control_path_value(values, catmull_rom, fraction);
        };
        if (normalize)
        {
            attributes["normalize"] = "true";
            attributes["catalog-definition"] = metadata.dump();
            attributes["track-definition"] = track.dump();
            evaluate = [component_values, component, start, end, catmull_rom](timeline::Time time)
            {
                const double fraction =
                    static_cast<double>((time - start).ticks()) / static_cast<double>((end - start).ticks());
                std::vector<double> sampled;
                for (const std::vector<double> &values : component_values)
                {
                    sampled.push_back(control_path_value(values, catmull_rom, fraction));
                }
                return normalize_path_vector(std::move(sampled))[component];
            };
        }
        timeline::Lane lane(id + suffix, label + suffix, "curve", start, grid.end_time());
        lane.add(timeline::Curve(id + suffix + "-path", "procedural-path", start, end, evaluate, label + suffix,
            clean_path_value(bounds.first), clean_path_value(bounds.second), attributes));
        lanes.push_back(std::move(lane));
    }
}

void animation_control_point_lanes(const Json &track, const Json &metadata, const std::string &id,
    const std::string &label, const std::string &parameter, const std::string &layer, const timeline::FrameGrid &grid,
    std::vector<timeline::Lane> &lanes)
{
    animation_control_point_lanes(track, metadata, id, label, parameter, layer, grid, {}, lanes);
}

Json animation_path_keys(const Json &path, const timeline::FrameGrid &grid)
{
    if (!path.is_object())
    {
        throw std::invalid_argument("track path must be an object");
    }
    if (grid.frame_count() < 2)
    {
        throw std::invalid_argument("path tracks require at least two frames");
    }
    const std::string kind = path.at("kind").get<std::string>();
    if (kind != "constant" && kind != "line")
    {
        throw std::invalid_argument("unsupported procedural path kind: " + kind);
    }
    const std::string from = path.at(kind == "constant" ? "value" : "from").get<std::string>();
    const std::string to = kind == "constant" ? from : path.at("to").get<std::string>();
    const std::string curve = kind == "constant" ? "hold" : "linear";
    // ParAnimator uses endpoint keys for these exact analytic definitions.
    return Json::array({Json{{"frame", 0}, {"value", from}, {"curve", curve}},
        Json{{"frame", grid.frame_count() - 1}, {"value", to}, {"curve", curve}}});
}

void integer_output(timeline::Lane &lane, const Json &metadata)
{
    const timeline::Keyframe &first = std::get<timeline::Keyframe>(lane.items().front());
    if (first.attributes().count("output-rounding") == 0)
    {
        return;
    }
    const timeline::Time start = first.time();
    const timeline::Time end = std::get<timeline::Keyframe>(lane.items().back()).time();
    const bool base = metadata.value("extrapolate", std::string("clamp")) == "base";
    lane.set_keyframe_output_evaluator([start, end, base](timeline::Time time, double value)
        { return base && (time < start || end < time) ? value : std::round(value); });
}

void validate_catalog_key_target(const Json &track, const Json &metadata)
{
    const std::string target = metadata.value("type", std::string{});
    if (target == "miim" || target == "potential" || target == "numeric-tuple-or-enum" || target == "color-map")
    {
        throw std::invalid_argument("unsupported generic target type: " + target);
    }
    const bool tuple = target == "numeric-tuple" || target == "point2" || target == "point3" || target == "vector2" ||
        target == "vector3";
    const bool normalized = (target == "vector2" || target == "vector3") && metadata.value("normalize", false);
    const bool integer_tuple = target == "integer-tuple";
    const bool scalar = target == "double" || target == "integer" || target == "integer-or-enum";
    if (track.contains("path") && (tuple || integer_tuple) && !normalized)
    {
        throw std::invalid_argument("constant and line tuple paths cannot supply ParAnimator's numeric array keys");
    }
    if (!track.contains("path") && target != "center-mag" && target != "corners" && !metadata.contains("default-curve"))
    {
        throw std::invalid_argument(normalized ? "vector normalization requires a catalog default-curve"
                                               : "keyed target requires a catalog default-curve");
    }
    if (track.contains("path"))
    {
        return;
    }
    const Json &keys = track.at("keys");
    if (!keys.is_array() || keys.empty())
    {
        throw std::invalid_argument("track keys must be a nonempty array");
    }
    for (const Json &key : keys)
    {
        const Json &value = key.at("value");
        const bool named = value.is_string() && metadata.contains("values") &&
            std::find(metadata.at("values").begin(), metadata.at("values").end(), value) != metadata.at("values").end();
        if (target == "yes-no" || target == "string" || target == "enum" || target == "inside" || target == "outside" ||
            target == "function-list" || (target == "integer-or-enum" && named))
        {
            bool valid = target == "yes-no" ? value.is_boolean() : target == "string" ? value.is_string() : named;
            if (target == "function-list")
            {
                valid = value.is_array() && !value.empty() &&
                    std::all_of(value.begin(), value.end(),
                        [&](const Json &entry)
                        {
                            return entry.is_string() &&
                                std::find(metadata.at("values").begin(), metadata.at("values").end(), entry) !=
                                metadata.at("values").end();
                        });
            }
            if ((target == "inside" || target == "outside") && value.is_string() && !named)
            {
                const std::vector<double> components = animation_value(value);
                valid = timeline::size_cast(components) == 1 && components[0] == std::trunc(components[0]) &&
                    components[0] >= std::numeric_limits<int>::min() &&
                    components[0] <= std::numeric_limits<int>::max() &&
                    (!metadata.contains("min") || components[0] >= metadata.at("min").get<double>()) &&
                    (!metadata.contains("max") || components[0] <= metadata.at("max").get<double>());
            }
            if (!valid)
            {
                throw std::invalid_argument("invalid catalog-declared " + target + " key value");
            }
            continue;
        }
        if (target == "integer-or-enum" && !value.is_number_integer())
        {
            throw std::invalid_argument("integer target requires JSON integer key values");
        }
        if (target == "integer" && value.is_string())
        {
            const std::string text = value.get<std::string>();
            std::size_t consumed = 0;
            try
            {
                static_cast<void>(std::stoi(text, &consumed));
            }
            catch (const std::exception &)
            {
                throw std::invalid_argument("integer output requires integral key values in int range");
            }
            if (consumed != text.size())
            {
                throw std::invalid_argument("integer output requires integral key values in int range");
            }
        }
        if (target == "double" && !value.is_number() && !value.is_string())
        {
            throw std::invalid_argument("double target requires numeric scalar key values");
        }
        if ((target == "complex" || target == "center-mag" || target == "corners") && !value.is_string())
        {
            throw std::invalid_argument(target + " target requires slash-delimited string key values");
        }
        const std::vector<double> values = animation_value(value);
        const int components = timeline::size_cast(values);
        if ((target == "complex" && components != 2) ||
            (target == "center-mag" && (components < 3 || components > 6)) ||
            (target == "corners" && components != 4 && components != 6))
        {
            throw std::invalid_argument("keyframe arity does not match its catalog target");
        }
        if (tuple && components != animation_path_arity(metadata, components))
        {
            throw std::invalid_argument("keyframe arity does not match its catalog target");
        }
        if (target == "double" && components != 1)
        {
            throw std::invalid_argument("double target requires a numeric scalar key value");
        }
        if ((tuple || integer_tuple) &&
            (!key.at("value").is_array() ||
                !std::all_of(key.at("value").begin(), key.at("value").end(),
                    [](const Json &value) { return value.is_number(); })))
        {
            throw std::invalid_argument("tuple targets require numeric array key values");
        }
        for (double value : values)
        {
            if ((metadata.contains("min") && value < metadata.at("min").get<double>()) ||
                (metadata.contains("max") && value > metadata.at("max").get<double>()))
            {
                throw std::invalid_argument("key value exceeds catalog bounds");
            }
        }
    }
    const std::string curve = keys.back().value("curve", metadata.value("default-curve", std::string("linear")));
    if ((tuple || scalar || integer_tuple || target == "complex") &&
        animation_interpolation(curve) == timeline::KeyframeInterpolation::GEOMETRIC)
    {
        throw std::invalid_argument("numeric targets support linear, hold, or step curves");
    }
}

void animation_key_lanes(const Json &track, const Json &metadata, const std::string &id, const std::string &label,
    const std::string &parameter, const std::string &layer, const timeline::FrameGrid &grid,
    const timeline::Attributes &source_attributes, std::vector<timeline::Lane> &lanes)
{
    const Json keys = track.contains("path") ? animation_path_keys(track.at("path"), grid) : track.at("keys");
    if (!keys.is_array() || keys.empty())
    {
        throw std::invalid_argument("track keys must be a nonempty array");
    }
    std::vector<std::vector<double>> values;
    const std::string target = metadata.value("type", std::string{});
    bool categorical = target == "enum" || target == "string" || target == "yes-no" || target == "inside" ||
        target == "outside" || target == "function-list" ||
        (target == "integer-or-enum" &&
            std::any_of(keys.begin(), keys.end(), [](const Json &key) { return key.at("value").is_string(); }));
    timeline::Ticks previous = -1;
    for (const Json &key : keys)
    {
        const timeline::Ticks frame = source_frame(key);
        if (frame <= previous || frame >= grid.frame_count())
        {
            throw std::invalid_argument("key frames must be strictly increasing and within num-frames");
        }
        previous = frame;
        if (categorical)
        {
            continue;
        }
        try
        {
            values.push_back(animation_value(key.at("value")));
        }
        catch (const std::exception &)
        {
            const Json &value = key.at("value");
            const bool named_value = value.is_string() && metadata.contains("values") &&
                (metadata.at("values").is_string() ||
                    std::find(metadata.at("values").begin(), metadata.at("values").end(), value) !=
                        metadata.at("values").end());
            const std::string type = metadata.value("type", std::string{});
            if (!named_value && type != "string" && type != "function-list" && type != "yes-no")
            {
                throw;
            }
            categorical = true;
            values.emplace_back();
        }
    }
    if (categorical)
    {
        timeline::Lane lane(id, label, "keyframes", grid.offset(), grid.end_time());
        for (int index = 0; index < timeline::size_cast(keys); ++index)
        {
            const Json &key = keys[index];
            const std::string curve = key.value("curve", metadata.value("default-curve", std::string("hold")));
            const std::string outgoing = index + 1 < timeline::size_cast(keys)
                ? keys[index + 1].value("curve", metadata.value("default-curve", std::string("hold")))
                : "hold";
            if (animation_interpolation(curve) != timeline::KeyframeInterpolation::HOLD ||
                animation_interpolation(outgoing) != timeline::KeyframeInterpolation::HOLD)
            {
                throw std::invalid_argument("categorical values require hold or step interpolation");
            }
            std::string value =
                key.at("value").is_string() ? key.at("value").get<std::string>() : key.at("value").dump();
            if (target == "yes-no" && key.at("value").is_boolean())
            {
                value = key.at("value").get<bool>() ? "yes" : "no";
            }
            else if (target == "function-list")
            {
                value.clear();
                for (const Json &entry : key.at("value"))
                {
                    value += (value.empty() ? "" : "/") + entry.get<std::string>();
                }
            }
            if (source_attributes.count("slot"))
            {
                value = function_pwm_value(
                    source_attributes.at("source-value"), std::stoi(source_attributes.at("slot")), value);
            }
            const timeline::Time start = grid.frame_start(source_frame(key));
            const timeline::Time end = index + 1 < timeline::size_cast(keys)
                ? grid.frame_start(source_frame(keys[index + 1]))
                : grid.end_time();
            timeline::Attributes attributes{{"parameter", parameter}, {"layer", layer}, {"value", value},
                {"curve", curve}, {"outgoing-curve", outgoing}, {"track", id}};
            attributes.insert(source_attributes.begin(), source_attributes.end());
            attributes["source-key"] = key.dump();
            attributes["catalog-definition"] = metadata.dump();
            attributes["track-definition"] = track.dump();
            if (track.contains("path"))
            {
                attributes["path"] = track.at("path").dump();
            }
            const std::string key_id = id + "-key-" + std::to_string(index);
            lane.add(timeline::Instant(key_id, "keyframe", start, value, std::nullopt, attributes));
            lane.add(timeline::Interval(key_id + "-hold", "keyframe-value", index == 0 ? grid.offset() : start, end,
                value, std::nullopt, attributes));
        }
        lanes.push_back(std::move(lane));
        return;
    }
    if (target == "center-mag")
    {
        int arity = 3;
        for (const std::vector<double> &value : values)
        {
            arity = std::max(arity, timeline::size_cast(value));
        }
        for (std::vector<double> &value : values)
        {
            const int authored_arity = timeline::size_cast(value);
            value.resize(arity, 0);
            if (arity > 3 && (authored_arity <= 3 || value[3] == 0))
            {
                value[3] = 1;
            }
        }
    }
    for (const std::vector<double> &value : values)
    {
        if (value.empty() || timeline::size_cast(value) != timeline::size_cast(values.front()))
        {
            throw std::invalid_argument("keyframe component counts must agree");
        }
    }
    const int components = timeline::size_cast(values.front());
    const std::string type = metadata.value("type", std::string{});
    const bool integer = type == "integer" || type == "integer-tuple" ||
        (type == "integer-or-enum" &&
            std::all_of(keys.begin(), keys.end(), [](const Json &key) { return key.at("value").is_number_integer(); }));
    if (integer)
    {
        if ((type != "integer-tuple" && components != 1) ||
            (type == "integer-tuple" && components != metadata.at("arity").get<int>()))
        {
            throw std::invalid_argument("integer output key arity does not match its target");
        }
        for (const std::vector<double> &value : values)
        {
            for (double scalar : value)
            {
                if (scalar != std::trunc(scalar) || scalar < std::numeric_limits<int>::min() ||
                    scalar > std::numeric_limits<int>::max())
                {
                    throw std::invalid_argument("integer output requires integral key values in int range");
                }
                if ((metadata.contains("min") && scalar < metadata.at("min").get<double>()) ||
                    (metadata.contains("max") && scalar > metadata.at("max").get<double>()))
                {
                    throw std::invalid_argument("integer output key exceeds catalog bounds");
                }
            }
        }
    }
    for (int component = 0; component < components; ++component)
    {
        const std::string suffix = components == 1 ? "" : "[" + std::to_string(component) + "]";
        timeline::Lane lane(id + suffix, label + suffix, "keyframes", grid.offset(), grid.end_time());
        for (int index = 0; index < timeline::size_cast(keys); ++index)
        {
            const Json &key = keys[index];
            const std::string authored_curve =
                key.value("curve", metadata.value("default-curve", std::string("linear")));
            std::string outgoing_curve = index + 1 < timeline::size_cast(keys)
                ? keys[index + 1].value("curve", metadata.value("default-curve", std::string("linear")))
                : "hold";
            static_cast<void>(animation_interpolation(authored_curve));
            if (target == "center-mag")
            {
                outgoing_curve = index + 1 < timeline::size_cast(keys) && (component == 2 || component == 3) &&
                        values[index][component] > 0 && values[index + 1][component] > 0
                    ? "geometric"
                    : "linear";
            }
            if (target == "corners")
            {
                outgoing_curve = "linear";
            }
            timeline::Attributes attributes{{"parameter", parameter}, {"layer", layer},
                {"value", key.at("value").dump()}, {"curve", authored_curve}, {"outgoing-curve", outgoing_curve},
                {"track", id}};
            attributes.insert(source_attributes.begin(), source_attributes.end());
            attributes["component"] = std::to_string(component);
            attributes["arity"] = std::to_string(components);
            attributes["catalog-definition"] = metadata.dump();
            attributes["track-definition"] = track.dump();
            if (track.contains("path"))
            {
                attributes["path"] = track.at("path").dump();
            }
            if (key.at("value").is_string())
            {
                attributes["value"] = key.at("value").get<std::string>();
            }
            if (integer)
            {
                if (animation_interpolation(outgoing_curve) == timeline::KeyframeInterpolation::GEOMETRIC)
                {
                    throw std::invalid_argument("integer output supports linear, hold, or step curves");
                }
                attributes["output-rounding"] = "nearest-half-away-from-zero";
                attributes["catalog-definition"] = metadata.dump();
                attributes["track-definition"] = track.dump();
            }
            lane.add(timeline::Keyframe(id + "-key-" + std::to_string(index), grid.frame_start(source_frame(key)),
                values[index][component], animation_interpolation(outgoing_curve), std::move(attributes)));
        }
        integer_output(lane, metadata);
        lanes.push_back(std::move(lane));
    }
}

void animation_key_lanes(const Json &track, const Json &metadata, const std::string &id, const std::string &label,
    const std::string &parameter, const std::string &layer, const timeline::FrameGrid &grid,
    std::vector<timeline::Lane> &lanes)
{
    animation_key_lanes(track, metadata, id, label, parameter, layer, grid, {}, lanes);
}

void validate_keyed_vector_segment(const std::vector<double> &from, const std::vector<double> &to)
{
    double scale = 0;
    for (int component = 0; component < timeline::size_cast(from); ++component)
    {
        scale = std::max({scale, std::abs(from[component]), std::abs(to[component])});
    }
    double projection = 0;
    double squared_delta = 0;
    for (int component = 0; component < timeline::size_cast(from); ++component)
    {
        const double origin = from[component] / scale;
        const double delta = to[component] / scale - origin;
        projection += origin * delta;
        squared_delta += delta * delta;
    }
    const double fraction = squared_delta == 0 ? 0 : std::clamp(-projection / squared_delta, 0.0, 1.0);
    double squared_distance = 0;
    std::vector<double> closest;
    for (int component = 0; component < timeline::size_cast(from); ++component)
    {
        const double value = from[component] + fraction * (to[component] - from[component]);
        closest.push_back(value);
        const double scaled = value / scale;
        squared_distance += scaled * scaled;
    }
    // Reject unresolved near-zero intervals as well as exact off-grid crossings.
    const double margin = 64 * std::numeric_limits<double>::epsilon();
    if (squared_distance <= margin * margin)
    {
        throw std::invalid_argument("vector normalization has a singular or unresolved linear interval");
    }
    static_cast<void>(normalize_vector(std::move(closest)));
}

void normalized_keyed_output(
    const Json &track, const Json &metadata, const timeline::FrameGrid &grid, std::vector<timeline::Lane> &lanes)
{
    if (track.contains("path"))
    {
        throw std::invalid_argument(
            "vector normalization requires numeric array keys; constant and line paths are unsupported by ParAnimator");
    }
    const Json &keys = track.at("keys");
    if (timeline::size_cast(keys) != 2 || source_frame(keys[0]) != 0 || source_frame(keys[1]) != grid.frame_count() - 1)
    {
        throw std::invalid_argument("vector normalization requires two keys spanning the full frame range");
    }
    const int arity = animation_path_arity(metadata, timeline::size_cast(lanes));
    for (const Json &key : keys)
    {
        const Json &value = key.at("value");
        if (!value.is_array() ||
            !std::all_of(value.begin(), value.end(), [](const Json &item) { return item.is_number(); }))
        {
            throw std::invalid_argument("vector normalization requires numeric array key values");
        }
        if (timeline::size_cast(value) != arity)
        {
            throw std::invalid_argument("vector normalization key arity does not match its target");
        }
        const std::vector<double> parsed = animation_value(value);
        static_cast<void>(normalize_vector(parsed));
        for (double component : parsed)
        {
            if ((metadata.contains("min") && component < metadata.at("min").get<double>()) ||
                (metadata.contains("max") && component > metadata.at("max").get<double>()))
            {
                throw std::invalid_argument("vector normalization key exceeds catalog bounds");
            }
        }
    }
    const timeline::KeyframeInterpolation interpolation =
        std::get<timeline::Keyframe>(lanes[0].items()[0]).interpolation();
    if (interpolation == timeline::KeyframeInterpolation::GEOMETRIC)
    {
        throw std::invalid_argument("vector normalization supports linear, hold, or step curves");
    }
    if (interpolation == timeline::KeyframeInterpolation::LINEAR)
    {
        validate_keyed_vector_segment(animation_value(keys[0].at("value")), animation_value(keys[1].at("value")));
    }
    const std::vector<timeline::Lane> authored = lanes;
    for (int component = 0; component < timeline::size_cast(lanes); ++component)
    {
        timeline::Lane &lane = lanes[component];
        timeline::Lane decorated(lane.id(), lane.label(), lane.kind(), lane.start(), lane.end());
        for (const timeline::Item &item : lane.items())
        {
            const timeline::Keyframe &key = std::get<timeline::Keyframe>(item);
            timeline::Attributes attributes = key.attributes();
            attributes["normalize"] = "true";
            attributes["component"] = std::to_string(component);
            attributes["catalog-definition"] = metadata.dump();
            attributes["track-definition"] = track.dump();
            decorated.add(
                timeline::Keyframe(key.id(), key.time(), key.value(), key.interpolation(), std::move(attributes)));
        }
        decorated.set_keyframe_output_evaluator(
            [authored, component](timeline::Time time, double)
            {
                std::vector<double> values;
                for (const timeline::Lane &input : authored)
                {
                    values.push_back(*input.evaluate_keyframes(time));
                }
                return normalize_vector(std::move(values))[component];
            });
        lane = std::move(decorated);
    }
}

double pwm_mix(const Json &key)
{
    if (key.contains("mix") == key.contains("duty"))
    {
        throw std::invalid_argument("PWM key requires exactly one of mix and duty");
    }
    const Json &value = key.at(key.contains("mix") ? "mix" : "duty");
    if (!value.is_number())
    {
        throw std::invalid_argument("PWM mix must be numeric");
    }
    const double mix = value.get<double>();
    if (!std::isfinite(mix) || mix < 0.0 || mix > 1.0)
    {
        throw std::invalid_argument("PWM mix must be finite and in the range 0 through 1");
    }
    return mix;
}

std::string pwm_endpoint(
    const Json &track, const Json &metadata, const std::string &name, const std::string &alias, bool default_value)
{
    if (track.contains(name) && track.contains(alias))
    {
        throw std::invalid_argument("PWM endpoint cannot contain both " + name + " and " + alias);
    }
    const bool specified = track.contains(name) || track.contains(alias);
    const std::string type = metadata.at("type").get<std::string>();
    if (type == "yes-no")
    {
        if (!specified)
        {
            return default_value ? "yes" : "no";
        }
        const Json &value = track.at(track.contains(name) ? name : alias);
        if (!value.is_boolean())
        {
            throw std::invalid_argument("PWM yes-no endpoints must be boolean");
        }
        return value.get<bool>() ? "yes" : "no";
    }
    if (!specified)
    {
        throw std::invalid_argument("PWM discrete targets require both endpoints");
    }
    const std::string value = track.at(track.contains(name) ? name : alias).get<std::string>();
    const Json &values = metadata.at("values");
    if (!values.is_array())
    {
        throw std::invalid_argument("PWM named value sets are not supported");
    }
    if (std::find(values.begin(), values.end(), value) != values.end())
    {
        return value;
    }
    if (type == "inside" || type == "outside")
    {
        std::size_t length = 0;
        const int number = std::stoi(value, &length);
        if (length == value.size() && (!metadata.contains("min") || number >= metadata.at("min").get<double>()) &&
            (!metadata.contains("max") || number <= metadata.at("max").get<double>()))
        {
            return value;
        }
    }
    throw std::invalid_argument("invalid PWM discrete endpoint: " + value);
}

void animation_pwm_lanes(const Json &track, const Json &metadata, const std::string &id, const std::string &label,
    const std::string &parameter, const std::string &layer, const timeline::FrameGrid &grid,
    const timeline::Attributes &source_attributes, std::vector<timeline::Lane> &lanes)
{
    if (parameter.find('[') != std::string::npos && source_attributes.count("slot") == 0)
    {
        throw std::invalid_argument("PWM output slots are not supported");
    }
    const std::string type = metadata.value("type", std::string{});
    if (type != "yes-no" && type != "enum" && type != "inside" && type != "outside" && type != "integer-or-enum")
    {
        throw std::invalid_argument("PWM requires a catalog-declared discrete target");
    }
    const Json &window_value = track.at("window");
    if (!window_value.is_number_integer() || window_value < 2 || window_value > std::numeric_limits<int>::max())
    {
        throw std::invalid_argument("PWM window must be an integer of at least 2");
    }
    const int window = window_value.get<int>();
    const Json &keys = track.at("keys");
    if (!keys.is_array() || timeline::size_cast(keys) != 2 || grid.frame_count() < 2)
    {
        throw std::invalid_argument("PWM requires exactly two keys and at least two frames");
    }
    if (source_frame(keys[0]) != 0 || source_frame(keys[1]) != grid.frame_count() - 1)
    {
        throw std::invalid_argument("PWM keys must span the full frame range");
    }
    const double from_mix = pwm_mix(keys[0]);
    const double to_mix = pwm_mix(keys[1]);
    const std::string a = pwm_endpoint(track, metadata, "a", "off", false);
    const std::string b = pwm_endpoint(track, metadata, "b", "on", true);
    const bool slotted = source_attributes.count("slot") != 0;
    const int slot = slotted ? std::stoi(source_attributes.at("slot")) : 0;
    const std::string output_a = slotted ? function_pwm_value(source_attributes.at("source-value"), slot, a) : a;
    const std::string output_b = slotted ? function_pwm_value(source_attributes.at("source-value"), slot, b) : b;
    timeline::Attributes attributes{{"parameter", parameter}, {"layer", layer}, {"track", id}, {"mode", "pwm"},
        {"pwm", track.dump()}, {"a", a}, {"b", b}, {"window", std::to_string(window)}};
    attributes.insert(source_attributes.begin(), source_attributes.end());
    timeline::Lane output(id, label, "events", grid.offset(), grid.end_time());
    timeline::Ticks run_start = 0;
    std::string previous;
    const auto append_run = [&](timeline::Ticks end_frame)
    {
        timeline::Attributes run_attributes(attributes);
        run_attributes["value"] = previous;
        run_attributes["signal"] = "output";
        const timeline::Time end = end_frame == grid.frame_count() ? grid.end_time() : grid.frame_start(end_frame);
        output.add(timeline::Interval(id + "-pwm-" + std::to_string(run_start), "pwm-output",
            grid.frame_start(run_start), end, previous, std::nullopt, std::move(run_attributes)));
    };
    for (timeline::Ticks frame = 0; frame < grid.frame_count(); ++frame)
    {
        const double fraction = static_cast<double>(frame) / static_cast<double>(grid.frame_count() - 1);
        const double mix = frame == grid.frame_count() - 1 ? to_mix : from_mix + fraction * (to_mix - from_mix);
        const int b_count = static_cast<int>(std::lround(mix * window));
        const std::string &value = frame % window < b_count ? output_b : output_a;
        if (frame > 0 && value != previous)
        {
            append_run(frame);
            run_start = frame;
        }
        previous = value;
    }
    append_run(grid.frame_count());
    timeline::Lane mix_lane(id + "-mix", label + " / mix", "keyframes", grid.offset(), grid.end_time());
    for (int index = 0; index < 2; ++index)
    {
        timeline::Attributes mix_attributes(attributes);
        mix_attributes["signal"] = "mix";
        mix_attributes["source-key"] = keys[index].dump();
        mix_lane.add(
            timeline::Keyframe(id + "-mix-key-" + std::to_string(index), grid.frame_start(source_frame(keys[index])),
                index == 0 ? from_mix : to_mix, timeline::KeyframeInterpolation::LINEAR, std::move(mix_attributes)));
    }
    lanes.push_back(std::move(output));
    lanes.push_back(std::move(mix_lane));
}

void camera2d_key_lanes(const Json &signal, const std::string &member, const std::string &type, const std::string &id,
    const std::string &label, const std::string &layer, const timeline::FrameGrid &grid,
    std::vector<timeline::Lane> &lanes)
{
    if ((member == "view-up" || member == "height" || member == "skew") && signal.contains("path"))
    {
        throw std::invalid_argument(
            "Camera2D " + member + " requires keyed input in the ParAnimator format; paths are not allowed");
    }
    if (member == "view-up" && signal.contains("normalize") && signal.at("normalize") != true)
    {
        throw std::invalid_argument("Camera2D view-up normalize must be true in the ParAnimator format");
    }
    if (signal.at("type") != type || (signal.contains("path") && (type != "point2" || signal.contains("keys"))))
    {
        throw std::invalid_argument("Camera2D " + member + " requires unambiguous " + type + " input");
    }
    const Json keys = signal.contains("path") ? animation_path_keys(signal.at("path"), grid) : signal.at("keys");
    if (!keys.is_array() || keys.size() != 2 || source_frame(keys[0]) != 0 ||
        source_frame(keys[1]) != grid.frame_count() - 1)
    {
        throw std::invalid_argument("Camera2D " + member + " requires two keys spanning the full frame range");
    }
    const int arity = type == "double" ? 1 : 2;
    for (const Json &key : keys)
    {
        const Json &value = key.at("value");
        if ((member == "height" || member == "skew") && !value.is_number())
        {
            throw std::invalid_argument("Camera2D " + member + " requires a JSON number in the ParAnimator format");
        }
        if (!value.is_string() && (arity != 1 || !value.is_number()))
        {
            throw std::invalid_argument("Camera2D " + member + " has an invalid value type");
        }
        const std::vector<double> values = animation_value(value);
        if (timeline::size_cast(values) != arity)
        {
            throw std::invalid_argument("Camera2D " + member + " has the wrong component count");
        }
        for (double component : values)
        {
            if (!std::isfinite(component) || (member == "height" && component <= 0))
            {
                throw std::invalid_argument("Camera2D " + member + " requires finite values and positive height");
            }
        }
        if (arity == 2 && key.value("curve", std::string("linear")) == "geometric")
        {
            throw std::invalid_argument("Camera2D " + member + " does not support geometric interpolation");
        }
    }
    if (member == "skew")
    {
        const double from = keys[0].at("value").get<double>();
        const double to = keys[1].at("value").get<double>();
        const timeline::KeyframeInterpolation interpolation =
            animation_interpolation(keys[1].value("curve", std::string("linear")));
        static_cast<void>(animation_interpolation(keys[0].value("curve", std::string("linear"))));
        if (interpolation == timeline::KeyframeInterpolation::LINEAR && !std::isfinite(to - from))
        {
            throw std::invalid_argument("Camera2D skew linear interpolation requires a finite difference");
        }
        if (interpolation == timeline::KeyframeInterpolation::GEOMETRIC)
        {
            const double ratio = to / from;
            if (from == 0 || to == 0 || !std::isfinite(ratio) || ratio <= 0)
            {
                throw std::invalid_argument("Camera2D skew geometric interpolation requires a finite positive ratio");
            }
            if (from < 0)
            {
                // Core geometric keys require positive values; retain this signed source definition analytically.
                const timeline::Time start = grid.offset();
                const timeline::Time end = grid.frame_start(grid.frame_count() - 1);
                const auto evaluate = [from, to, ratio, start, end](timeline::Time time)
                {
                    if (time <= start)
                    {
                        return from;
                    }
                    if (!(time < end))
                    {
                        return to;
                    }
                    const double fraction =
                        static_cast<double>((time - start).ticks()) / static_cast<double>((end - start).ticks());
                    return from * std::pow(ratio, fraction);
                };
                timeline::Lane lane(id + "-skew", label + " / skew", "curve", start, grid.end_time());
                lane.add(timeline::Curve(lane.id() + "-geometric", "camera2d-input", start, end, evaluate, lane.label(),
                    std::min(from, to), std::max(from, to),
                    {{"signal", signal.dump()}, {"curve", "geometric"}, {"layer", layer}, {"parameter", "skew"}}));
                lanes.push_back(std::move(lane));
                return;
            }
        }
    }
    animation_key_lanes(
        signal, Json{{"type", type}}, id + "-" + member, label + " / " + member, member, layer, grid, lanes);
}

void camera2d_look_lanes(const Json &look, const std::string &id, const std::string &label, const std::string &layer,
    const timeline::FrameGrid &grid, std::vector<timeline::Lane> &signals)
{
    if (look.at("type") != "point2" || (look.contains("path") && look.contains("keys")))
    {
        throw std::invalid_argument("Camera2D look-at requires point2 input with either keys or path");
    }
    const std::string kind = look.contains("path") ? look.at("path").at("kind").get<std::string>() : "";
    if (kind == "circle" || kind == "ellipse" || kind == "lissajous" || kind == "spiral")
    {
        animation_planar_lanes(look.at("path"), Json{{"type", "point2"}}, id + "-look-at", label + " / look-at",
            "look-at", layer, grid, signals);
    }
    else if (kind == "bezier" || kind == "catmull-rom")
    {
        animation_control_point_lanes(
            look, Json{{"type", "point2"}}, id + "-look-at", label + " / look-at", "look-at", layer, grid, signals);
    }
    else
    {
        camera2d_key_lanes(look, "look-at", "point2", id, label, layer, grid, signals);
    }
}

double camera2d_sample(const timeline::Lane &lane, timeline::Time time)
{
    if (std::holds_alternative<timeline::Curve>(lane.items().front()))
    {
        return std::get<timeline::Curve>(lane.items().front()).sample(time);
    }
    return *lane.evaluate_keyframes(time);
}

std::pair<double, double> camera2d_bounds(const timeline::Lane &lane, timeline::Time start, timeline::Time end)
{
    if (std::holds_alternative<timeline::Curve>(lane.items().front()))
    {
        const timeline::Curve &curve = std::get<timeline::Curve>(lane.items().front());
        return {*curve.minimum(), *curve.maximum()};
    }
    return std::minmax(camera2d_sample(lane, start), camera2d_sample(lane, end));
}

double camera2d_direction_component(double value, double look, bool eye)
{
    return eye ? clean_path_value(value) - clean_path_value(look) : value;
}

std::array<double, 2> camera2d_normalize(const std::array<double, 2> &direction)
{
    const double length = std::hypot(direction[0], direction[1]);
    if (length == 0 || !std::isfinite(length))
    {
        throw std::invalid_argument("Camera2D direction cannot normalize a zero or singular vector");
    }
    return {clean_path_value(direction[0] / length), clean_path_value(direction[1] / length)};
}

void camera2d_validate_segment(const std::array<double, 2> &from, const std::array<double, 2> &to)
{
    const std::array<double, 2> from_up = camera2d_normalize(from);
    const std::array<double, 2> to_up = camera2d_normalize(to);
    if (from_up[0] * to_up[1] == from_up[1] * to_up[0] && from_up[0] * to_up[0] + from_up[1] * to_up[1] < 0)
    {
        throw std::invalid_argument("Camera2D direction crosses a zero vector between keys");
    }
}

double camera2d_segment_end(const timeline::Lane &lane, timeline::Time start, timeline::Time end)
{
    const timeline::Keyframe &key = std::get<timeline::Keyframe>(lane.items().front());
    return camera2d_sample(lane, key.interpolation() == timeline::KeyframeInterpolation::HOLD ? start : end);
}

/// An owned camera component over normalized time, with a bound on its
/// uncleaned derivative. Held keys describe the left side of the final jump.
///
struct CameraComponentMotion
{
    std::function<double(double)> m_evaluate;
    double m_speed;
};

CameraComponentMotion camera2d_component_motion(const timeline::Lane &lane, int component)
{
    if (std::holds_alternative<timeline::Keyframe>(lane.items().front()))
    {
        const timeline::Keyframe &from = std::get<timeline::Keyframe>(lane.items().front());
        const timeline::Keyframe &to = std::get<timeline::Keyframe>(lane.items().back());
        const double value = from.value();
        const double change = from.interpolation() == timeline::KeyframeInterpolation::HOLD ? 0 : to.value() - value;
        return {
            [value, change](double fraction) { return clean_path_value(value + fraction * change); }, std::abs(change)};
    }
    const timeline::Curve &curve = std::get<timeline::Curve>(lane.items().front());
    const Json path = Json::parse(curve.attributes().at("path"));
    const std::string kind = path.at("kind").get<std::string>();
    if (kind == "bezier" || kind == "catmull-rom")
    {
        std::vector<double> values;
        for (const Json &point : path.at("control-points"))
        {
            values.push_back(animation_value(point)[component]);
        }
        double speed = 0;
        const int segments = timeline::size_cast(values) - 1;
        const bool catmull_rom = kind == "catmull-rom";
        for (int segment = 0; segment < segments; ++segment)
        {
            if (catmull_rom)
            {
                const std::array<double, 4> hull = catmull_rom_hull(values, segment);
                for (int point = 0; point < 3; ++point)
                {
                    speed = std::max(speed, std::abs(hull[point + 1] - hull[point]) * 3 * segments);
                }
            }
            else
            {
                speed = std::max(speed, std::abs(values[segment + 1] - values[segment]) * segments);
            }
        }
        return {[values, catmull_rom](double fraction) { return control_path_value(values, catmull_rom, fraction); },
            speed};
    }
    const double origin = animation_value(path.at("center"))[component];
    const bool spiral = kind == "spiral";
    const bool lissajous = kind == "lissajous";
    const double radius = path_number(path,
        spiral                 ? "from-radius"
            : kind == "circle" ? "radius"
            : component == 0   ? "x-radius"
                               : "y-radius");
    const double to_radius = spiral ? path_number(path, "to-radius") : radius;
    const double change = to_radius - radius;
    const double phase = lissajous && component == 1 ? 0 : path.value("phase", 0.0);
    const double frequency =
        lissajous ? path_number(path, component == 0 ? "x-frequency" : "y-frequency") : path.value("turns", 1.0);
    constexpr double PI = 3.141592653589793238462643383279502884;
    const double speed = std::abs(change) + 2 * PI * std::abs(frequency) * std::max(radius, to_radius);
    return {[origin, radius, change, phase, frequency, component](double fraction)
        { return planar_path_value(origin, radius, change, phase, frequency, component, fraction); }, speed};
}

void camera2d_validate_motion(
    const std::array<CameraComponentMotion, 4> &motion, double from, double to, int depth, int &remaining)
{
    if (--remaining < 0)
    {
        throw std::invalid_argument("Camera2D cannot exclude a singular direction within the validation limit");
    }
    const double middle = from + (to - from) / 2;
    std::array<double, 2> direction;
    bool safe = false;
    for (int component = 0; component < 2; ++component)
    {
        const double look = motion[component].m_evaluate(middle);
        const double eye = motion[component + 2].m_evaluate(middle);
        direction[component] = eye - look;
        const double radius = (motion[component].m_speed + motion[component + 2].m_speed) * (to - from) / 2;
        // Cleaning can move each component by 1e-12; allow arithmetic roundoff too.
        const double margin =
            2e-12 + 32 * std::numeric_limits<double>::epsilon() * (std::abs(eye) + std::abs(look) + radius);
        safe = safe || std::abs(direction[component]) > radius + margin;
    }
    static_cast<void>(camera2d_normalize(direction));
    if (safe)
    {
        return;
    }
    if (depth >= 52)
    {
        throw std::invalid_argument("Camera2D cannot exclude a singular direction within the validation limit");
    }
    // The derivative bounds certify the entire interval, not just the sampled midpoint.
    camera2d_validate_motion(motion, from, middle, depth + 1, remaining);
    camera2d_validate_motion(motion, middle, to, depth + 1, remaining);
}

bool camera2d_fixed_look(const std::vector<timeline::Lane> &signals, timeline::Time start, timeline::Time end)
{
    return std::holds_alternative<timeline::Keyframe>(signals[0].items().front()) &&
        camera2d_sample(signals[0], start) == camera2d_sample(signals[0], end) &&
        camera2d_sample(signals[1], start) == camera2d_sample(signals[1], end);
}

void camera2d_validate_lissajous_eye(const Json &path, double x, double y, double phase)
{
    if (std::abs(x) > 1 + 1e-12 || std::abs(y) > 1 + 1e-12)
    {
        return;
    }
    constexpr double PI = 3.141592653589793238462643383279502884;
    const double x_frequency = path_number(path, "x-frequency");
    const double y_frequency = path_number(path, "y-frequency");
    const bool solve_x = x_frequency <= y_frequency;
    const double frequency = solve_x ? x_frequency : y_frequency;
    const double root =
        (solve_x ? std::acos(std::clamp(x, -1.0, 1.0)) : std::asin(std::clamp(y, -1.0, 1.0))) / (2 * PI);
    const double offset = std::remainder(phase, 360) / 360;
    const std::array<double, 2> angles =
        solve_x ? std::array<double, 2>{root - offset, -root - offset} : std::array<double, 2>{root, 0.5 - root};
    // Enumerate crossings of the slower axis, then check the other axis at each candidate time.
    for (const double angle : angles)
    {
        const double first = std::ceil(-angle - 1e-12);
        const double last = std::floor(frequency - angle + 1e-12);
        if (last < first)
        {
            continue;
        }
        const bool equal_frequency = x_frequency == y_frequency;
        const double cycles = equal_frequency ? 0 : last - first;
        if (cycles >= 100000)
        {
            throw std::invalid_argument("Camera2D Lissajous eye exceeds the direction validation limit");
        }
        for (int cycle = 0; cycle <= static_cast<int>(cycles); ++cycle)
        {
            const double fraction = std::clamp((angle + first + cycle) / frequency, 0.0, 1.0);
            const double other = equal_frequency ? std::sin(2 * PI * angle)
                : solve_x                        ? std::sin(2 * PI * y_frequency * fraction)
                                                 : std::cos(2 * PI * (offset + x_frequency * fraction));
            if (std::abs(other - (solve_x ? y : x)) < 1e-12)
            {
                throw std::invalid_argument("Camera2D Lissajous eye reaches a singular direction at look-at");
            }
        }
    }
}

void camera2d_validate_eye_hull(
    const std::vector<std::array<double, 2>> &points, const std::array<double, 2> &look, int depth, int &remaining)
{
    if (--remaining < 0)
    {
        throw std::invalid_argument("Camera2D control-point eye exceeds the direction validation limit");
    }
    for (const std::array<double, 2> &point : {points.front(), points.back()})
    {
        static_cast<void>(camera2d_normalize({camera2d_direction_component(point[0], look[0], true),
            camera2d_direction_component(point[1], look[1], true)}));
    }
    for (int component = 0; component < 2; ++component)
    {
        double minimum = points.front()[component];
        double maximum = minimum;
        for (const std::array<double, 2> &point : points)
        {
            minimum = std::min(minimum, point[component]);
            maximum = std::max(maximum, point[component]);
        }
        if (camera2d_direction_component(minimum, look[component], true) > 0 ||
            camera2d_direction_component(maximum, look[component], true) < 0)
        {
            return;
        }
    }
    if (depth == 52)
    {
        throw std::invalid_argument("Camera2D eye cannot exclude a singular direction within the validation limit");
    }
    // Subdivision tightens the whole-curve hull; it is not frame sampling.
    std::vector<std::array<double, 2>> values(points);
    std::vector<std::array<double, 2>> left{points.front()};
    std::vector<std::array<double, 2>> right{points.back()};
    for (int order = timeline::size_cast(values) - 1; order > 0; --order)
    {
        for (int point = 0; point < order; ++point)
        {
            for (int component = 0; component < 2; ++component)
            {
                values[point][component] = 0.5 * values[point][component] + 0.5 * values[point + 1][component];
            }
        }
        left.push_back(values.front());
        right.push_back(values[order - 1]);
    }
    std::reverse(right.begin(), right.end());
    camera2d_validate_eye_hull(left, look, depth + 1, remaining);
    camera2d_validate_eye_hull(right, look, depth + 1, remaining);
}

void camera2d_validate_control_eye(const Json &path, const std::array<double, 2> &look)
{
    std::vector<std::array<double, 2>> points;
    std::array<std::vector<double>, 2> values;
    for (const Json &point : path.at("control-points"))
    {
        const std::vector<double> parsed = animation_value(point.get<std::string>());
        points.push_back({parsed[0], parsed[1]});
        for (int component = 0; component < 2; ++component)
        {
            values[component].push_back(parsed[component]);
        }
    }
    int remaining = 100000;
    if (path.at("kind") == "bezier")
    {
        camera2d_validate_eye_hull(points, look, 0, remaining);
        return;
    }
    for (int segment = 0; segment < timeline::size_cast(points) - 1; ++segment)
    {
        const std::array<double, 4> x = catmull_rom_hull(values[0], segment);
        const std::array<double, 4> y = catmull_rom_hull(values[1], segment);
        std::vector<std::array<double, 2>> hull;
        for (int point = 0; point < 4; ++point)
        {
            hull.push_back({x[point], y[point]});
        }
        camera2d_validate_eye_hull(hull, look, 0, remaining);
    }
}

void camera2d_eye_lanes(const Json &eye, const std::string &id, const std::string &label, const std::string &layer,
    const timeline::FrameGrid &grid, std::vector<timeline::Lane> &signals)
{
    if (!eye.contains("path"))
    {
        camera2d_key_lanes(eye, "eye", "point2", id, label, layer, grid, signals);
        return;
    }
    if (eye.at("type") != "point2" || eye.contains("keys"))
    {
        throw std::invalid_argument("Camera2D eye requires point2 input with either keys or path");
    }
    const Json &path = eye.at("path");
    const std::string kind = path.at("kind").get<std::string>();
    if (kind == "constant" || kind == "line")
    {
        camera2d_key_lanes(eye, "eye", "point2", id, label, layer, grid, signals);
        return;
    }
    const bool spiral = kind == "spiral";
    const bool lissajous = kind == "lissajous";
    const bool control_points = kind == "bezier" || kind == "catmull-rom";
    if (kind != "circle" && kind != "ellipse" && !spiral && !lissajous && !control_points)
    {
        throw std::invalid_argument("unsupported Camera2D eye path kind: " + kind);
    }
    const timeline::Time start = grid.offset();
    const timeline::Time end = grid.frame_start(grid.frame_count() - 1);
    const bool fixed_look = camera2d_fixed_look(signals, start, end);
    if (control_points)
    {
        animation_control_point_lanes(
            eye, Json{{"type", "point2"}}, id + "-eye", label + " / eye", "eye", layer, grid, signals);
        if (fixed_look)
        {
            camera2d_validate_control_eye(
                path, {camera2d_sample(signals[0], start), camera2d_sample(signals[1], start)});
        }
        return;
    }
    const std::vector<double> center = animation_value(path.at("center"));
    if (timeline::size_cast(center) != 2)
    {
        throw std::invalid_argument("Camera2D eye orbit center requires two components");
    }
    const double x_radius = path_number(path, spiral ? "from-radius" : kind == "circle" ? "radius" : "x-radius");
    const double y_radius = kind == "ellipse" || lissajous ? path_number(path, "y-radius") : x_radius;
    const double to_radius = spiral ? path_number(path, "to-radius") : x_radius;
    if (x_radius <= 0 || y_radius <= 0 || to_radius <= 0)
    {
        throw std::invalid_argument("Camera2D eye orbit radii must be positive to avoid singular directions");
    }
    animation_planar_lanes(path, Json{{"type", "point2"}}, id + "-eye", label + " / eye", "eye", layer, grid, signals);
    if (!fixed_look)
    {
        return;
    }
    constexpr double PI = 3.141592653589793238462643383279502884;
    const double dx = camera2d_sample(signals[0], start) - center[0];
    const double dy = camera2d_sample(signals[1], start) - center[1];
    const double phase = path.contains("phase") ? path_number(path, "phase") : 0;
    if (lissajous)
    {
        camera2d_validate_lissajous_eye(path, dx / x_radius, dy / y_radius, phase);
        return;
    }
    const double turns = path.contains("turns") ? path_number(path, "turns") : 1;
    if (spiral && to_radius != x_radius)
    {
        // A changing radius can reach the fixed look-at only once, independent of the frame grid.
        const double distance = std::hypot(dx, dy);
        const double fraction = (distance - x_radius) / (to_radius - x_radius);
        if (fraction >= 0 && fraction <= 1)
        {
            const double radians = (phase + 360 * turns * fraction) * PI / 180;
            if (std::hypot(dx / distance - std::cos(radians), dy / distance - std::sin(radians)) < 1e-12)
            {
                throw std::invalid_argument("Camera2D spiral eye reaches a singular direction at look-at");
            }
        }
        return;
    }
    const double x = dx / x_radius;
    const double y = dy / y_radius;
    if (std::abs(std::hypot(x, y) - 1) < 1e-12)
    {
        // A look-at on the supporting ellipse is singular only on the traveled arc.
        const double angle = std::atan2(y, x) / (2 * PI);
        const double start_phase = phase / 360;
        const double end_phase = start_phase + turns;
        const double first = std::min(start_phase, end_phase);
        const double last = std::max(start_phase, end_phase);
        const double winding = std::ceil(first - angle - 1e-12);
        if (angle + winding <= last + 1e-12)
        {
            throw std::invalid_argument("Camera2D eye orbit reaches a singular direction at look-at");
        }
    }
}

double camera2d_corners_aspect(const std::vector<double> &view)
{
    double width;
    double height;
    if (view.size() == 4)
    {
        width = std::abs(view[1] - view[0]);
        height = std::abs(view[3] - view[2]);
    }
    else if (view.size() == 6)
    {
        width = std::hypot(view[1] - view[4], view[2] - view[5]);
        height = std::hypot(view[0] - view[4], view[3] - view[5]);
    }
    else
    {
        throw std::invalid_argument("Camera2D source corners requires four or six components");
    }
    if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0)
    {
        throw std::invalid_argument("Camera2D source corners has invalid width or height");
    }
    return width / height;
}

void animation_camera2d_lanes(const Json &track, const Json &catalog, const std::filesystem::path &source_path,
    const Json &config, const std::string &video, const std::string &id, const std::string &layer,
    const timeline::FrameGrid &grid, std::vector<timeline::Lane> &lanes)
{
    const std::string name = track.at("name").get<std::string>();
    if (name.empty() || grid.frame_count() < 2)
    {
        throw std::invalid_argument("Camera2D requires a name and at least two frames");
    }
    if (track.value("mode", std::string("keyframes")) != "keyframes")
    {
        throw std::invalid_argument("Camera2D non-keyframe modes are not supported yet");
    }
    const std::string output = track.at("output").get<std::string>();
    const bool corners = output == "corners";
    const Json &parameters = catalog.at("parameters");
    if ((!corners && output != "center-mag") || !parameters.contains(output) ||
        parameters.at(output).at("type") != output)
    {
        throw std::invalid_argument("Camera2D requires a catalog-declared corners or center-mag output");
    }
    if (track.at("aspect") != "source" || (!corners && video != "F6"))
    {
        throw std::invalid_argument("Camera2D requires source aspect and supported video mode F6");
    }
    const std::map<std::string, std::string> source = animation_source(source_path, config);
    if (source.count(output) == 0)
    {
        throw std::invalid_argument("Camera2D output is missing from the source PAR entry");
    }
    const std::vector<double> source_view = animation_value(source.at(output));
    if (!corners && (timeline::size_cast(source_view) < 3 || timeline::size_cast(source_view) > 6))
    {
        throw std::invalid_argument("Camera2D source center-mag requires three through six components");
    }
    const double stretch = corners || source_view.size() < 4 || source_view[3] == 0 ? 1 : source_view[3];
    const double aspect = corners ? camera2d_corners_aspect(source_view) : (4.0 / 3.0) / std::abs(stretch);
    if (!std::isfinite(aspect) || aspect <= 0)
    {
        throw std::invalid_argument("Camera2D source aspect must be finite and positive");
    }
    const std::string label = layer.empty() ? name : layer + " / " + name;
    std::vector<timeline::Lane> signals;
    camera2d_look_lanes(track.at("look-at"), id, label, layer, grid, signals);
    const bool eye = track.contains("eye");
    if (eye)
    {
        camera2d_eye_lanes(track.at("eye"), id, label, layer, grid, signals);
    }
    else
    {
        camera2d_key_lanes(track.at("view-up"), "view-up", "vector2", id, label, layer, grid, signals);
    }
    camera2d_key_lanes(track.at("height"), "height", "double", id, label, layer, grid, signals);
    if (track.contains("skew"))
    {
        camera2d_key_lanes(track.at("skew"), "skew", "double", id, label, layer, grid, signals);
    }

    const timeline::Time start = grid.offset();
    const timeline::Time end = grid.frame_start(grid.frame_count() - 1);
    const std::optional<timeline::Lane> skew =
        track.contains("skew") ? std::optional<timeline::Lane>(signals[5]) : std::nullopt;
    const auto skew_value = [skew](timeline::Time time)
    {
        return skew ? camera2d_sample(*skew, time) : 0;
    };
    const auto direction = [x = signals[2], y = signals[3], look_x = signals[0], look_y = signals[1], eye](
                               timeline::Time time)
    {
        return std::array<double, 2>{
            camera2d_direction_component(camera2d_sample(x, time), camera2d_sample(look_x, time), eye),
            camera2d_direction_component(camera2d_sample(y, time), camera2d_sample(look_y, time), eye)};
    };
    static_cast<void>(camera2d_normalize(direction(end)));
    static_cast<void>(camera2d_normalize(direction(start)));
    if (eye && !camera2d_fixed_look(signals, start, end) &&
        (std::holds_alternative<timeline::Curve>(signals[0].items().front()) ||
            std::holds_alternative<timeline::Curve>(signals[2].items().front())))
    {
        const std::array<CameraComponentMotion, 4> motion{camera2d_component_motion(signals[0], 0),
            camera2d_component_motion(signals[1], 1), camera2d_component_motion(signals[2], 0),
            camera2d_component_motion(signals[3], 1)};
        for (const CameraComponentMotion &component : motion)
        {
            if (!std::isfinite(component.m_speed))
            {
                throw std::invalid_argument("Camera2D singular direction validation limit requires finite bounds");
            }
        }
        int remaining = 100000;
        camera2d_validate_motion(motion, 0, 1, 0, remaining);
    }
    else if (std::holds_alternative<timeline::Keyframe>(signals[2].items().front()))
    {
        const std::array<double, 2> segment_end{
            camera2d_direction_component(camera2d_segment_end(signals[2], start, end),
                eye ? camera2d_segment_end(signals[0], start, end) : 0, eye),
            camera2d_direction_component(camera2d_segment_end(signals[3], start, end),
                eye ? camera2d_segment_end(signals[1], start, end) : 0, eye)};
        camera2d_validate_segment(direction(start), segment_end);
    }
    const auto normalized_up = [direction](timeline::Time time)
    {
        return camera2d_normalize(direction(time));
    };
    const auto magnification = [height = signals[4], aspect](timeline::Time time)
    {
        return 4 / (aspect * *height.evaluate_keyframes(time));
    };
    const double from_mag = corners ? 0 : magnification(start);
    const double to_mag = corners ? 0 : magnification(end);
    if (!corners && (!std::isfinite(from_mag) || !std::isfinite(to_mag) || from_mag <= 0 || to_mag <= 0))
    {
        throw std::invalid_argument("Camera2D magnification must be finite and positive");
    }
    const std::array<std::string, 6> components = corners
        ? std::array<std::string, 6>{"top-left-x", "bottom-right-x", "bottom-right-y", "top-left-y", "bottom-left-x",
              "bottom-left-y"}
        : std::array<std::string, 6>{"center-x", "center-y", "magnification", "x-mag-factor", "rotation", "skew"};
    std::array<std::pair<double, double>, 6> bounds{camera2d_bounds(signals[0], start, end),
        camera2d_bounds(signals[1], start, end), std::minmax(from_mag, to_mag), {stretch, stretch}, {-180, 180},
        std::minmax(skew_value(start), skew_value(end))};
    if (corners)
    {
        constexpr double PI = 3.141592653589793238462643383279502884;
        const double from = skew_value(start);
        const double to = skew_value(end);
        const bool hold = skew && std::holds_alternative<timeline::Keyframe>(skew->items().front()) &&
            std::get<timeline::Keyframe>(skew->items().front()).interpolation() ==
                timeline::KeyframeInterpolation::HOLD;
        if (std::abs(std::remainder(from, 180)) == 90 || std::abs(std::remainder(to, 180)) == 90 ||
            (!hold && std::abs(to - from) >= 90 - std::remainder(std::min(from, to), 180)))
        {
            throw std::invalid_argument("Camera2D corners skew crosses a singular tangent angle");
        }
        const double tangent = std::max(std::abs(std::tan(from * PI / 180)), std::abs(std::tan(to * PI / 180)));
        // Rotation preserves distance; bound every sheared corner over the full continuous interval.
        const double height = camera2d_bounds(signals[4], start, end).second;
        const double radius = std::hypot(height * aspect / 2 + height * tangent / 2, height / 2);
        for (int component = 0; component < 6; ++component)
        {
            const int coordinate = component == 0 || component == 1 || component == 4 ? 0 : 1;
            const std::pair<double, double> center = camera2d_bounds(signals[coordinate], start, end);
            bounds[component] = {center.first - radius, center.second + radius};
            if (!std::isfinite(bounds[component].first) || !std::isfinite(bounds[component].second))
            {
                throw std::invalid_argument("Camera2D corners output requires finite bounds");
            }
        }
    }
    const timeline::Attributes attributes{{"camera2d", track.dump()}, {"track", id}, {"layer", layer}, {"camera", name},
        {"source-value", source.at(output)}, {"aspect", std::to_string(aspect)},
        {"source-entry", config.at("source").at("name").get<std::string>()},
        {"source-file", (source_path.parent_path() / config.at("source").at("file").get<std::string>()).string()}};
    for (int component = 0; component < 6; ++component)
    {
        const auto evaluate = [x = signals[0], y = signals[1], height = signals[4], normalized_up, magnification,
                                  stretch, aspect, corners, component, skew_value](timeline::Time time)
        {
            const std::array<double, 2> up = normalized_up(time);
            const double center_x = camera2d_sample(x, time);
            const double center_y = camera2d_sample(y, time);
            constexpr double PI = 3.141592653589793238462643383279502884;
            if (corners)
            {
                const double half_height = camera2d_sample(height, time) / 2;
                const double half_width = camera2d_sample(height, time) * aspect / 2;
                const double offset = half_height * std::tan(skew_value(time) * PI / 180);
                const std::array<double, 6> values{center_x + (-half_width + offset) * up[1] + half_height * up[0],
                    center_x + (half_width - offset) * up[1] - half_height * up[0],
                    center_y - (half_width - offset) * up[0] - half_height * up[1],
                    center_y - (-half_width + offset) * up[0] + half_height * up[1],
                    center_x + (-half_width - offset) * up[1] - half_height * up[0],
                    center_y - (-half_width - offset) * up[0] - half_height * up[1]};
                return clean_path_value(values[component]);
            }
            const std::array<double, 6> values{center_x, center_y, magnification(time), stretch,
                std::atan2(up[0], up[1]) * 180 / PI, skew_value(time)};
            return clean_path_value(values[component]);
        };
        const std::string suffix = "-" + output + "[" + std::to_string(component) + "]";
        timeline::Attributes output_attributes = attributes;
        output_attributes["parameter"] = output;
        output_attributes["component"] = components[component];
        timeline::Lane lane(id + suffix, label + " / " + components[component], "curve", start, grid.end_time());
        lane.add(timeline::Curve(id + suffix + "-camera", "camera2d", start, end, evaluate, lane.label(),
            clean_path_value(bounds[component].first), clean_path_value(bounds[component].second), output_attributes));
        lanes.push_back(std::move(lane));
    }
    for (int component = 0; component < timeline::size_cast(signals); ++component)
    {
        const std::string member = component < 2 ? "look-at"
            : component < 4                      ? (eye ? "eye" : "view-up")
            : component == 4                     ? "height"
                                                 : "skew";
        timeline::Attributes input_attributes = attributes;
        input_attributes["parameter"] = name + "." + member;
        input_attributes["signal"] = track.at(member).dump();
        input_attributes["component"] = std::to_string(component >= 4 ? 0 : component % 2);
        timeline::Lane lane(signals[component].id(), signals[component].label(),
            member == "view-up" ? "curve" : signals[component].kind(), start, grid.end_time());
        if (member == "view-up")
        {
            const auto evaluate = [normalized_up, component](timeline::Time time)
            {
                return normalized_up(time)[component - 2];
            };
            input_attributes["normalize"] = "true";
            lane.add(timeline::Curve(lane.id() + "-normalized", "camera2d-input", start, end, evaluate, lane.label(),
                -1, 1, input_attributes));
        }
        else
        {
            for (const timeline::Item &item : signals[component].items())
            {
                if (std::holds_alternative<timeline::Curve>(item))
                {
                    const timeline::Curve &curve = std::get<timeline::Curve>(item);
                    timeline::Attributes curve_attributes = curve.attributes();
                    for (const auto &[field, value] : input_attributes)
                    {
                        curve_attributes[field] = value;
                    }
                    const auto evaluate = [curve](timeline::Time time)
                    {
                        return curve.sample(time);
                    };
                    lane.add(timeline::Curve(curve.id(), curve.kind(), curve.start(), curve.end(), evaluate,
                        curve.label(), curve.minimum(), curve.maximum(), curve_attributes));
                    continue;
                }
                const timeline::Keyframe &key = std::get<timeline::Keyframe>(item);
                timeline::Attributes key_attributes = key.attributes();
                for (const auto &[field, value] : input_attributes)
                {
                    key_attributes[field] = value;
                }
                lane.add(timeline::Keyframe(key.id(), key.time(), key.value(), key.interpolation(), key_attributes));
            }
        }
        lanes.push_back(std::move(lane));
    }
    if (eye)
    {
        for (int component = 0; component < 2; ++component)
        {
            const std::string suffix = "-derived-view-up[" + std::to_string(component) + "]";
            timeline::Attributes derived_attributes = attributes;
            derived_attributes["parameter"] = name + ".derived-view-up";
            derived_attributes["component"] = std::to_string(component);
            derived_attributes["derived-from"] = "eye-look-at";
            const auto evaluate = [normalized_up, component](timeline::Time time)
            {
                return normalized_up(time)[component];
            };
            timeline::Lane lane(id + suffix, label + " / derived-view-up[" + std::to_string(component) + "]", "curve",
                start, grid.end_time());
            lane.add(timeline::Curve(lane.id() + "-normalized", "camera2d-direction", start, end, evaluate,
                lane.label(), -1, 1, derived_attributes));
            lanes.push_back(std::move(lane));
        }
        if (track.contains("view-up"))
        {
            std::vector<timeline::Lane> authored_up;
            camera2d_key_lanes(track.at("view-up"), "view-up", "vector2", id, label, layer, grid, authored_up);
            // ParAnimator does not evaluate view-up when eye supplies the direction.
            for (int component = 0; component < 2; ++component)
            {
                timeline::Lane lane(
                    authored_up[component].id(), authored_up[component].label(), "keyframes", start, grid.end_time());
                for (const timeline::Item &item : authored_up[component].items())
                {
                    const timeline::Keyframe &key = std::get<timeline::Keyframe>(item);
                    timeline::Attributes up_attributes = key.attributes();
                    for (const auto &[field, value] : attributes)
                    {
                        up_attributes[field] = value;
                    }
                    up_attributes["parameter"] = name + ".view-up";
                    up_attributes["component"] = std::to_string(component);
                    up_attributes["signal"] = track.at("view-up").dump();
                    up_attributes["used-by-camera"] = "false";
                    lane.add(timeline::Keyframe(
                        key.id(), key.time(), key.value(), key.interpolation(), std::move(up_attributes)));
                }
                lanes.push_back(std::move(lane));
            }
        }
    }
}

/// Catalog contract for one specialized view member and its camera eligibility.
///
struct ViewMember
{
    std::string name;
    std::string type;
    int components;
    bool camera;
};

const std::array<ViewMember, 12> ID_VIEW_MEMBERS{
    ViewMember{"rotation", "numeric-tuple", 3, true},
    ViewMember{"perspective", "integer", 1, true},
    ViewMember{"xyshift", "numeric-tuple", 2, true},
    ViewMember{"scalexyz", "numeric-tuple", 3, false},
    ViewMember{"roughness", "integer", 1, false},
    ViewMember{"sphere", "yes-no", 1, false},
    ViewMember{"longitude", "numeric-tuple", 2, false},
    ViewMember{"latitude", "numeric-tuple", 2, false},
    ViewMember{"radius", "integer", 1, false},
    ViewMember{"stereo", "integer", 1, false},
    ViewMember{"interocular", "integer", 1, false},
    ViewMember{"converge", "integer", 1, false},
};

void view_fields(const Json &object, const std::vector<std::string> &allowed, const std::string &family)
{
    if (!object.is_object())
    {
        throw std::invalid_argument(family + " view requires an object");
    }
    for (Json::const_iterator field = object.begin(); field != object.end(); ++field)
    {
        if (std::find(allowed.begin(), allowed.end(), field.key()) == allowed.end())
        {
            throw std::invalid_argument("unsupported " + family + " view field '" + field.key() + "'");
        }
    }
}

std::vector<timeline::Lane> view_keys(const Json &signal, const Json &metadata, const std::string &member,
    const std::string &id, const std::string &label, const std::string &layer, const timeline::FrameGrid &grid,
    const std::string &family)
{
    if (signal.contains("path"))
    {
        throw std::invalid_argument(family + " view paths are not allowed");
    }
    const std::string type = metadata.at("type").get<std::string>();
    const bool camera = type == "point3" || type == "vector3";
    view_fields(signal,
        camera ? std::vector<std::string>{"type", "keys", "normalize"}
               : std::vector<std::string>{"type", "arity", "keys"},
        family);
    if (signal.at("type") != type || (type == "numeric-tuple" && signal.at("arity") != metadata.at("arity")))
    {
        throw std::invalid_argument(family + " view " + member + " has the wrong type or arity");
    }
    if (signal.contains("normalize") && (type != "vector3" || signal.at("normalize") != true))
    {
        throw std::invalid_argument(family + " view-up normalize must be true");
    }
    const Json &keys = signal.at("keys");
    if (!keys.is_array() || keys.size() != 2 || source_frame(keys[0]) != 0 ||
        source_frame(keys[1]) != grid.frame_count() - 1)
    {
        throw std::invalid_argument(family + " view " + member + " requires two keys spanning the full frame range");
    }
    const int components = camera ? 3 : metadata.value("arity", 1);
    Json realized = signal;
    for (int index = 0; index < 2; ++index)
    {
        const Json &key = keys[index];
        view_fields(key, {"frame", "value", "curve"}, family);
        const std::string curve = key.value("curve",
            metadata.value("default-curve", std::string(type == "yes-no" || type == "enum" ? "hold" : "linear")));
        const timeline::KeyframeInterpolation interpolation = animation_interpolation(curve);
        if (interpolation == timeline::KeyframeInterpolation::GEOMETRIC ||
            ((type == "yes-no" || type == "enum") && interpolation != timeline::KeyframeInterpolation::HOLD))
        {
            throw std::invalid_argument(family + " view " + member + " requires scalar or discrete interpolation");
        }
        const Json &value = key.at("value");
        if (type == "enum")
        {
            const Json &allowed = metadata.at("values");
            if (!value.is_string() || !allowed.is_array() ||
                std::find(allowed.begin(), allowed.end(), value) == allowed.end())
            {
                throw std::invalid_argument(family + " " + member + " requires a catalog enum value");
            }
            continue;
        }
        if (type == "double" && !value.is_number())
        {
            throw std::invalid_argument(family + " " + member + " requires a JSON number");
        }
        if (type == "yes-no")
        {
            if (!value.is_boolean())
            {
                throw std::invalid_argument("Id 3D sphere requires a JSON boolean");
            }
            realized["keys"][index]["value"] = value.get<bool>() ? "yes" : "no";
            continue;
        }
        if (type == "integer" && !value.is_number_integer())
        {
            throw std::invalid_argument(family + " " + member + " requires a JSON integer");
        }
        if (components > 1 && !value.is_string())
        {
            throw std::invalid_argument(family + " " + member + " requires a slash-delimited tuple string");
        }
        const std::vector<double> values = animation_value(value);
        if (timeline::size_cast(values) != components)
        {
            throw std::invalid_argument(family + " " + member + " has the wrong component count");
        }
        for (double scalar : values)
        {
            if (type == "integer" &&
                (scalar < std::numeric_limits<int>::min() || scalar > std::numeric_limits<int>::max()))
            {
                throw std::invalid_argument(family + " " + member + " exceeds the source integer range");
            }
            if ((metadata.contains("min") && scalar < metadata.at("min").get<double>()) ||
                (metadata.contains("max") && scalar > metadata.at("max").get<double>()) ||
                (member == "radius" && scalar < 0) || (member == "stereo" && (scalar < 0 || scalar > 4)))
            {
                throw std::invalid_argument(family + " " + member + " exceeds catalog or source bounds");
            }
        }
    }
    std::vector<timeline::Lane> lanes;
    animation_key_lanes(realized, metadata, id, label, member, layer, grid, lanes);
    if (type != "yes-no" && type != "enum")
    {
        for (const timeline::Lane &lane : lanes)
        {
            const timeline::Keyframe &from = std::get<timeline::Keyframe>(lane.items().front());
            const timeline::Keyframe &to = std::get<timeline::Keyframe>(lane.items().back());
            if (from.interpolation() == timeline::KeyframeInterpolation::LINEAR &&
                !std::isfinite(to.value() - from.value()))
            {
                throw std::invalid_argument(family + " interpolation requires a finite difference");
            }
        }
    }
    return lanes;
}

timeline::Lane view_annotated(const timeline::Lane &source, const timeline::Attributes &attributes)
{
    timeline::Lane lane(source.id(), source.label(), source.kind(), source.start(), source.end());
    for (const timeline::Item &item : source.items())
    {
        std::visit(
            [&lane, &attributes](const auto &value)
            {
                timeline::Attributes combined = value.attributes();
                for (const auto &[name, text] : attributes)
                {
                    combined[name] = text;
                }
                using Value = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, timeline::Keyframe>)
                {
                    lane.add(
                        timeline::Keyframe(value.id(), value.time(), value.value(), value.interpolation(), combined));
                }
                else if constexpr (std::is_same_v<Value, timeline::Instant>)
                {
                    lane.add(timeline::Instant(
                        value.id(), value.kind(), value.time(), value.label(), value.strength(), combined));
                }
                else if constexpr (std::is_same_v<Value, timeline::Interval>)
                {
                    lane.add(timeline::Interval(value.id(), value.kind(), value.start(), value.end(), value.label(),
                        value.strength(), combined));
                }
                else
                {
                    throw std::logic_error("view key definition contains an unexpected item");
                }
            },
            item);
    }
    return lane;
}

double camera3d_normalization_interval(const std::array<std::array<double, 3>, 2> &up, int depth)
{
    double minimum_squared = 0;
    double maximum_squared = 0;
    constexpr double ROUNDING = 16 * std::numeric_limits<double>::epsilon();
    for (const std::array<double, 3> &point : up)
    {
        const double squared = point[0] * point[0] + point[1] * point[1] + point[2] * point[2];
        if (squared == 0 || !std::isfinite(squared))
        {
            throw std::invalid_argument("camera view-up has an invalid normalization length");
        }
    }
    for (int axis = 0; axis < 3; ++axis)
    {
        const double first = up[0][axis];
        const double last = up[1][axis];
        const bool constant = first == last;
        const double minimum = (first <= 0 && last >= 0) || (last <= 0 && first >= 0)
            ? 0
            : std::min(std::abs(first), std::abs(last)) * (constant ? 1 : 1 - ROUNDING);
        const double maximum = std::max(std::abs(first), std::abs(last)) * (constant ? 1 : 1 + ROUNDING);
        minimum_squared += minimum * minimum;
        maximum_squared += maximum * maximum;
    }
    if (minimum_squared > 0 && std::isfinite(maximum_squared))
    {
        // Rounded subnormal squares can make the first normalized component
        // exceed one. The camera normalizes again after component cleanup.
        return minimum_squared < std::numeric_limits<double>::min() ? 2 : 1;
    }
    if (depth == 16)
    {
        throw std::invalid_argument("camera view-up has an invalid normalization interval");
    }
    std::array<double, 3> middle{};
    for (int axis = 0; axis < 3; ++axis)
    {
        middle[axis] = up[0][axis] + (up[1][axis] - up[0][axis]) / 2;
    }
    return std::max(camera3d_normalization_interval({up[0], middle}, depth + 1),
        camera3d_normalization_interval({middle, up[1]}, depth + 1));
}

double camera3d_normalization_bounds(
    const std::vector<timeline::Lane> &signals, timeline::Time start, timeline::Time end)
{
    std::array<std::array<double, 3>, 2> up{};
    std::array<double, 3> last{};
    for (int axis = 0; axis < 3; ++axis)
    {
        up[0][axis] = camera2d_sample(signals[6 + axis], start);
        up[1][axis] = camera2d_segment_end(signals[6 + axis], start, end);
        last[axis] = camera2d_sample(signals[6 + axis], end);
    }
    return std::max(camera3d_normalization_interval(up, 0), camera3d_normalization_interval({last, last}, 0));
}

bool camera3d_straight_up(const std::vector<timeline::Lane> &signals, timeline::Time start, timeline::Time end)
{
    for (int point = 0; point < 3; ++point)
    {
        const auto sample = [&](int component)
        {
            return point == 1 ? camera2d_segment_end(signals[component], start, end)
                              : camera2d_sample(signals[component], point == 0 ? start : end);
        };
        const double x = sample(6);
        const double y = sample(7);
        const double z = sample(8);
        const double length = std::sqrt(x * x + y * y + z * z);
        if (x != 0 || y <= 0 || length == 0 || !std::isfinite(length) || y / length < 1e-12)
        {
            return false;
        }
    }
    // Positive affine y makes z/y monotone. Bound source cleanup locally,
    // not with the smallest y divided by an unrelated endpoint's length.
    const double y = camera2d_sample(signals[7], start);
    const double z = camera2d_sample(signals[8], start);
    const double last_y = camera2d_segment_end(signals[7], start, end);
    const double last_z = camera2d_segment_end(signals[8], start, end);
    if (y == last_y && z == last_z)
    {
        return true;
    }
    const double minimum_y = std::min(1 / std::hypot(1.0, z / y), 1 / std::hypot(1.0, last_z / last_y));
    return minimum_y >= std::sqrt(2.0) * 1e-12;
}

void id_view_validate_azimuth(const std::array<std::array<double, 3>, 2> &direction,
    const std::array<std::array<double, 3>, 2> &up, double direction_scale, double up_scale)
{
    constexpr double ROUNDING = 128 * std::numeric_limits<double>::epsilon();
    const double horizontal_squared = direction[0][0] * direction[0][0] + direction[0][2] * direction[0][2];
    const double proportional = (up[0][0] * direction[0][0] + up[0][2] * direction[0][2]) / horizontal_squared;
    const double residual_margin = ROUNDING * std::max(1.0, std::abs(proportional));
    double residual = 0;
    double minimum_orientation = std::numeric_limits<double>::max();
    double maximum_direction = 0;
    double maximum_up = 0;
    for (int point = 0; point < 2; ++point)
    {
        const double x = up[point][0] - proportional * direction[point][0];
        const double z = up[point][2] - proportional * direction[point][2];
        if (!std::isfinite(proportional) || std::abs(x) > residual_margin || std::abs(z) > residual_margin)
        {
            throw std::invalid_argument("unsupported Id 3D camera view-up; requires world-up, a fixed vertical "
                                        "viewing plane, or a proportional moving-azimuth hint");
        }
        residual = std::max(residual, std::hypot(x, z));
        minimum_orientation = std::min(minimum_orientation, up[point][1] - proportional * direction[point][1]);
        maximum_direction = std::max(maximum_direction,
            std::abs(direction[point][0]) + std::abs(direction[point][1]) + std::abs(direction[point][2]));
        maximum_up = std::max(maximum_up, std::abs(up[point][0]) + std::abs(up[point][1]) + std::abs(up[point][2]));
    }
    if (minimum_orientation <= 0)
    {
        throw std::invalid_argument("unsupported Id 3D camera view-up orientation interval");
    }

    // Affine horizontal hints must share one proportionality constant with
    // the moving direction. Their signed cross product is then h(t) * S(t).
    const double dx = direction[1][0] - direction[0][0];
    const double dz = direction[1][2] - direction[0][2];
    const double squared_delta = dx * dx + dz * dz;
    const double nearest =
        squared_delta == 0 ? 0 : std::clamp(-(direction[0][0] * dx + direction[0][2] * dz) / squared_delta, 0.0, 1.0);
    const double minimum_horizontal = std::hypot(direction[0][0] + nearest * dx, direction[0][2] + nearest * dz);
    if (minimum_horizontal <= 2e-12 / direction_scale)
    {
        throw std::invalid_argument("Id 3D camera view-up has a vertical direction interval");
    }
    double up_delta_squared = 0;
    double up_projection = 0;
    for (int axis = 0; axis < 3; ++axis)
    {
        const double delta = up[1][axis] - up[0][axis];
        up_delta_squared += delta * delta;
        up_projection += up[0][axis] * delta;
    }
    const double nearest_up = up_delta_squared == 0 ? 0 : std::clamp(-up_projection / up_delta_squared, 0.0, 1.0);
    double minimum_up_squared = 0;
    for (int axis = 0; axis < 3; ++axis)
    {
        const double component = (up[0][axis] + nearest_up * (up[1][axis] - up[0][axis])) * up_scale;
        minimum_up_squared += component * component;
    }
    if (minimum_up_squared == 0)
    {
        throw std::invalid_argument("Id 3D camera view-up has an invalid normalization interval");
    }

    // Bound cleanup and floating-point residuals over the whole interval,
    // before comparing normalized camera-up vectors with source tolerance.
    const double cleanup = std::sqrt(3.0) * 1e-12;
    const double direction_error = 2 * cleanup / direction_scale + 2 * ROUNDING;
    const double up_error =
        (minimum_up_squared < std::numeric_limits<double>::min() ? 2 : 1) * cleanup + residual + 2 * residual_margin;
    const double minimum = minimum_horizontal * minimum_orientation;
    const double cross_error = (maximum_direction + direction_error) * up_error + maximum_up * direction_error;
    if (minimum <= cross_error || minimum_horizontal <= direction_error ||
        4 * cross_error / (minimum - cross_error) + 4 * direction_error / (minimum_horizontal - direction_error) >=
            5e-10)
    {
        throw std::invalid_argument(
            "unsupported Id 3D camera view-up; cannot bound moving-azimuth roll within source tolerance");
    }
}

void id_view_validate_up_interval(const std::vector<timeline::Lane> &signals, timeline::Time start, timeline::Time end)
{
    const auto sample = [&](int component, int point)
    {
        return point == 1 ? camera2d_segment_end(signals[component], start, end)
                          : camera2d_sample(signals[component], start);
    };
    std::array<std::array<double, 3>, 2> direction{};
    std::array<std::array<double, 3>, 2> up{};
    bool world_up = true;
    int zero_axis = -1;
    double maximum_up_squared = 0;
    double direction_scale = 0;
    for (int point = 0; point < 2; ++point)
    {
        double squared = 0;
        for (int axis = 0; axis < 3; ++axis)
        {
            direction[point][axis] = sample(3 + axis, point) - sample(axis, point);
            direction_scale = std::max(direction_scale, std::abs(direction[point][axis]));
            up[point][axis] = sample(6 + axis, point);
            squared += up[point][axis] * up[point][axis];
        }
        world_up = world_up && up[point][0] == 0 && up[point][2] == 0 && up[point][1] > 0;
        if (!std::isfinite(squared) || squared == 0)
        {
            throw std::invalid_argument("Id 3D camera view-up has an invalid normalization length");
        }
        maximum_up_squared = std::max(maximum_up_squared, squared);
    }
    if (world_up)
    {
        return;
    }
    for (int axis : {0, 2})
    {
        bool zero = true;
        for (int component : {axis, 3 + axis, 6 + axis})
        {
            zero = zero && sample(component, 0) == 0 && sample(component, 1) == 0;
        }
        if (zero)
        {
            zero_axis = axis;
            break;
        }
    }
    const double up_scale = std::sqrt(maximum_up_squared);
    if (!std::isfinite(direction_scale) || direction_scale == 0)
    {
        throw std::invalid_argument("Id 3D camera view-up has a singular direction");
    }
    for (int point = 0; point < 2; ++point)
    {
        for (int axis = 0; axis < 3; ++axis)
        {
            up[point][axis] /= up_scale;
            direction[point][axis] /= direction_scale;
        }
    }
    const double horizontal_length = std::hypot(direction[0][0], direction[0][2]);
    if (horizontal_length == 0)
    {
        throw std::invalid_argument("Id 3D camera view-up has a vertical direction");
    }
    const double plane_x = zero_axis == 0 ? 0 : zero_axis == 2 ? 1 : direction[0][0] / horizontal_length;
    const double plane_z = zero_axis == 0 ? 1 : zero_axis == 2 ? 0 : direction[0][2] / horizontal_length;
    constexpr double PLANE_TOLERANCE = 128 * std::numeric_limits<double>::epsilon();
    std::array<double, 2> horizontal_direction{};
    std::array<double, 2> horizontal_up{};
    for (int point = 0; point < 2; ++point)
    {
        if (std::abs(plane_x * direction[point][2] - plane_z * direction[point][0]) > PLANE_TOLERANCE ||
            std::abs(plane_x * up[point][2] - plane_z * up[point][0]) > PLANE_TOLERANCE)
        {
            id_view_validate_azimuth(direction, up, direction_scale, up_scale);
            return;
        }
        horizontal_direction[point] = plane_x * direction[point][0] + plane_z * direction[point][2];
        horizontal_up[point] = plane_x * up[point][0] + plane_z * up[point][2];
    }
    const auto validate = [&](int first, int last)
    {
        const double h = horizontal_direction[first];
        const double y = direction[first][1];
        const double dh = horizontal_direction[last] - h;
        const double dy = direction[last][1] - y;
        const double uh = horizontal_up[first];
        const double uy = up[first][1];
        const double duh = horizontal_up[last] - uh;
        const double duy = up[last][1] - uy;
        const double sign = h < 0 ? -1 : 1;
        const double minimum_horizontal = std::min(std::abs(h), std::abs(horizontal_direction[last]));
        if (sign * horizontal_direction[last] <= 0 || minimum_horizontal <= 2e-12 / direction_scale)
        {
            throw std::invalid_argument("Id 3D camera view-up has a vertical direction interval");
        }

        // In a fixed vertical plane, the signed cross product is quadratic.
        // Its minimum must survive source component cleanup and rounding.
        const double a = sign * (dh * duy - dy * duh);
        const double b = sign * (h * duy + dh * uy - y * duh - dy * uh);
        const double c = sign * (h * uy - y * uh);
        const double fraction = a > 0 ? std::clamp(-b / (2 * a), 0.0, 1.0) : 0;
        const double minimum = std::min({c, a + b + c, (a * fraction + b) * fraction + c});
        const double maximum_direction = std::max(std::abs(h), std::abs(horizontal_direction[last])) +
            std::max(std::abs(y), std::abs(direction[last][1]));
        const double maximum_up =
            std::max(std::abs(uh), std::abs(horizontal_up[last])) + std::max(std::abs(uy), std::abs(up[last][1]));
        const double squared_delta = duh * duh + duy * duy;
        const double nearest = squared_delta == 0 ? 0 : std::clamp(-(uh * duh + uy * duy) / squared_delta, 0.0, 1.0);
        const double nearest_h = (uh + nearest * duh) * up_scale;
        const double nearest_y = (uy + nearest * duy) * up_scale;
        const double minimum_up_squared = nearest_h * nearest_h + nearest_y * nearest_y;
        const double up_cleanup = minimum_up_squared < std::numeric_limits<double>::min() ? 2e-12 : 1e-12;
        const double margin = 2e-12 / direction_scale * maximum_up + up_cleanup * maximum_direction +
            4e-24 / direction_scale + 128 * std::numeric_limits<double>::epsilon();
        if (minimum <= margin || minimum_up_squared == 0)
        {
            throw std::invalid_argument("unsupported Id 3D camera view-up orientation or normalization interval");
        }
        if (zero_axis < 0)
        {
            // Oblique component cleanup can leave the plane. Bound both
            // normalized right vectors before comparing source camera up.
            const double cleanup = std::sqrt(3.0) * 1e-12;
            const double direction_error = 2 * cleanup / direction_scale + 2 * PLANE_TOLERANCE;
            const double up_error = std::sqrt(3.0) * up_cleanup + 2 * PLANE_TOLERANCE;
            const double cross_error = (maximum_direction + direction_error) * up_error + maximum_up * direction_error;
            if (minimum <= cross_error || minimum_horizontal <= direction_error ||
                4 * cross_error / (minimum - cross_error) +
                        4 * direction_error / (minimum_horizontal - direction_error) >=
                    5e-10)
            {
                throw std::invalid_argument(
                    "unsupported Id 3D camera view-up; cannot bound oblique roll within source tolerance");
            }
        }
    };
    validate(0, 1);
}

bool id_view_certify_up_interval(const std::array<std::array<double, 9>, 2> &values, int depth, int &budget)
{
    if (--budget < 0)
    {
        return false;
    }
    constexpr double ROUNDING = 32 * std::numeric_limits<double>::epsilon();
    std::array<std::array<double, 3>, 2> direction{};
    std::array<std::array<double, 3>, 2> up{};
    double direction_scale = 0;
    double up_scale_squared = 0;
    double direction_error = 0;
    double maximum_input = 0;
    const auto minimum_absolute = [](double first, double last)
    {
        return first * last <= 0 ? 0 : std::min(std::abs(first), std::abs(last));
    };
    for (int axis = 0; axis < 3; ++axis)
    {
        for (int offset : {0, 3})
        {
            const double first = values[0][offset + axis];
            const double last = values[1][offset + axis];
            maximum_input = std::max({maximum_input, std::abs(first), std::abs(last)});
            const bool cleaned = std::max(std::abs(first), std::abs(last)) < 1e-12;
            if (!cleaned && minimum_absolute(first, last) < 1e-12)
            {
                direction_error += 1e-12;
            }
            for (int point = 0; point < 2; ++point)
            {
                direction[point][axis] += (offset == 0 ? -1 : 1) * (cleaned ? 0 : values[point][offset + axis]);
            }
        }
        for (int point = 0; point < 2; ++point)
        {
            direction_scale = std::max(direction_scale, std::abs(direction[point][axis]));
            up[point][axis] = values[point][6 + axis];
        }
    }
    for (const std::array<double, 3> &point : up)
    {
        const double squared = point[0] * point[0] + point[1] * point[1] + point[2] * point[2];
        if (!std::isfinite(squared) || squared == 0)
        {
            return false;
        }
        up_scale_squared = std::max(up_scale_squared, squared);
    }
    if (!std::isfinite(direction_scale) || direction_scale == 0)
    {
        return false;
    }
    const double up_scale = std::sqrt(up_scale_squared);
    direction_error = (direction_error + ROUNDING * maximum_input) / direction_scale;
    for (int point = 0; point < 2; ++point)
    {
        for (int axis = 0; axis < 3; ++axis)
        {
            direction[point][axis] /= direction_scale;
            up[point][axis] /= up_scale;
        }
    }
    const auto minimum_length = [](const std::array<std::array<double, 3>, 2> &points, bool horizontal)
    {
        double squared = 0;
        double projection = 0;
        for (int axis = 0; axis < 3; ++axis)
        {
            if (!horizontal || axis != 1)
            {
                const double delta = points[1][axis] - points[0][axis];
                squared += delta * delta;
                projection += points[0][axis] * delta;
            }
        }
        const double fraction = squared == 0 ? 0 : std::clamp(-projection / squared, 0.0, 1.0);
        double length_squared = 0;
        for (int axis = 0; axis < 3; ++axis)
        {
            if (!horizontal || axis != 1)
            {
                const double component = points[0][axis] + fraction * (points[1][axis] - points[0][axis]);
                length_squared += component * component;
            }
        }
        return std::sqrt(length_squared);
    };
    const double minimum_up = minimum_length(up, false);
    const double minimum_up_squared = minimum_up * minimum_up * up_scale_squared;
    if (minimum_up_squared == 0)
    {
        return false;
    }
    const double up_cleanup = minimum_up_squared < std::numeric_limits<double>::min() ? 2e-12 : 1e-12;
    double up_error = 0;
    for (int axis = 0; axis < 3; ++axis)
    {
        const double maximum = std::max(std::abs(up[0][axis]), std::abs(up[1][axis]));
        if (maximum < 1e-12 / std::sqrt(2.0) * minimum_up * (1 - ROUNDING))
        {
            up[0][axis] = 0;
            up[1][axis] = 0;
        }
        else if (minimum_absolute(up[0][axis], up[1][axis]) < up_cleanup * (1 + ROUNDING))
        {
            up_error += up_cleanup;
        }
    }
    const auto cross = [](const std::array<double, 3> &first, const std::array<double, 3> &last)
    {
        return std::array<double, 3>{first[1] * last[2] - first[2] * last[1], first[2] * last[0] - first[0] * last[2],
            first[0] * last[1] - first[1] * last[0]};
    };
    const auto dot = [](const std::array<double, 3> &first, const std::array<double, 3> &last)
    {
        return first[0] * last[0] + first[1] * last[1] + first[2] * last[2];
    };
    const auto quadratic_minimum = [](double first, double middle, double last)
    {
        const double a = first - 2 * middle + last;
        const double b = 2 * (middle - first);
        const double fraction = a > 0 ? std::clamp(-b / (2 * a), 0.0, 1.0) : 0;
        return std::min({first, last, (a * fraction + b) * fraction + first});
    };
    // Bernstein cross-product controls enclose every continuous sample.
    // Principal planes retain an exact quadratic orientation minimum.
    std::array<std::array<double, 3>, 3> right{cross(direction[0], up[0]), {}, cross(direction[1], up[1])};
    const std::array<double, 3> first_middle = cross(direction[0], up[1]);
    const std::array<double, 3> last_middle = cross(direction[1], up[0]);
    for (int axis = 0; axis < 3; ++axis)
    {
        right[1][axis] = (first_middle[axis] + last_middle[axis]) / 2;
    }
    const double maximum_direction =
        std::max(std::sqrt(dot(direction[0], direction[0])), std::sqrt(dot(direction[1], direction[1])));
    const double maximum_up = std::max(std::sqrt(dot(up[0], up[0])), std::sqrt(dot(up[1], up[1])));
    const double minimum_horizontal = minimum_length(direction, true);
    const double cross_error = (maximum_direction + direction_error) * up_error + maximum_up * direction_error +
        ROUNDING * maximum_direction * maximum_up;
    for (int axis : {0, 2})
    {
        if (direction[0][axis] == 0 && direction[1][axis] == 0 && up[0][axis] == 0 && up[1][axis] == 0)
        {
            const int horizontal_axis = 2 - axis;
            const double sign = (axis == 0 ? -1 : 1) * (direction[0][horizontal_axis] < 0 ? -1 : 1);
            const double minimum =
                quadratic_minimum(sign * right[0][axis], sign * right[1][axis], sign * right[2][axis]);
            if (direction[0][horizontal_axis] * direction[1][horizontal_axis] > 0 &&
                minimum_horizontal > direction_error && minimum > cross_error)
            {
                return true;
            }
        }
    }
    const std::array<double, 3> world_first{-direction[0][2], 0, direction[0][0]};
    const std::array<double, 3> world_last{-direction[1][2], 0, direction[1][0]};
    const double minimum_orientation =
        std::min({dot(right[0], world_first), (2 * dot(right[1], world_first) + dot(right[0], world_last)) / 3,
            (dot(right[2], world_first) + 2 * dot(right[1], world_last)) / 3, dot(right[2], world_last)});
    const double maximum_horizontal =
        std::max(std::hypot(direction[0][0], direction[0][2]), std::hypot(direction[1][0], direction[1][2]));
    const double minimum_cross = minimum_orientation / maximum_horizontal;
    const double maximum_roll = std::max({std::abs(right[0][1]), std::abs(right[1][1]), std::abs(right[2][1])});
    if (minimum_horizontal > direction_error && minimum_cross > cross_error &&
        2 * maximum_direction * maximum_roll / minimum_orientation + 4 * cross_error / (minimum_cross - cross_error) +
                4 * direction_error / (minimum_horizontal - direction_error) <
            1e-9)
    {
        return true;
    }
    if (depth == 16)
    {
        return false;
    }
    std::array<double, 9> middle{};
    for (int component = 0; component < 9; ++component)
    {
        middle[component] = values[0][component] + (values[1][component] - values[0][component]) / 2;
    }
    return id_view_certify_up_interval({values[0], middle}, depth + 1, budget) &&
        id_view_certify_up_interval({middle, values[1]}, depth + 1, budget);
}

bool id_view_certify_up(const std::vector<timeline::Lane> &signals, timeline::Time start, timeline::Time end)
{
    std::array<std::array<double, 9>, 2> values{};
    for (int component = 0; component < 9; ++component)
    {
        values[0][component] = camera2d_sample(signals[component], start);
        values[1][component] = camera2d_segment_end(signals[component], start, end);
    }
    int budget = 4096;
    return id_view_certify_up_interval(values, 0, budget);
}

void id_view_validate_up(const std::vector<timeline::Lane> &signals, timeline::Time start, timeline::Time end)
{
    bool straight = true;
    for (int point = 0; point < 3; ++point)
    {
        const auto sample = [&](int component)
        {
            return clean_path_value(point == 1 ? camera2d_segment_end(signals[component], start, end)
                                               : camera2d_sample(signals[component], point == 0 ? start : end));
        };
        straight = straight && sample(0) == sample(3) && sample(1) == sample(4) && sample(2) > sample(5);
    }
    if (straight && camera3d_straight_up(signals, start, end))
    {
        return;
    }
    // A held destination may have its own viewing plane and normalization scale.
    for (const std::pair<timeline::Time, timeline::Time> &interval : {std::pair{start, end}, std::pair{end, end}})
    {
        try
        {
            id_view_validate_up_interval(signals, interval.first, interval.second);
        }
        catch (const std::invalid_argument &)
        {
            if (!id_view_certify_up(signals, interval.first, interval.second))
            {
                throw;
            }
        }
    }
}

std::pair<double, double> id_view_validate_camera(
    const std::vector<timeline::Lane> &signals, timeline::Time start, timeline::Time end)
{
    for (timeline::Time time : {start, end})
    {
        for (int component = 0; component < 3; ++component)
        {
            if (std::abs(clean_path_value(camera2d_sample(signals[3 + component], time))) >= 1e-9)
            {
                throw std::invalid_argument("Id 3D camera requires a centered look-at");
            }
        }
    }
    id_view_validate_up(signals, start, end);
    std::array<double, 3> from{};
    std::array<double, 3> to{};
    std::array<double, 3> last{};
    for (int component = 0; component < 3; ++component)
    {
        from[component] = clean_path_value(camera2d_sample(signals[3 + component], start)) -
            clean_path_value(camera2d_sample(signals[component], start));
        to[component] = clean_path_value(camera2d_segment_end(signals[3 + component], start, end)) -
            clean_path_value(camera2d_segment_end(signals[component], start, end));
        last[component] = clean_path_value(camera2d_sample(signals[3 + component], end)) -
            clean_path_value(camera2d_sample(signals[component], end));
    }
    const double dx = to[0] - from[0];
    const double dy = to[1] - from[1];
    const double dz = to[2] - from[2];
    const double projected = dx * dx + dz * dz;
    const double fraction = projected == 0 ? 0 : std::clamp(-(from[0] * dx + from[2] * dz) / projected, 0.0, 1.0);
    // Cleanup of both eye/look horizontal components can erase a direction
    // whose norm exceeds one cleanup threshold. Certify that small region.
    if ((std::hypot(from[0] + fraction * dx, from[2] + fraction * dz) <= 3e-12 ||
            std::hypot(last[0], last[2]) <= 3e-12) &&
        (!id_view_certify_up(signals, start, end) || !id_view_certify_up(signals, end, end)))
    {
        throw std::invalid_argument("Id 3D camera has a vertical or zero direction");
    }
    const auto distance = [](const std::array<double, 3> &point)
    {
        return std::sqrt(point[0] * point[0] + point[1] * point[1] + point[2] * point[2]);
    };
    const double maximum = std::max({distance(from), distance(to), distance(last)});
    if (!std::isfinite(maximum) || std::round(maximum) > std::numeric_limits<int>::max())
    {
        throw std::invalid_argument("Id 3D camera perspective exceeds the source integer range");
    }
    const double squared = projected + dy * dy;
    const double nearest =
        squared == 0 ? 0 : std::clamp(-(from[0] * dx + from[1] * dy + from[2] * dz) / squared, 0.0, 1.0);
    const double minimum =
        std::min(distance({from[0] + nearest * dx, from[1] + nearest * dy, from[2] + nearest * dz}), distance(last));
    return {std::floor(minimum), std::ceil(maximum)};
}

void animation_id_view_lanes(const Json &track, const Json &catalog, const std::filesystem::path &source_path,
    const Json &config, const std::string &id, const std::string &layer, const timeline::FrameGrid &grid,
    std::vector<timeline::Lane> &lanes)
{
    std::vector<std::string> fields{"name", "type", "outputs", "camera3d"};
    std::vector<std::string> output_fields;
    for (const ViewMember &member : ID_VIEW_MEMBERS)
    {
        fields.push_back(member.name);
        output_fields.push_back(member.name);
    }
    view_fields(track, fields, "Id 3D");
    const std::string name = track.at("name").get<std::string>();
    const std::string label = layer.empty() ? name : layer + " / " + name;
    const Json &outputs = track.at("outputs");
    view_fields(outputs, output_fields, "Id 3D");
    if (outputs.empty())
    {
        throw std::invalid_argument("Id 3D view requires at least one output");
    }
    const std::map<std::string, std::string> source = animation_source(source_path, config);
    const timeline::Time start = grid.offset();
    const timeline::Time end = grid.frame_start(grid.frame_count() - 1);
    timeline::Attributes attributes{{"id-3d-view", track.dump()}, {"view-name", name}, {"layer", layer}, {"track", id},
        {"source-entry", config.at("source").at("name").get<std::string>()},
        {"source-file", (source_path.parent_path() / config.at("source").at("file").get<std::string>()).string()}};
    std::vector<timeline::Lane> signals;
    if (track.contains("camera3d"))
    {
        const Json &camera = track.at("camera3d");
        view_fields(camera, {"eye", "look-at", "view-up"}, "Id 3D");
        for (const std::string &member : {"eye", "look-at", "view-up"})
        {
            const Json metadata{{"type", member == "view-up" ? "vector3" : "point3"}};
            std::vector<timeline::Lane> input = view_keys(
                camera.at(member), metadata, member, id + "-" + member, label + " / " + member, layer, grid, "Id 3D");
            for (timeline::Lane &lane : input)
            {
                signals.push_back(std::move(lane));
            }
        }
    }
    const bool derived =
        std::any_of(ID_VIEW_MEMBERS.begin(), ID_VIEW_MEMBERS.end(), [&track, &outputs](const ViewMember &member)
            { return member.camera && outputs.contains(member.name) && !track.contains(member.name); });
    std::pair<double, double> camera_bounds{0, 0};
    double up_extent = 1;
    if (derived)
    {
        if (signals.empty())
        {
            throw std::invalid_argument("Id 3D output requires an explicit member or camera3d");
        }
        up_extent = camera3d_normalization_bounds(signals, start, end);
        camera_bounds = id_view_validate_camera(signals, start, end);
    }
    std::vector<timeline::Lane> authored;
    for (const ViewMember &member : ID_VIEW_MEMBERS)
    {
        if (!outputs.contains(member.name))
        {
            if (track.contains(member.name))
            {
                throw std::invalid_argument("Id 3D member requires its output declaration");
            }
            continue;
        }
        const std::string parameter = outputs.at(member.name).get<std::string>();
        const Json &parameters = catalog.at("parameters");
        if (!parameters.contains(parameter))
        {
            throw std::invalid_argument("Id 3D output is missing from the catalog");
        }
        const Json &metadata = parameters.at(parameter);
        if (metadata.at("type") != member.type ||
            (member.components > 1 && metadata.at("arity") != member.components) ||
            metadata.value("extrapolate", std::string("clamp")) != "clamp")
        {
            throw std::invalid_argument("Id 3D output catalog has an incompatible type, arity, or extrapolation");
        }
        timeline::Attributes output_attributes = attributes;
        output_attributes["parameter"] = parameter;
        output_attributes["member"] = member.name;
        output_attributes["source-value"] = source.count(parameter) ? source.at(parameter) : "";
        std::vector<timeline::Lane> input;
        if (track.contains(member.name))
        {
            input = view_keys(track.at(member.name), metadata, member.name, id + "-" + member.name,
                label + " / " + member.name, layer, grid, "Id 3D");
        }
        else if (!member.camera || signals.empty())
        {
            throw std::invalid_argument("Id 3D output requires an explicit member");
        }
        if (!input.empty() && member.type != "integer")
        {
            for (int component = 0; component < timeline::size_cast(input); ++component)
            {
                output_attributes["component"] = std::to_string(component);
                output_attributes["signal"] = track.at(member.name).dump();
                lanes.push_back(view_annotated(input[component], output_attributes));
            }
            continue;
        }
        for (int component = 0; component < member.components; ++component)
        {
            const std::string suffix = member.components == 1 ? "" : "[" + std::to_string(component) + "]";
            timeline::Lane lane(
                id + "-" + member.name + suffix, label + " / " + member.name + suffix, "curve", start, grid.end_time());
            output_attributes["component"] = std::to_string(component);
            output_attributes["derived-from"] = input.empty() ? "camera3d" : "keys";
            double minimum = member.name == "rotation" ? -180 : 0;
            double maximum = member.name == "rotation" ? 180 : 0;
            std::function<double(timeline::Time)> evaluate;
            if (!input.empty())
            {
                const timeline::Lane keys = input.front();
                const std::pair<double, double> bounds = camera2d_bounds(keys, start, end);
                minimum = bounds.first;
                maximum = bounds.second;
                evaluate = [keys](timeline::Time time)
                {
                    return std::round(*keys.evaluate_keyframes(time));
                };
                output_attributes["signal"] = track.at(member.name).dump();
                timeline::Lane definition(
                    lane.id() + "-keys", lane.label() + " keys", "keyframes", start, grid.end_time());
                for (const timeline::Item &item : keys.items())
                {
                    definition.add(std::get<timeline::Keyframe>(item));
                }
                authored.push_back(view_annotated(definition, output_attributes));
            }
            else
            {
                const std::array<timeline::Lane, 6> inputs{
                    signals[0], signals[1], signals[2], signals[3], signals[4], signals[5]};
                evaluate = [inputs, member, component](timeline::Time time)
                {
                    std::array<double, 3> target{};
                    for (int axis = 0; axis < 3; ++axis)
                    {
                        target[axis] = clean_path_value(*inputs[axis + 3].evaluate_keyframes(time)) -
                            clean_path_value(*inputs[axis].evaluate_keyframes(time));
                    }
                    const double distance =
                        std::sqrt(target[0] * target[0] + target[1] * target[1] + target[2] * target[2]);
                    if (member.name == "perspective")
                    {
                        return std::round(distance);
                    }
                    if (member.name == "xyshift" || component == 2)
                    {
                        return 0.0;
                    }
                    for (double &value : target)
                    {
                        value /= distance;
                    }
                    constexpr double PI = 3.14159265358979323846;
                    return clean_path_value(component == 0
                            ? -std::atan2(target[1], std::hypot(target[0], target[2])) * 180 / PI
                            : std::atan2(target[0], -target[2]) * 180 / PI);
                };
                if (member.name == "perspective")
                {
                    minimum = camera_bounds.first;
                    maximum = camera_bounds.second;
                }
            }
            lane.add(timeline::Curve(lane.id() + "-view", "id-3d-view", start, end, evaluate, lane.label(), minimum,
                maximum, output_attributes));
            lanes.push_back(std::move(lane));
        }
    }
    for (timeline::Lane &lane : authored)
    {
        lanes.push_back(std::move(lane));
    }
    for (int component = 0; component < timeline::size_cast(signals); ++component)
    {
        const std::string member = component < 3 ? "eye" : component < 6 ? "look-at" : "view-up";
        timeline::Attributes input_attributes = attributes;
        input_attributes["parameter"] = name + "." + member;
        input_attributes["component"] = std::to_string(component % 3);
        input_attributes["signal"] = track.at("camera3d").at(member).dump();
        input_attributes["used-by-camera"] = derived ? "true" : "false";
        if (member == "view-up" && derived)
        {
            const std::array<timeline::Lane, 3> inputs{signals[6], signals[7], signals[8]};
            timeline::Lane lane(signals[component].id(), signals[component].label(), "curve", start, grid.end_time());
            input_attributes["normalize"] = "true";
            lane.add(timeline::Curve(
                lane.id() + "-normalized", "id-3d-input", start, end,
                [inputs, axis = component % 3](timeline::Time time)
                {
                    std::array<double, 3> up{};
                    for (int index = 0; index < 3; ++index)
                    {
                        up[index] = *inputs[index].evaluate_keyframes(time);
                    }
                    const double length = std::sqrt(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]);
                    return clean_path_value(up[axis] / length);
                },
                lane.label(), -up_extent, up_extent, input_attributes));
            lanes.push_back(std::move(lane));
        }
        else
        {
            lanes.push_back(view_annotated(signals[component], input_attributes));
        }
    }
}

const std::array<ViewMember, 4> JULIBROT_VIEW_MEMBERS{
    ViewMember{"mode", "enum", 1, false},
    ViewMember{"geometry", "numeric-tuple", 6, true},
    ViewMember{"eyes", "double", 1, false},
    ViewMember{"from-to", "numeric-tuple", 4, false},
};

/// Inclusive component bounds used to certify a Julibrot camera interval.
using JulibrotRange = std::pair<double, double>;

double julibrot_maximum_absolute(JulibrotRange range)
{
    return std::max(std::abs(range.first), std::abs(range.second));
}

double julibrot_minimum_absolute(JulibrotRange range)
{
    return range.first <= 0 && range.second >= 0 ? 0 : std::min(std::abs(range.first), std::abs(range.second));
}

JulibrotRange julibrot_product(JulibrotRange lhs, JulibrotRange rhs)
{
    const std::array<double, 4> products{
        lhs.first * rhs.first, lhs.first * rhs.second, lhs.second * rhs.first, lhs.second * rhs.second};
    return {*std::min_element(products.begin(), products.end()), *std::max_element(products.begin(), products.end())};
}

JulibrotRange julibrot_difference(JulibrotRange lhs, JulibrotRange rhs)
{
    return {lhs.first - rhs.second, lhs.second - rhs.first};
}

JulibrotRange julibrot_clean_range(JulibrotRange range)
{
    return {clean_path_value(range.first), clean_path_value(range.second)};
}

double julibrot_camera_distance(const std::array<double, 9> &camera)
{
    std::array<double, 3> forward{};
    std::array<double, 3> hint{};
    const double hint_length = std::sqrt(camera[6] * camera[6] + camera[7] * camera[7] + camera[8] * camera[8]);
    for (int axis = 0; axis < 3; ++axis)
    {
        const double look = clean_path_value(camera[3 + axis]);
        if (std::abs(look) >= 1e-9)
        {
            throw std::invalid_argument("Julibrot camera requires a centered look-at");
        }
        forward[axis] = look - clean_path_value(camera[axis]);
        hint[axis] = clean_path_value(camera[6 + axis] / hint_length);
    }
    const double distance = std::sqrt(forward[0] * forward[0] + forward[1] * forward[1] + forward[2] * forward[2]);
    if (distance == 0 || !std::isfinite(distance))
    {
        throw std::invalid_argument("Julibrot camera requires a finite nondegenerate straight-on direction");
    }
    for (double &component : forward)
    {
        component /= distance;
    }
    if (std::abs(forward[0]) >= 1e-9 || std::abs(forward[1]) >= 1e-9 || std::abs(forward[2] + 1) >= 1e-9)
    {
        throw std::invalid_argument("unsupported Julibrot camera; requires a straight-on direction");
    }
    const double cleaned_length = std::sqrt(hint[0] * hint[0] + hint[1] * hint[1] + hint[2] * hint[2]);
    if (hint_length == 0 || !std::isfinite(hint_length) || cleaned_length == 0 || !std::isfinite(cleaned_length))
    {
        throw std::invalid_argument("Julibrot camera view-up has an invalid normalization length");
    }
    for (double &component : hint)
    {
        component /= cleaned_length;
    }
    std::array<double, 3> right{forward[1] * hint[2] - forward[2] * hint[1],
        forward[2] * hint[0] - forward[0] * hint[2], forward[0] * hint[1] - forward[1] * hint[0]};
    const double right_length = std::sqrt(right[0] * right[0] + right[1] * right[1] + right[2] * right[2]);
    if (right_length == 0 || !std::isfinite(right_length))
    {
        throw std::invalid_argument("Julibrot camera view-up is parallel to its direction");
    }
    for (double &component : right)
    {
        component /= right_length;
    }
    const std::array<double, 3> up{right[1] * forward[2] - right[2] * forward[1],
        right[2] * forward[0] - right[0] * forward[2], right[0] * forward[1] - right[1] * forward[0]};
    if (std::abs(up[0]) >= 1e-9 || std::abs(up[1] - 1) >= 1e-9 || std::abs(up[2]) >= 1e-9)
    {
        throw std::invalid_argument("unsupported Julibrot camera view-up; requires a straight-on camera frame");
    }
    return distance;
}

bool julibrot_camera_interval(
    const std::array<double, 9> &first, const std::array<double, 9> &last, JulibrotRange &distance)
{
    constexpr double ROUNDING = 32 * std::numeric_limits<double>::epsilon();
    std::array<JulibrotRange, 9> input{};
    for (int component = 0; component < 9; ++component)
    {
        const double minimum = std::min(first[component], last[component]);
        const double maximum = std::max(first[component], last[component]);
        const double margin =
            first[component] == last[component] ? 0 : ROUNDING * std::max(std::abs(minimum), std::abs(maximum));
        input[component] = {minimum - margin, maximum + margin};
    }
    std::array<JulibrotRange, 3> direction{};
    double minimum_squared = 0;
    double maximum_squared = 0;
    double hint_minimum_squared = 0;
    double hint_maximum_squared = 0;
    for (int axis = 0; axis < 3; ++axis)
    {
        const JulibrotRange look = julibrot_clean_range(input[3 + axis]);
        if (julibrot_maximum_absolute(look) >= 1e-9)
        {
            return false;
        }
        direction[axis] = julibrot_difference(look, julibrot_clean_range(input[axis]));
        const double minimum = julibrot_minimum_absolute(direction[axis]);
        const double maximum = julibrot_maximum_absolute(direction[axis]);
        minimum_squared += minimum * minimum;
        maximum_squared += maximum * maximum;
        const double hint_minimum = julibrot_minimum_absolute(input[6 + axis]);
        const double hint_maximum = julibrot_maximum_absolute(input[6 + axis]);
        hint_minimum_squared += hint_minimum * hint_minimum;
        hint_maximum_squared += hint_maximum * hint_maximum;
    }
    if (direction[2].second >= 0 || minimum_squared < std::numeric_limits<double>::min() ||
        !std::isfinite(maximum_squared) || hint_minimum_squared == 0 || !std::isfinite(hint_maximum_squared))
    {
        return false;
    }
    distance = {std::sqrt(minimum_squared), std::sqrt(maximum_squared)};
    const double minimum_z = -direction[2].second;
    const double maximum_z = -direction[2].first;
    const JulibrotRange inverse_z{1 / maximum_z, 1 / minimum_z};
    const JulibrotRange x = julibrot_product(direction[0], inverse_z);
    const JulibrotRange y = julibrot_product(direction[1], inverse_z);
    const double forward_x = julibrot_maximum_absolute(x) * (1 + ROUNDING);
    const double forward_y = julibrot_maximum_absolute(y) * (1 + ROUNDING);
    if (forward_x >= 1e-9 || forward_y >= 1e-9)
    {
        return false;
    }
    const JulibrotRange inverse_hint{1 / std::sqrt(hint_maximum_squared), 1 / std::sqrt(hint_minimum_squared)};
    std::array<JulibrotRange, 3> hint{};
    for (int axis = 0; axis < 3; ++axis)
    {
        JulibrotRange normalized = julibrot_product(input[6 + axis], inverse_hint);
        const double margin = ROUNDING * julibrot_maximum_absolute(normalized);
        hint[axis] = julibrot_clean_range({normalized.first - margin, normalized.second + margin});
    }
    // Positive normalization scales cancel from the right-vector direction.
    // Keep source hint cleanup before taking this cross product. Small-angle
    // bounds avoid losing the strict tolerance in a broad [-1, -1] enclosure.
    const JulibrotRange yz = julibrot_product(y, hint[2]);
    const JulibrotRange xz = julibrot_product(x, hint[2]);
    const JulibrotRange right_x{yz.first + hint[1].first, yz.second + hint[1].second};
    const JulibrotRange right_y{-hint[0].second - xz.second, -hint[0].first - xz.first};
    const JulibrotRange right_z = julibrot_difference(julibrot_product(x, hint[1]), julibrot_product(y, hint[0]));
    const double minimum_right_x =
        right_x.first - ROUNDING * (julibrot_maximum_absolute(yz) + julibrot_maximum_absolute(hint[1]));
    if (minimum_right_x <= 0)
    {
        return false;
    }
    const double right_y_ratio = (julibrot_maximum_absolute(right_y) +
                                     ROUNDING * (julibrot_maximum_absolute(hint[0]) + julibrot_maximum_absolute(xz))) /
        minimum_right_x;
    const double right_z_ratio =
        (julibrot_maximum_absolute(right_z) +
            ROUNDING *
                (forward_x * julibrot_maximum_absolute(hint[1]) + forward_y * julibrot_maximum_absolute(hint[0]))) /
        minimum_right_x;
    const double up_x = (right_y_ratio + right_z_ratio * forward_y) * (1 + ROUNDING);
    const double up_z = (forward_y + right_y_ratio * forward_x) * (1 + ROUNDING);
    const double up_y_error = (right_y_ratio * right_y_ratio + right_z_ratio * right_z_ratio + forward_x * forward_x +
                                  forward_y * forward_y) /
            2 +
        right_z_ratio * forward_x + ROUNDING;
    return up_x < 1e-9 && up_z < 1e-9 && up_y_error < 1e-9;
}

JulibrotRange julibrot_camera_subdivide(
    const std::array<double, 9> &first, const std::array<double, 9> &last, int depth, int &budget)
{
    const double first_distance = julibrot_camera_distance(first);
    const double last_distance = julibrot_camera_distance(last);
    if (first == last)
    {
        return {first_distance, last_distance};
    }
    JulibrotRange distance;
    if (julibrot_camera_interval(first, last, distance))
    {
        return distance;
    }
    if (depth == 16 || --budget == 0)
    {
        throw std::invalid_argument("unsupported Julibrot straight-on direction or view-up tolerance interval");
    }
    std::array<double, 9> middle{};
    for (int component = 0; component < 9; ++component)
    {
        middle[component] = first[component] + (last[component] - first[component]) / 2;
    }
    const JulibrotRange left = julibrot_camera_subdivide(first, middle, depth + 1, budget);
    const JulibrotRange right = julibrot_camera_subdivide(middle, last, depth + 1, budget);
    return {std::min(left.first, right.first), std::max(left.second, right.second)};
}

std::pair<double, double> julibrot_camera_bounds(
    const std::vector<timeline::Lane> &signals, timeline::Time start, timeline::Time end)
{
    std::array<double, 9> first{};
    std::array<double, 9> last{};
    std::array<double, 9> endpoint{};
    bool straight = true;
    for (int component = 0; component < 9; ++component)
    {
        first[component] = camera2d_sample(signals[component], start);
        last[component] = camera2d_segment_end(signals[component], start, end);
        endpoint[component] = camera2d_sample(signals[component], end);
        if (component < 6 && component % 3 != 2)
        {
            straight = straight && clean_path_value(first[component]) == 0 && clean_path_value(last[component]) == 0 &&
                clean_path_value(endpoint[component]) == 0;
        }
    }
    const double endpoint_distance = julibrot_camera_distance(endpoint);
    julibrot_camera_distance(first);
    julibrot_camera_distance(last);
    if (straight && camera3d_straight_up(signals, start, end))
    {
        // Preserve the local-ratio proof for exact-axis hints whose magnitude
        // spans the subnormal and large finite ranges.
        const std::pair<double, double> eye = camera2d_bounds(signals[2], start, end);
        const std::pair<double, double> look = camera2d_bounds(signals[5], start, end);
        const double minimum = clean_path_value(eye.first) - clean_path_value(look.second);
        const double maximum = clean_path_value(eye.second) - clean_path_value(look.first);
        if (minimum > 0 && minimum * minimum > 0 && std::isfinite(maximum * maximum))
        {
            return {std::min(std::sqrt(minimum * minimum), endpoint_distance),
                std::max(std::sqrt(maximum * maximum), endpoint_distance)};
        }
    }
    int budget = 4096;
    const JulibrotRange distance = julibrot_camera_subdivide(first, last, 0, budget);
    return {std::min(distance.first, endpoint_distance), std::max(distance.second, endpoint_distance)};
}

void animation_julibrot_view_lanes(const Json &track, const Json &catalog, const std::filesystem::path &source_path,
    const Json &config, const std::string &id, const std::string &layer, const timeline::FrameGrid &grid,
    std::vector<timeline::Lane> &lanes)
{
    view_fields(track, {"name", "type", "outputs", "camera3d", "mode", "geometry", "eyes", "from-to"}, "Julibrot");
    const std::string name = track.at("name").get<std::string>();
    if (name.empty())
    {
        throw std::invalid_argument("Julibrot view requires a nonempty name");
    }
    const std::string label = layer.empty() ? name : layer + " / " + name;
    const Json &outputs = track.at("outputs");
    view_fields(outputs, {"mode", "geometry", "eyes", "from-to"}, "Julibrot");
    if (outputs.empty())
    {
        throw std::invalid_argument("Julibrot view requires at least one output");
    }
    const std::map<std::string, std::string> source = animation_source(source_path, config);
    const timeline::Time start = grid.offset();
    const timeline::Time end = grid.frame_start(grid.frame_count() - 1);
    const timeline::Attributes attributes{{"julibrot-view", track.dump()}, {"view-name", name}, {"layer", layer},
        {"track", id}, {"source-entry", config.at("source").at("name").get<std::string>()},
        {"source-file", (source_path.parent_path() / config.at("source").at("file").get<std::string>()).string()}};
    std::vector<timeline::Lane> signals;
    if (track.contains("camera3d"))
    {
        const Json &camera = track.at("camera3d");
        view_fields(camera, {"eye", "look-at", "view-up"}, "Julibrot");
        for (const std::string &member : {"eye", "look-at", "view-up"})
        {
            const Json metadata{{"type", member == "view-up" ? "vector3" : "point3"}};
            std::vector<timeline::Lane> input = view_keys(camera.at(member), metadata, member, id + "-" + member,
                label + " / " + member, layer, grid, "Julibrot");
            for (timeline::Lane &lane : input)
            {
                signals.push_back(std::move(lane));
            }
        }
    }
    const bool derived = outputs.contains("geometry") && !track.contains("geometry");
    std::pair<double, double> camera_bounds{0, 0};
    double up_extent = 1;
    if (derived)
    {
        if (signals.empty())
        {
            throw std::invalid_argument("Julibrot geometry output requires an explicit member or camera3d");
        }
        up_extent = camera3d_normalization_bounds(signals, start, end);
        camera_bounds = julibrot_camera_bounds(signals, start, end);
    }
    for (const ViewMember &member : JULIBROT_VIEW_MEMBERS)
    {
        if (!outputs.contains(member.name))
        {
            if (track.contains(member.name))
            {
                throw std::invalid_argument("Julibrot member requires its output declaration");
            }
            continue;
        }
        const std::string parameter = outputs.at(member.name).get<std::string>();
        const Json &parameters = catalog.at("parameters");
        if (!parameters.contains(parameter))
        {
            throw std::invalid_argument("Julibrot output is missing from the catalog");
        }
        const Json &metadata = parameters.at(parameter);
        if (metadata.at("type") != member.type ||
            (member.components > 1 && metadata.at("arity") != member.components) ||
            metadata.value("extrapolate", std::string("clamp")) != "clamp")
        {
            throw std::invalid_argument("Julibrot output catalog has an incompatible type, arity, or extrapolation");
        }
        timeline::Attributes output_attributes = attributes;
        output_attributes["parameter"] = parameter;
        output_attributes["member"] = member.name;
        output_attributes["source-value"] = source.count(parameter) ? source.at(parameter) : "";
        if (track.contains(member.name))
        {
            const Json &signal = track.at(member.name);
            if (member.name == "mode")
            {
                for (const Json &key : signal.at("keys"))
                {
                    const Json &value = key.at("value");
                    if (value != "monocular" && value != "lefteye" && value != "righteye" && value != "red-blue")
                    {
                        throw std::invalid_argument("Julibrot mode requires a supported enum value");
                    }
                }
            }
            const std::vector<timeline::Lane> input = view_keys(signal, metadata, member.name, id + "-" + member.name,
                label + " / " + member.name, layer, grid, "Julibrot");
            output_attributes["signal"] = signal.dump();
            if (!source.count(parameter))
            {
                const Json &value = signal.at("keys")[0].at("value");
                output_attributes["source-value"] = value.is_string() ? value.get<std::string>() : value.dump();
            }
            for (int component = 0; component < timeline::size_cast(input); ++component)
            {
                output_attributes["component"] = std::to_string(component);
                lanes.push_back(view_annotated(input[component], output_attributes));
            }
            continue;
        }
        if (!member.camera || signals.empty())
        {
            throw std::invalid_argument("Julibrot output requires an explicit member");
        }
        if (!source.count(parameter))
        {
            throw std::invalid_argument("Julibrot camera output requires source geometry");
        }
        const std::vector<double> base = animation_value(source.at(parameter));
        if (timeline::size_cast(base) != 6)
        {
            throw std::invalid_argument("Julibrot source geometry must have six values");
        }
        const std::array<timeline::Lane, 6> inputs{
            signals[0], signals[1], signals[2], signals[3], signals[4], signals[5]};
        for (int component = 0; component < 6; ++component)
        {
            const double constant = clean_path_value(base[component]);
            const double minimum = component == 5 ? camera_bounds.first : constant;
            const double maximum = component == 5 ? camera_bounds.second : constant;
            if ((metadata.contains("min") && minimum < metadata.at("min").get<double>()) ||
                (metadata.contains("max") && maximum > metadata.at("max").get<double>()))
            {
                throw std::invalid_argument("Julibrot geometry exceeds catalog bounds");
            }
            const std::string suffix = "[" + std::to_string(component) + "]";
            timeline::Lane lane(
                id + "-geometry" + suffix, label + " / geometry" + suffix, "curve", start, grid.end_time());
            output_attributes["component"] = std::to_string(component);
            output_attributes["derived-from"] = "camera3d";
            lane.add(timeline::Curve(
                lane.id() + "-view", "julibrot-view", start, end,
                [inputs, component, constant](timeline::Time time)
                {
                    if (component != 5)
                    {
                        return constant;
                    }
                    std::array<double, 3> target{};
                    for (int axis = 0; axis < 3; ++axis)
                    {
                        target[axis] = clean_path_value(*inputs[3 + axis].evaluate_keyframes(time)) -
                            clean_path_value(*inputs[axis].evaluate_keyframes(time));
                    }
                    return clean_path_value(
                        std::sqrt(target[0] * target[0] + target[1] * target[1] + target[2] * target[2]));
                },
                lane.label(), minimum, maximum, output_attributes));
            lanes.push_back(std::move(lane));
        }
    }
    for (int component = 0; component < timeline::size_cast(signals); ++component)
    {
        const std::string member = component < 3 ? "eye" : component < 6 ? "look-at" : "view-up";
        timeline::Attributes input_attributes = attributes;
        input_attributes["parameter"] = name + "." + member;
        input_attributes["component"] = std::to_string(component % 3);
        input_attributes["signal"] = track.at("camera3d").at(member).dump();
        input_attributes["used-by-camera"] = derived ? "true" : "false";
        if (member == "view-up" && derived)
        {
            const std::array<timeline::Lane, 3> inputs{signals[6], signals[7], signals[8]};
            timeline::Lane lane(signals[component].id(), signals[component].label(), "curve", start, grid.end_time());
            input_attributes["normalize"] = "true";
            lane.add(timeline::Curve(
                lane.id() + "-normalized", "julibrot-input", start, end,
                [inputs, axis = component % 3](timeline::Time time)
                {
                    std::array<double, 3> up{};
                    for (int index = 0; index < 3; ++index)
                    {
                        up[index] = *inputs[index].evaluate_keyframes(time);
                    }
                    const double length = std::sqrt(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]);
                    return clean_path_value(up[axis] / length);
                },
                lane.label(), -up_extent, up_extent, input_attributes));
            lanes.push_back(std::move(lane));
        }
        else
        {
            lanes.push_back(view_annotated(signals[component], input_attributes));
        }
    }
}

timeline::Ticks positive_remainder(timeline::Ticks value, timeline::Ticks period)
{
    const timeline::Ticks remainder = value % period;
    return remainder < 0 ? remainder + period : remainder;
}

void extrapolate_keyframes(const Json &track, const Json &metadata, const std::string &policy,
    const std::filesystem::path &source_path, const Json &config, const timeline::FrameGrid &grid,
    std::vector<timeline::Lane> &lanes)
{
    const std::string type = metadata.value("type", std::string{});
    if (!track.contains("keys") || track.value("mode", std::string("keyframes")) != "keyframes" ||
        (type != "integer" && type != "integer-or-enum" && type != "double") || timeline::size_cast(lanes) != 1 ||
        !std::holds_alternative<timeline::Keyframe>(lanes[0].items()[0]))
    {
        throw std::invalid_argument("non-clamp extrapolation requires numeric scalar keyframes");
    }
    const Json &keys = track.at("keys");
    if (timeline::size_cast(keys) != 2)
    {
        throw std::invalid_argument("non-clamp extrapolation requires exactly two keys");
    }
    if (type == "integer-or-enum" &&
        (!keys[0].at("value").is_number_integer() || !keys[1].at("value").is_number_integer()))
    {
        throw std::invalid_argument("integer-or-enum extrapolation requires JSON integer key values");
    }
    timeline::Lane &lane = lanes[0];
    for (const timeline::Item &item : lane.items())
    {
        const timeline::Keyframe &key = std::get<timeline::Keyframe>(item);
        if (key.interpolation() == timeline::KeyframeInterpolation::GEOMETRIC)
        {
            throw std::invalid_argument("scalar extrapolation supports linear, hold, or step curves");
        }
        if ((type == "integer" || type == "integer-or-enum") &&
            (key.value() != std::trunc(key.value()) || key.value() < std::numeric_limits<int>::min() ||
                key.value() > std::numeric_limits<int>::max()))
        {
            throw std::invalid_argument("integer extrapolation requires integral key values in int range");
        }
        if ((metadata.contains("min") && key.value() < metadata.at("min").get<double>()) ||
            (metadata.contains("max") && key.value() > metadata.at("max").get<double>()))
        {
            throw std::invalid_argument("extrapolation key value exceeds catalog bounds");
        }
    }
    std::optional<double> base;
    std::string base_text;
    if (policy == "base")
    {
        const std::map<std::string, std::string> source = animation_source(source_path, config);
        const std::string parameter = track.at("parameter").get<std::string>();
        if (source.count(parameter) == 0)
        {
            throw std::invalid_argument("base extrapolation requires a numeric source value");
        }
        base_text = source.at(parameter);
        const std::vector<double> parsed = animation_value(Json(base_text));
        if (timeline::size_cast(parsed) != 1)
        {
            throw std::invalid_argument("base extrapolation requires a numeric scalar source value");
        }
        base = parsed[0];
    }
    // Capture the ordinary lane before installing the recipe to avoid self-reference.
    const timeline::Lane authored = lane;
    const timeline::Time first = std::get<timeline::Keyframe>(lane.items()[0]).time();
    const timeline::Time last = std::get<timeline::Keyframe>(lane.items()[1]).time();
    const timeline::Ticks span = (last - first).ticks();
    const timeline::Ticks extra = policy == "cycle" ? grid.frame_duration().ticks() : span;
    if ((policy == "cycle" || policy == "ping-pong") && span > std::numeric_limits<timeline::Ticks>::max() - extra)
    {
        throw std::invalid_argument("extrapolation period exceeds timeline tick range");
    }
    const timeline::Ticks period = policy == "cycle" || policy == "ping-pong" ? span + extra : 1;
    timeline::KeyframeEvaluator evaluator = [authored, policy, first, last, span, period, base](
                                                timeline::Time time) -> std::optional<double>
    {
        if (time < first || last < time)
        {
            if (policy == "omit")
            {
                return std::nullopt;
            }
            if (policy == "base")
            {
                return base;
            }
            const timeline::Ticks phase = positive_remainder(
                positive_remainder(time.ticks(), period) - positive_remainder(first.ticks(), period), period);
            const timeline::Ticks offset = policy == "ping-pong" && phase > span ? period - phase : phase;
            time = first + timeline::Duration::from_ticks(std::min(offset, span));
        }
        return authored.evaluate_keyframes(time);
    };
    timeline::Lane decorated(lane.id(), lane.label(), lane.kind(), lane.start(), lane.end());
    for (const timeline::Item &item : lane.items())
    {
        const timeline::Keyframe &key = std::get<timeline::Keyframe>(item);
        timeline::Attributes attributes = key.attributes();
        attributes["extrapolate"] = policy;
        attributes["track-definition"] = track.dump();
        attributes["catalog-definition"] = metadata.dump();
        if (base)
        {
            attributes["source-value"] = base_text;
            attributes["source-entry"] = config.at("source").at("name").get<std::string>();
            attributes["source-file"] =
                (source_path.parent_path() / config.at("source").at("file").get<std::string>()).string();
        }
        decorated.add(
            timeline::Keyframe(key.id(), key.time(), key.value(), key.interpolation(), std::move(attributes)));
    }
    decorated.set_keyframe_evaluator(std::move(evaluator));
    integer_output(decorated, metadata);
    lane = std::move(decorated);
}

void animation_tracks(const Json &tracks, const Json &catalog, const std::string &layer, const std::string &prefix,
    const std::filesystem::path &source_path, const Json &config, const std::string &video,
    const timeline::FrameGrid &grid, std::vector<timeline::Lane> &lanes, std::vector<std::string> &diagnostics)
{
    int index = 0;
    for (const Json &track : tracks)
    {
        const std::string id = prefix + std::to_string(index++);
        try
        {
            if (track.value("type", std::string("parameter")) == "color-map")
            {
                std::vector<timeline::Lane> track_lanes;
                detail::color_map_lanes(track, source_path, id, layer, grid, track_lanes);
                for (timeline::Lane &lane : track_lanes)
                {
                    lanes.push_back(std::move(lane));
                }
                continue;
            }
            if (track.value("type", std::string("parameter")) == "julibrot-view")
            {
                std::vector<timeline::Lane> track_lanes;
                animation_julibrot_view_lanes(track, catalog, source_path, config, id, layer, grid, track_lanes);
                for (timeline::Lane &lane : track_lanes)
                {
                    lanes.push_back(std::move(lane));
                }
                continue;
            }
            if (track.value("type", std::string("parameter")) == "id-3d-view")
            {
                std::vector<timeline::Lane> track_lanes;
                animation_id_view_lanes(track, catalog, source_path, config, id, layer, grid, track_lanes);
                for (timeline::Lane &lane : track_lanes)
                {
                    lanes.push_back(std::move(lane));
                }
                continue;
            }
            if (track.value("type", std::string("parameter")) == "camera2d")
            {
                std::vector<timeline::Lane> track_lanes;
                animation_camera2d_lanes(track, catalog, source_path, config, video, id, layer, grid, track_lanes);
                for (timeline::Lane &lane : track_lanes)
                {
                    lanes.push_back(std::move(lane));
                }
                continue;
            }
            const std::string parameter = track.at("parameter").get<std::string>();
            if (parameter.empty())
            {
                throw std::invalid_argument("track parameter must not be empty");
            }
            const std::string mode = track.value("mode", std::string("keyframes"));
            const std::map<std::string, std::string> source = animation_source(source_path, config);
            timeline::Attributes source_attributes{{"source-entry", config.at("source").at("name").get<std::string>()},
                {"source-file",
                    (source_path.parent_path() / config.at("source").at("file").get<std::string>()).string()},
                {"track-definition", track.dump()}};
            const Json metadata = resolve_animation_target(catalog, parameter, source, source_attributes);
            source_attributes["catalog-definition"] = metadata.dump();
            const std::string extrapolation = metadata.value("extrapolate", std::string("clamp"));
            if (extrapolation != "clamp" && extrapolation != "base" && extrapolation != "omit" &&
                extrapolation != "cycle" && extrapolation != "ping-pong")
            {
                throw std::invalid_argument("unknown extrapolation policy: " + extrapolation);
            }
            const std::string label = layer.empty() ? parameter : layer + " / " + parameter;
            std::vector<timeline::Lane> track_lanes;
            if ((mode != "keyframes" && mode != "pwm") || track.value("type", std::string("parameter")) != "parameter")
            {
                throw std::invalid_argument("unsupported track mode or specialized track type");
            }
            if (track.contains("keys") && track.contains("path"))
            {
                throw std::invalid_argument("track cannot contain both keys and path");
            }
            if (!track.contains("keys") && !track.contains("path"))
            {
                throw std::invalid_argument("track requires keys or path");
            }
            const std::string path_kind = track.contains("path") && track.at("path").is_object()
                ? track.at("path").value("kind", std::string{})
                : "";
            if (mode == "pwm")
            {
                if (track.contains("path"))
                {
                    throw std::invalid_argument("PWM tracks cannot contain a path");
                }
                animation_pwm_lanes(track, metadata, id, label, parameter, layer, grid, source_attributes, track_lanes);
            }
            else if (path_kind == "circle" || path_kind == "ellipse" || path_kind == "lissajous" ||
                path_kind == "spiral")
            {
                animation_planar_lanes(
                    track.at("path"), metadata, id, label, parameter, layer, grid, source_attributes, track_lanes);
            }
            else if (path_kind == "bezier" || path_kind == "catmull-rom")
            {
                animation_control_point_lanes(
                    track, metadata, id, label, parameter, layer, grid, source_attributes, track_lanes);
            }
            else
            {
                validate_catalog_key_target(track, metadata);
                if (track.contains("path") && metadata.at("type") != "center-mag" && metadata.at("type") != "corners" &&
                    !metadata.contains("default-curve"))
                {
                    throw std::invalid_argument(metadata.value("normalize", false)
                            ? "vector normalization requires a catalog default-curve"
                            : "keyed target requires a catalog default-curve");
                }
                if (metadata.at("type") == "complex" && source_attributes.at("output-parameter") != "params")
                {
                    throw std::invalid_argument("unsupported keyed complex target without params slots");
                }
                animation_key_lanes(track, metadata, id, label, parameter, layer, grid, source_attributes, track_lanes);
                const std::string type = metadata.value("type", std::string{});
                if ((type == "vector2" || type == "vector3") && metadata.value("normalize", false))
                {
                    normalized_keyed_output(track, metadata, grid, track_lanes);
                }
            }
            if (extrapolation != "clamp")
            {
                extrapolate_keyframes(track, metadata, extrapolation, source_path, config, grid, track_lanes);
            }
            for (timeline::Lane &lane : track_lanes)
            {
                lanes.push_back(std::move(lane));
            }
        }
        catch (const std::exception &error)
        {
            diagnostics.push_back("ParAnimator " + id + ": " + error.what());
        }
    }
}

void import_par_animator(const std::filesystem::path &source_path, const Json &config, const JsonImportOptions &options,
    JsonImportResult &result)
{
    if (!validate_par_animator_root(config, result.diagnostics))
    {
        return;
    }

    try
    {
        const auto [track_count, keyframe_count] = summarize_par_animator_content(config);
        timeline::Metadata metadata(source_path.filename().string(), source_path.string());
        timeline::FrameGrid frame_grid(timeline::Timebase(options.ticks_per_second),
            config.at("num-frames").get<timeline::Ticks>(), options.frames_per_second_numerator,
            options.frames_per_second_denominator);
        const Json catalog = animation_catalog(source_path, config, result.diagnostics);
        std::vector<timeline::Lane> lanes;
        if (config.contains("tracks"))
        {
            animation_tracks(config.at("tracks"), catalog, "", "animation-", source_path, config,
                config.at("video").get<std::string>(), frame_grid, lanes, result.diagnostics);
        }
        else
        {
            int index = 0;
            for (const Json &layer : config.at("layers"))
            {
                animation_tracks(layer.at("tracks"), catalog, layer.at("id").get<std::string>(),
                    "animation-layer-" + std::to_string(index++) + "-", source_path, layer,
                    config.at("video").get<std::string>(), frame_grid, lanes, result.diagnostics);
            }
        }
        if (track_count > 0 && lanes.empty())
        {
            throw std::invalid_argument("no supported animation tracks were imported");
        }
        result.document.emplace(frame_grid, track_count, keyframe_count, std::move(metadata));
        for (timeline::Lane &lane : lanes)
        {
            result.document->add_lane(std::move(lane));
        }
    }
    catch (const std::exception &error)
    {
        result.document.reset();
        result.diagnostics.emplace_back("Unable to import ParAnimator config: " + std::string(error.what()));
    }
}

bool validate_overlay_string_field(
    const Json &object, std::string_view field, std::string_view context, std::vector<std::string> &diagnostics)
{
    const std::string name(field);
    if (!object.contains(name) || !object.at(name).is_string() || object.at(name).get<std::string>().empty())
    {
        diagnostics.emplace_back(std::string(context) + " requires non-empty string property '" + name + "'.");
        return false;
    }
    return true;
}

bool validate_beat_keys_overlay(const Json &config, std::vector<std::string> &diagnostics)
{
    if (!config.is_object())
    {
        diagnostics.emplace_back("Beat-keys overlay must be a JSON object.");
        return false;
    }

    bool valid = true;
    if (!config.contains("version") || !config.at("version").is_number_integer() ||
        config.at("version").get<timeline::Ticks>() != 1)
    {
        diagnostics.emplace_back("Unsupported beat-keys overlay version.");
        valid = false;
    }
    if (!config.contains("generator") || !config.at("generator").is_object())
    {
        diagnostics.emplace_back("Beat-keys overlay requires a generator object.");
        valid = false;
    }
    else
    {
        const Json &generator = config.at("generator");
        valid = validate_overlay_string_field(generator, "name", "Beat-keys generator", diagnostics) && valid;
        valid = validate_overlay_string_field(generator, "version", "Beat-keys generator", diagnostics) && valid;
        if (generator.contains("name") && generator.at("name").is_string() &&
            generator.at("name").get<std::string>() != "beat-keys")
        {
            diagnostics.emplace_back("Unsupported beat-keys overlay generator.");
            valid = false;
        }
    }
    if (!config.contains("source") || !config.at("source").is_object())
    {
        diagnostics.emplace_back("Beat-keys overlay requires a source object.");
        valid = false;
    }
    else
    {
        const Json &source = config.at("source");
        valid = validate_overlay_string_field(source, "base_animation", "Beat-keys source", diagnostics) && valid;
        valid = validate_overlay_string_field(source, "timeline", "Beat-keys source", diagnostics) && valid;
        valid = validate_overlay_string_field(source, "adapter_config", "Beat-keys source", diagnostics) && valid;
    }
    if (!config.contains("keyframes") || !config.at("keyframes").is_array())
    {
        diagnostics.emplace_back("Beat-keys overlay requires a keyframes array.");
        valid = false;
    }
    else
    {
        constexpr std::array<std::string_view, 3> OPERATIONS{"add", "multiply", "replace"};
        constexpr std::array<std::string_view, 5> SOURCES{
            "music.rms", "music.peak", "music.note_pulse", "music.effect_pulse", "music.row_pulse"};
        for (const Json &keyframe : config.at("keyframes"))
        {
            if (!keyframe.is_object() || !keyframe.contains("frame") || !keyframe.at("frame").is_number_integer() ||
                keyframe.at("frame").get<timeline::Ticks>() < 0 ||
                !validate_overlay_string_field(keyframe, "target", "Beat-keys keyframe", diagnostics) ||
                !validate_overlay_string_field(keyframe, "op", "Beat-keys keyframe", diagnostics) ||
                !keyframe.contains("value") || !keyframe.at("value").is_number() ||
                !validate_overlay_string_field(keyframe, "source", "Beat-keys keyframe", diagnostics))
            {
                diagnostics.emplace_back("Beat-keys overlay contains an invalid keyframe.");
                valid = false;
                continue;
            }
            const auto operation = keyframe.at("op").get<std::string>();
            const auto source = keyframe.at("source").get<std::string>();
            const auto value = keyframe.at("value").get<double>();
            if (std::find(OPERATIONS.begin(), OPERATIONS.end(), operation) == OPERATIONS.end() ||
                std::find(SOURCES.begin(), SOURCES.end(), source) == SOURCES.end() || !std::isfinite(value))
            {
                diagnostics.emplace_back("Beat-keys overlay contains an invalid keyframe.");
                valid = false;
            }
        }
    }
    if (!config.contains("diagnostics") || !config.at("diagnostics").is_object() ||
        !config.at("diagnostics").contains("warnings") || !config.at("diagnostics").at("warnings").is_array())
    {
        diagnostics.emplace_back("Beat-keys overlay requires diagnostics warnings.");
        valid = false;
    }
    return valid;
}

std::vector<timeline::NamedCount> named_counts(const std::map<std::string, int> &counts)
{
    std::vector<timeline::NamedCount> result{};
    result.reserve(counts.size());
    for (const auto &[name, count] : counts)
    {
        result.emplace_back(name, count);
    }
    return result;
}

void append_diagnostic_array(
    const Json &diagnostics, std::string_view field, std::string_view prefix, std::vector<std::string> &result)
{
    const std::string field_name(field);
    if (!diagnostics.contains(field_name))
    {
        return;
    }
    if (!diagnostics.at(field_name).is_array())
    {
        result.emplace_back("ParBeatdown diagnostics." + field_name + " must be an array.");
        return;
    }
    int index = 0;
    for (const Json &message : diagnostics.at(field_name))
    {
        if (!message.is_string())
        {
            result.emplace_back(
                "ParBeatdown diagnostics." + field_name + "[" + std::to_string(index) + "] must be a string.");
        }
        else
        {
            result.emplace_back(std::string(prefix) + message.get<std::string>());
        }
        ++index;
    }
}

void import_beat_keys_overlay(const std::filesystem::path &source_path, const Json &config,
    const JsonImportOptions &options, JsonImportResult &result)
{
    if (!validate_beat_keys_overlay(config, result.diagnostics))
    {
        return;
    }

    try
    {
        std::optional<timeline::Ticks> first_frame{};
        std::optional<timeline::Ticks> last_frame{};
        std::map<std::string, int> target_counts{};
        std::map<std::string, int> source_counts{};
        std::map<std::string, std::vector<timeline::Keyframe>> keyframes_by_target{};
        for (const Json &keyframe : config.at("keyframes"))
        {
            const auto frame = keyframe.at("frame").get<timeline::Ticks>();
            first_frame = first_frame ? std::min(*first_frame, frame) : frame;
            last_frame = last_frame ? std::max(*last_frame, frame) : frame;
            const auto target = keyframe.at("target").get<std::string>();
            const auto source_name = keyframe.at("source").get<std::string>();
            ++target_counts[target];
            ++source_counts[source_name];
        }

        std::optional<timeline::FrameGrid> frame_grid{};
        if (last_frame)
        {
            if (*last_frame == std::numeric_limits<timeline::Ticks>::max())
            {
                throw std::overflow_error("beat-keys frame extent is too large");
            }
            frame_grid.emplace(timeline::Timebase(options.ticks_per_second), *last_frame + 1,
                options.frames_per_second_numerator, options.frames_per_second_denominator);
            int keyframe_index = 0;
            for (const Json &keyframe : config.at("keyframes"))
            {
                const auto target = keyframe.at("target").get<std::string>();
                timeline::Attributes attributes{{"operation", keyframe.at("op").get<std::string>()},
                    {"source", keyframe.at("source").get<std::string>()}};
                keyframes_by_target[target].emplace_back("keyframe-" + std::to_string(keyframe_index),
                    frame_grid->frame_start(keyframe.at("frame").get<timeline::Ticks>()),
                    keyframe.at("value").get<double>(), timeline::KeyframeInterpolation::HOLD, std::move(attributes));
                ++keyframe_index;
            }
        }

        const Json &generator = config.at("generator");
        const Json &source = config.at("source");
        timeline::GenerationSummary generation_summary(generator.at("name").get<std::string>(),
            generator.at("version").get<std::string>(),
            {timeline::SourceReference("base_animation", source.at("base_animation").get<std::string>()),
                timeline::SourceReference("timeline", source.at("timeline").get<std::string>()),
                timeline::SourceReference("adapter_config", source.at("adapter_config").get<std::string>())},
            named_counts(target_counts), named_counts(source_counts));
        const std::optional<timeline::Time> first_time =
            first_frame ? std::optional{frame_grid->frame_start(*first_frame)} : std::nullopt;
        const std::optional<timeline::Time> last_time =
            last_frame ? std::optional{frame_grid->frame_start(*last_frame)} : std::nullopt;
        timeline::SourceSummary source_summary(config.at("schema").get<std::string>(), config.at("version").get<int>(),
            0, 0, first_frame, last_frame, first_time, last_time, std::nullopt, std::move(generation_summary));
        timeline::Metadata metadata(source_path.filename().string(), source_path.string());
        append_diagnostic_array(config.at("diagnostics"), "warnings", "Warning: ", result.diagnostics);
        if (frame_grid)
        {
            result.document.emplace(std::move(*frame_grid), std::move(source_summary),
                timeline::size_cast(target_counts), timeline::size_cast(config.at("keyframes")), std::move(metadata));
            for (auto &[target, keyframes] : keyframes_by_target)
            {
                timeline::Lane lane(target, target, "keyframes", result.document->frame_grid()->offset(),
                    result.document->frame_grid()->end_time());
                for (timeline::Keyframe &keyframe : keyframes)
                {
                    lane.add(std::move(keyframe));
                }
                result.document->add_lane(std::move(lane));
            }
        }
        else
        {
            result.document.emplace(timeline::Timebase(options.ticks_per_second), std::move(source_summary),
                timeline::size_cast(target_counts), timeline::size_cast(config.at("keyframes")), std::move(metadata));
        }
    }
    catch (const std::exception &error)
    {
        result.document.reset();
        result.diagnostics.emplace_back("Unable to import beat-keys overlay: " + std::string(error.what()));
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
    return true;
}

std::pair<timeline::Ticks, timeline::Ticks> rational_frame_rate(double frames_per_second)
{
    if (!std::isfinite(frames_per_second) || frames_per_second <= 0.0)
    {
        throw std::invalid_argument("frame rate must be finite and positive");
    }

    const timeline::Ticks integer_rate = static_cast<timeline::Ticks>(std::llround(frames_per_second));
    if (std::abs(frames_per_second - static_cast<double>(integer_rate)) < 0.000000001)
    {
        return {integer_rate, 1};
    }

    constexpr std::array<std::pair<timeline::Ticks, timeline::Ticks>, 4> COMMON_RATES{
        std::pair<timeline::Ticks, timeline::Ticks>{24000, 1001},
        std::pair<timeline::Ticks, timeline::Ticks>{30000, 1001},
        std::pair<timeline::Ticks, timeline::Ticks>{60000, 1001},
        std::pair<timeline::Ticks, timeline::Ticks>{120000, 1001},
    };
    for (const std::pair<timeline::Ticks, timeline::Ticks> &rate : COMMON_RATES)
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
    const timeline::Ticks divisor = std::gcd(numerator, SCALE);
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
    const Json &config, const JsonImportOptions &options, std::vector<std::string> &diagnostics)
{
    if (!options.beat_keys_config_path.empty())
    {
        Json beat_keys{};
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

/// Borrows a validated source record while retaining its original array index.
///
struct TrackerRecord
{
    int source_index;
    const Json &fields;
};

std::vector<TrackerRecord> tracker_records(const Json &items, std::string_view field,
    const timeline::Timebase &timebase, std::vector<std::string> &diagnostics)
{
    std::vector<TrackerRecord> records{};
    int index = 0;
    for (const Json &item : items)
    {
        try
        {
            if (!item.is_object())
            {
                throw std::invalid_argument("record must be an object");
            }
            if (item.contains("frame"))
            {
                const Json &frame = item.at("frame");
                if (!frame.is_number_integer() ||
                    (frame.is_number_unsigned() &&
                        frame.get<Json::number_unsigned_t>() >= std::numeric_limits<timeline::Ticks>::max()) ||
                    frame.get<timeline::Ticks>() == std::numeric_limits<timeline::Ticks>::max())
                {
                    throw std::invalid_argument("frame must be a representable integer");
                }
            }
            if (item.contains("time_seconds"))
            {
                if (!item.at("time_seconds").is_number())
                {
                    throw std::invalid_argument("time_seconds must be numeric");
                }
                const auto seconds = item.at("time_seconds").get<double>();
                if (!std::isfinite(seconds) || seconds < 0.0)
                {
                    throw std::invalid_argument("time_seconds must be finite and non-negative");
                }
                const timeline::Time time = timebase.time_from_seconds(seconds, timeline::TimeRounding::NEAREST);
                // Lane bounds need an exclusive endpoint after the final item.
                static_cast<void>(time + timeline::Duration::from_ticks(1));
            }
            else if (!item.contains("frame") || item.at("frame").get<timeline::Ticks>() < 0)
            {
                throw std::invalid_argument("record requires time_seconds or a non-negative frame");
            }
            if (field == "events")
            {
                if (!item.contains("kind") || !item.at("kind").is_string() ||
                    item.at("kind").get<std::string>().empty())
                {
                    throw std::invalid_argument("event requires a non-empty kind");
                }
                for (const char *name : {"strength", "confidence"})
                {
                    if (item.contains(name) &&
                        (!item.at(name).is_number() || !std::isfinite(item.at(name).get<double>())))
                    {
                        throw std::invalid_argument(std::string(name) + " must be finite and numeric");
                    }
                }
            }
            else if (item.contains("rms") &&
                (!item.at("rms").is_number() || !std::isfinite(item.at("rms").get<double>()) ||
                    item.at("rms").get<double>() < 0.0))
            {
                throw std::invalid_argument("rms must be finite, numeric and non-negative");
            }
            records.push_back(TrackerRecord{index, item});
        }
        catch (const std::exception &error)
        {
            diagnostics.emplace_back(
                "Skipped ParBeatdown " + std::string(field) + "[" + std::to_string(index) + "]: " + error.what());
        }
        ++index;
    }
    return records;
}

void expand_extent(const std::vector<TrackerRecord> &items, TrackerExtent &extent)
{
    for (const TrackerRecord &record : items)
    {
        const Json &item = record.fields;
        if (item.contains("frame"))
        {
            const auto frame = item.at("frame").get<timeline::Ticks>();
            extent.first_frame = extent.first_frame ? std::min(*extent.first_frame, frame) : frame;
            extent.last_frame = extent.last_frame ? std::max(*extent.last_frame, frame) : frame;
        }
        if (item.contains("time_seconds"))
        {
            const auto seconds = item.at("time_seconds").get<double>();
            extent.first_seconds = extent.first_seconds ? std::min(*extent.first_seconds, seconds) : seconds;
            extent.last_seconds = extent.last_seconds ? std::max(*extent.last_seconds, seconds) : seconds;
        }
    }
}

TrackerExtent tracker_extent(
    const Json &config, const std::vector<TrackerRecord> &events, const std::vector<TrackerRecord> &features)
{
    TrackerExtent extent{};
    expand_extent(events, extent);
    expand_extent(features, extent);
    const std::optional<timeline::Ticks> item_first_frame = extent.first_frame;
    const std::optional<timeline::Ticks> item_last_frame = extent.last_frame;
    const std::optional<double> item_first_seconds = extent.first_seconds;
    const std::optional<double> item_last_seconds = extent.last_seconds;

    if (config.contains("timeline"))
    {
        const Json &timeline = config.at("timeline");
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
            extent.first_frame = item_first_frame;
            extent.last_frame = item_last_frame;
            if (extent.last_frame)
            {
                extent.frame_count = std::max(timeline::Ticks{0}, *extent.last_frame + 1);
            }
        }
        if (duration_seconds > 0.0 || !item_last_seconds)
        {
            extent.first_seconds = 0.0;
            extent.last_seconds = duration_seconds;
        }
        else
        {
            extent.first_seconds = item_first_seconds;
            extent.last_seconds = item_last_seconds;
        }
    }
    else if (extent.last_frame)
    {
        extent.frame_count = std::max(timeline::Ticks{0}, *extent.last_frame + 1);
    }
    return extent;
}

std::optional<timeline::Time> tracker_item_time(
    const Json &item, const timeline::Timebase &timebase, const std::optional<timeline::FrameGrid> &frame_grid)
{
    if (item.contains("time_seconds"))
    {
        const auto seconds = item.at("time_seconds").get<double>();
        if (!std::isfinite(seconds))
        {
            throw std::invalid_argument("ParBeatdown event time_seconds must be finite.");
        }
        return timebase.time_from_seconds(seconds, timeline::TimeRounding::NEAREST);
    }
    if (item.contains("frame") && frame_grid)
    {
        return frame_grid->frame_start(item.at("frame").get<timeline::Ticks>());
    }
    return std::nullopt;
}

timeline::Attributes tracker_event_attributes(const Json &event)
{
    timeline::Attributes result{};
    for (Json::const_iterator field = event.begin(); field != event.end(); ++field)
    {
        if (field.key() == "kind" || field.key() == "time_seconds" || field.key() == "frame" ||
            field.key() == "strength" || field.key() == "confidence")
        {
            continue;
        }
        result.emplace(field.key(), field->is_string() ? field->get<std::string>() : field->dump());
    }
    return result;
}

std::optional<timeline::Lane> tracker_event_lane(const std::vector<TrackerRecord> &records,
    const timeline::Timebase &timebase, const std::optional<timeline::FrameGrid> &frame_grid,
    std::vector<std::string> &diagnostics)
{
    std::vector<timeline::Instant> events{};
    std::optional<timeline::Time> first_time{};
    std::optional<timeline::Time> last_time{};
    for (const TrackerRecord &record : records)
    {
        try
        {
            const Json &event = record.fields;
            const std::optional<timeline::Time> time = tracker_item_time(event, timebase, frame_grid);
            if (!time)
            {
                throw std::invalid_argument("frame-only event requires frame timing metadata");
            }
            static_cast<void>(*time + timeline::Duration::from_ticks(1));

            if (frame_grid && (*time < frame_grid->offset() || frame_grid->end_time() <= *time))
            {
                throw std::out_of_range("event time is outside the document frame grid");
            }
            const auto kind = event.at("kind").get<std::string>();
            std::optional<double> strength{};
            if (event.contains("strength"))
            {
                strength = event.at("strength").get<double>();
            }
            else if (event.contains("confidence"))
            {
                strength = event.at("confidence").get<double>();
            }
            events.emplace_back("event-" + std::to_string(record.source_index), kind, *time, kind, strength,
                tracker_event_attributes(event));
            first_time = first_time ? std::min(*first_time, *time) : *time;
            last_time = last_time ? std::max(*last_time, *time) : *time;
        }
        catch (const std::exception &error)
        {
            diagnostics.emplace_back(
                "Skipped ParBeatdown events[" + std::to_string(record.source_index) + "]: " + error.what());
        }
    }
    if (events.empty())
    {
        return std::nullopt;
    }

    timeline::Time lane_start = *first_time;
    timeline::Time lane_end = *last_time + timeline::Duration::from_ticks(1);
    if (frame_grid && frame_grid->duration().ticks() > 0)
    {
        lane_start = frame_grid->offset();
        lane_end = frame_grid->end_time();
    }

    timeline::Lane lane("tracker-events", "Music events", "events", lane_start, lane_end);
    for (timeline::Instant &event : events)
    {
        lane.add(std::move(event));
    }
    return lane;
}

std::optional<timeline::Lane> tracker_feature_lane(const std::vector<TrackerRecord> &records,
    const timeline::Timebase &timebase, const std::optional<timeline::FrameGrid> &frame_grid, const char *field,
    const char *label, std::vector<std::string> &diagnostics)
{
    std::vector<timeline::CurveSample> samples{};
    double maximum = 1.0;
    for (const TrackerRecord &record : records)
    {
        const Json &feature = record.fields;
        if (!feature.contains(field))
        {
            continue;
        }
        try
        {
            const std::optional<timeline::Time> time = tracker_item_time(feature, timebase, frame_grid);
            if (!time)
            {
                throw std::invalid_argument("frame-only feature requires frame timing metadata");
            }
            if (!samples.empty() && *time <= samples.back().time())
            {
                throw std::invalid_argument(std::string(label) + " sample times must be strictly increasing");
            }
            static_cast<void>(*time + timeline::Duration::from_ticks(1));
            if (frame_grid && (*time < frame_grid->offset() || frame_grid->end_time() <= *time))
            {
                throw std::out_of_range(std::string(label) + " sample time is outside the document frame grid");
            }
            const double value = feature.at(field).get<double>();
            samples.emplace_back(*time, value);
            maximum = std::max(maximum, value);
        }
        catch (const std::exception &error)
        {
            diagnostics.emplace_back(
                "Skipped ParBeatdown features[" + std::to_string(record.source_index) + "]: " + error.what());
        }
    }
    if (timeline::size_cast(samples) < 2)
    {
        if (!samples.empty())
        {
            diagnostics.emplace_back(
                std::string("ParBeatdown ") + label + " curve requires at least two valid samples.");
        }
        return std::nullopt;
    }

    timeline::Time lane_start = samples.front().time();
    timeline::Time lane_end = samples.back().time() + timeline::Duration::from_ticks(1);
    if (frame_grid && frame_grid->duration().ticks() > 0)
    {
        lane_start = frame_grid->offset();
        lane_end = frame_grid->end_time();
    }

    const std::string id = std::string("tracker-") + field;
    timeline::Lane lane(id, label, "curve", lane_start, lane_end);
    lane.add(
        timeline::Curve(id, field, std::move(samples), label, timeline::CurveInterpolation::LINEAR, 0.0, maximum, {}));
    return lane;
}

void append_tracker_diagnostics(const Json &config, std::vector<std::string> &diagnostics)
{
    if (!config.contains("diagnostics"))
    {
        return;
    }
    const Json &source = config.at("diagnostics");
    if (!source.is_object())
    {
        diagnostics.emplace_back("ParBeatdown diagnostics must be an object.");
        return;
    }
    append_diagnostic_array(source, "warnings", "Warning: ", diagnostics);
    append_diagnostic_array(source, "unsupported", "Unsupported: ", diagnostics);
    append_diagnostic_array(source, "log", "Log: ", diagnostics);
}

std::string tracker_source_string(const Json &source, std::string_view field, std::vector<std::string> &diagnostics)
{
    const std::string name(field);
    if (!source.contains(name))
    {
        return {};
    }
    if (!source.at(name).is_string())
    {
        diagnostics.emplace_back("ParBeatdown source." + name + " must be a string.");
        return {};
    }
    return source.at(name).get<std::string>();
}

timeline::Metadata tracker_metadata(
    const std::filesystem::path &path, const Json &config, std::vector<std::string> &diagnostics)
{
    std::string title = path.filename().string();
    std::string description = path.string();
    if (config.contains("source"))
    {
        const Json &source = config.at("source");
        if (!source.is_object())
        {
            diagnostics.emplace_back("ParBeatdown source must be an object.");
        }
        else
        {
            std::string source_title = tracker_source_string(source, "title", diagnostics);
            std::string file = tracker_source_string(source, "file", diagnostics);
            const std::string format = tracker_source_string(source, "format", diagnostics);
            if (!source_title.empty())
            {
                title = std::move(source_title);
            }
            if (!file.empty())
            {
                description = std::move(file);
                if (!format.empty())
                {
                    description += " (" + format + ")";
                }
            }
        }
    }
    return timeline::Metadata(std::move(title), std::move(description));
}

std::optional<timeline::GenerationSummary> tracker_generation_summary(
    const Json &config, std::vector<std::string> &diagnostics)
{
    if (!config.contains("generator"))
    {
        return std::nullopt;
    }
    try
    {
        const Json &generator = config.at("generator");
        std::vector<timeline::SourceReference> sources{};
        if (config.contains("source") && config.at("source").is_object())
        {
            const Json &source = config.at("source");
            if (source.contains("file") && source.at("file").is_string() &&
                !source.at("file").get<std::string>().empty())
            {
                sources.emplace_back("music", source.at("file").get<std::string>());
            }
        }
        return timeline::GenerationSummary(generator.at("name").get<std::string>(),
            generator.at("version").get<std::string>(), std::move(sources), {}, {});
    }
    catch (const std::exception &error)
    {
        diagnostics.emplace_back("Unable to import ParBeatdown generator metadata: " + std::string(error.what()));
        return std::nullopt;
    }
}

void import_tracker_timeline(const std::filesystem::path &source_path, const Json &config,
    const JsonImportOptions &options, JsonImportResult &result)
{
    if (!validate_tracker_timeline(config, result.diagnostics))
    {
        return;
    }

    try
    {
        const timeline::Timebase timebase(options.ticks_per_second);
        const std::optional<TrackerTiming> timing = tracker_timing(config, options, result.diagnostics);
        const std::vector<TrackerRecord> events =
            tracker_records(config.at("events"), "events", timebase, result.diagnostics);
        const std::vector<TrackerRecord> features =
            tracker_records(config.at("features"), "features", timebase, result.diagnostics);
        const TrackerExtent extent = tracker_extent(config, events, features);
        std::optional<timeline::Time> first_time = extent.first_seconds
            ? std::optional{timebase.time_from_seconds(*extent.first_seconds, timeline::TimeRounding::NEAREST)}
            : std::nullopt;
        std::optional<timeline::Time> last_time = extent.last_seconds
            ? std::optional{timebase.time_from_seconds(*extent.last_seconds, timeline::TimeRounding::NEAREST)}
            : std::nullopt;
        std::optional<timeline::Duration> frame_offset{};
        std::optional<timeline::FrameGrid> frame_grid{};
        if (timing)
        {
            frame_offset = timebase.duration_from_seconds(timing->offset_seconds, timeline::TimeRounding::NEAREST);
            const auto grid_origin = timeline::Time::from_ticks(-frame_offset->ticks());
            frame_grid.emplace(timebase, extent.frame_count, timing->frames_per_second_numerator,
                timing->frames_per_second_denominator, grid_origin);
            if (!first_time && extent.first_frame)
            {
                first_time = frame_grid->frame_start(*extent.first_frame);
                last_time = frame_grid->frame_start(*extent.last_frame);
            }
        }

        append_tracker_diagnostics(config, result.diagnostics);
        std::optional<timeline::Lane> event_lane = tracker_event_lane(events, timebase, frame_grid, result.diagnostics);
        std::optional<timeline::Lane> rms_lane =
            tracker_feature_lane(features, timebase, frame_grid, "rms", "RMS", result.diagnostics);
        timeline::Metadata metadata = tracker_metadata(source_path, config, result.diagnostics);
        std::optional<timeline::GenerationSummary> generation = tracker_generation_summary(config, result.diagnostics);
        timeline::SourceSummary source_summary(config.at("schema").get<std::string>(), config.at("version").get<int>(),
            timeline::size_cast(config.at("features")), timeline::size_cast(config.at("events")), extent.first_frame,
            extent.last_frame, first_time, last_time, frame_offset, std::move(generation));
        if (frame_grid)
        {
            result.document.emplace(std::move(*frame_grid), std::move(source_summary), std::move(metadata));
        }
        else
        {
            result.document.emplace(timebase, std::move(source_summary), std::move(metadata));
        }
        if (event_lane)
        {
            result.document->add_lane(std::move(*event_lane));
        }
        if (rms_lane)
        {
            result.document->add_lane(std::move(*rms_lane));
        }
    }
    catch (const std::exception &error)
    {
        result.document.reset();
        result.diagnostics.emplace_back("Unable to import ParBeatdown timeline: " + std::string(error.what()));
    }
}

timeline::Ticks source_frame(const Json &record)
{
    const Json &frame = record.at("frame");
    if (!frame.is_number_integer() ||
        (frame.is_number_unsigned() &&
            frame.get<Json::number_unsigned_t>() >= std::numeric_limits<timeline::Ticks>::max()) ||
        frame.get<timeline::Ticks>() < 0 || frame.get<timeline::Ticks>() == std::numeric_limits<timeline::Ticks>::max())
    {
        throw std::invalid_argument("source frame must be a nonnegative representable integer");
    }
    return frame.get<timeline::Ticks>();
}

MappingRecipe mapping_recipe(const Json &binding)
{
    MappingRecipe recipe;
    recipe.source = binding.at("source").get<std::string>();
    recipe.target = binding.at("target").get<std::string>();
    recipe.operation = binding.at("op").get<std::string>();
    recipe.scale = binding.value("scale", 1.0);
    recipe.offset = binding.value("offset", 0.0);
    recipe.decay_seconds = binding.value("decay_seconds", 0.0);
    if (binding.contains("clamp"))
    {
        recipe.clamp = std::pair<double, double>{
            binding.at("clamp").at("min").get<double>(), binding.at("clamp").at("max").get<double>()};
    }
    return recipe;
}

std::vector<MappingInput> mapping_inputs(const Json &source, const std::vector<MappingRecipe> &recipes)
{
    std::vector<MappingInput> inputs;
    for (const char *field : {"features", "events"})
    {
        for (const Json &record : source.at(field))
        {
            if (std::string_view(field) == "features")
            {
                for (const char *feature : {"rms", "peak"})
                {
                    const std::string name = std::string("music.") + feature;
                    const bool used = std::any_of(recipes.begin(), recipes.end(),
                        [&name](const MappingRecipe &recipe) { return recipe.source == name; });
                    if (used)
                    {
                        inputs.push_back(MappingInput{name, source_frame(record), record.at(feature).get<double>()});
                    }
                }
            }
            else
            {
                const std::string kind = record.at("kind").get<std::string>();
                const std::string name = "music." + kind + "_pulse";
                const bool used = std::any_of(recipes.begin(), recipes.end(),
                    [&name](const MappingRecipe &recipe) { return recipe.source == name; });
                if (used)
                {
                    inputs.push_back(MappingInput{name, source_frame(record), 1.0});
                }
            }
        }
    }
    return inputs;
}

void import_beat_keys_mapping(const std::filesystem::path &config_path, const Json &config,
    const JsonImportOptions &options, JsonImportResult &result)
{
    try
    {
        if (!config.contains("version") || !config.at("version").is_number_integer() ||
            config.at("version").get<timeline::Ticks>() != 1)
        {
            throw std::invalid_argument("unsupported beat-keys config version");
        }
        if (!config.at("bindings").is_array())
        {
            throw std::invalid_argument("beat-keys bindings must be an array");
        }
        std::vector<MappingRecipe> recipes;
        for (const Json &binding : config.at("bindings"))
        {
            recipes.push_back(mapping_recipe(binding));
        }
        const std::string location = config.at("source").at("timeline").get<std::string>();
        if (location.empty())
        {
            throw std::invalid_argument("source timeline reference must not be empty");
        }
        const std::filesystem::path source_path = (config_path.parent_path() / location).lexically_normal();
        Json source;
        if (!read_json_file(source_path, "beat-keys source timeline", source, result.diagnostics))
        {
            return;
        }
        // Feature-only beat-keys inputs may omit their unused event array.
        if (source.is_object() && !source.contains("events"))
        {
            source["events"] = Json::array();
        }
        JsonImportOptions source_options = options;
        source_options.beat_keys_config_path = config_path;
        JsonImportResult music;
        import_tracker_timeline(source_path, source, source_options, music);
        result.diagnostics.insert(result.diagnostics.end(), music.diagnostics.begin(), music.diagnostics.end());
        if (!music.document)
        {
            return;
        }
        const bool uses_peak = std::any_of(
            recipes.begin(), recipes.end(), [](const MappingRecipe &recipe) { return recipe.source == "music.peak"; });
        if (uses_peak)
        {
            const std::vector<TrackerRecord> records =
                tracker_records(source.at("features"), "features", music.document->timebase(), result.diagnostics);
            std::optional<timeline::Lane> peak = tracker_feature_lane(
                records, music.document->timebase(), music.document->frame_grid(), "peak", "Peak", result.diagnostics);
            if (peak)
            {
                music.document->add_lane(std::move(*peak));
            }
        }
        std::vector<MappingInput> inputs = mapping_inputs(source, recipes);
        const MappingOutput output{
            config.at("output").at("mode").get<std::string>(), config.at("output").at("namespace").get<std::string>()};
        result.mapping.emplace(std::move(*music.document), std::move(recipes), std::move(inputs), output, config_path);
        result.document = result.mapping->materialize();
    }
    catch (const std::exception &error)
    {
        result.document.reset();
        result.mapping.reset();
        result.diagnostics.emplace_back("Unable to import beat-keys mapping: " + std::string(error.what()));
    }
}

} // namespace

JsonImportResult import_timeline_json(const std::filesystem::path &source_path, const JsonImportOptions &options)
{
    JsonImportResult result{};
    if (source_path.empty())
    {
        result.diagnostics.emplace_back("No JSON file was selected.");
        return result;
    }

    Json config{};
    if (!read_json_file(source_path, "timeline JSON", config, result.diagnostics))
    {
        return result;
    }

    if (config.is_object() && config.contains("schema"))
    {
        if (!config.at("schema").is_string())
        {
            result.diagnostics.emplace_back("Unsupported timeline JSON schema.");
            return result;
        }
        const auto schema = config.at("schema").get<std::string>();
        if (schema == TRACKER_TIMELINE_SCHEMA)
        {
            import_tracker_timeline(source_path, config, options, result);
        }
        else if (schema == BEAT_KEYS_OVERLAY_SCHEMA)
        {
            import_beat_keys_overlay(source_path, config, options, result);
        }
        else if (schema == BEAT_KEYS_SCHEMA)
        {
            import_beat_keys_mapping(source_path, config, options, result);
        }
        else
        {
            result.diagnostics.emplace_back("Unsupported timeline JSON schema.");
        }
    }
    else
    {
        import_par_animator(source_path, config, options, result);
    }
    return result;
}

} // namespace timeline_par_animator
