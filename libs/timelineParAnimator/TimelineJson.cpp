// Copyright (c) 2026 Richard Thomson

#include <timelineParAnimator/TimelineJson.h>

#include <timeline/size_cast.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string_view>
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
    Json result{{"parameters", Json::object()}, {"fractal-types", Json::object()}};
    for (const Json &location : config.at("parameter-catalogs"))
    {
        Json catalog;
        if (!read_json_file(
                path.parent_path() / location.get<std::string>(), "parameter catalog", catalog, diagnostics))
        {
            throw std::invalid_argument("unable to resolve parameter catalog");
        }
        if (!catalog.at("parameters").is_object())
        {
            throw std::invalid_argument("parameter catalog requires a parameters object");
        }
        result.at("parameters").update(catalog.at("parameters"));
        if (catalog.contains("fractal-types"))
        {
            result.at("fractal-types").update(catalog.at("fractal-types"));
        }
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
        throw std::invalid_argument("invalid PWM function slot");
    }
    const std::string_view text = parameter.substr(PREFIX.size(), parameter.size() - PREFIX.size() - 1);
    int slot = 0;
    const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), slot);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || slot < 0 ||
        slot == std::numeric_limits<int>::max())
    {
        throw std::invalid_argument("invalid PWM function slot");
    }
    return slot;
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
    constexpr std::array<std::string_view, 31> ID_FUNCTIONS{"sin", "cos", "tan", "cotan", "sinh", "cosh", "tanh",
        "cotanh", "exp", "log", "sqr", "recip", "ident", "cosxx", "flip", "conj", "zero", "one", "asin", "asinh",
        "acos", "acosh", "atan", "atanh", "sqrt", "abs", "cabs", "floor", "ceil", "trunc", "round"};
    metadata["values"] = ID_FUNCTIONS;
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

void animation_planar_lanes(const Json &path, const Json &metadata, const std::string &id, const std::string &label,
    const std::string &parameter, const std::string &layer, const timeline::FrameGrid &grid,
    std::vector<timeline::Lane> &lanes)
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
        const timeline::Attributes attributes{{"parameter", parameter}, {"layer", layer}, {"track", id},
            {"path", path.dump()}, {"component", std::to_string(component)}};
        const auto evaluate = [origin, radius, radius_change, component_phase, frequency, component, start, end](
                                  timeline::Time time)
        {
            constexpr double PI = 3.141592653589793238462643383279502884;
            const double fraction =
                static_cast<double>((time - start).ticks()) / static_cast<double>((end - start).ticks());
            const double radians = (component_phase + 360.0 * frequency * fraction) * PI / 180.0;
            const double sampled_radius = radius + fraction * radius_change;
            return clean_path_value(origin + sampled_radius * (component == 0 ? std::cos(radians) : std::sin(radians)));
        };
        timeline::Lane lane(id + suffix, label + suffix, "curve", start, grid.end_time());
        lane.add(timeline::Curve(id + suffix + "-path", "procedural-path", start, end, evaluate, label + suffix,
            clean_path_value(origin - maximum_radius), clean_path_value(origin + maximum_radius), attributes));
        lanes.push_back(std::move(lane));
    }
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

void animation_control_point_lanes(const Json &path, const Json &metadata, const std::string &id,
    const std::string &label, const std::string &parameter, const std::string &layer, const timeline::FrameGrid &grid,
    std::vector<timeline::Lane> &lanes)
{
    if (grid.frame_count() < 2)
    {
        throw std::invalid_argument("path tracks require at least two frames");
    }
    const bool catmull_rom = path.at("kind").get<std::string>() == "catmull-rom";
    const std::string path_name = catmull_rom ? "Catmull-Rom" : "Bezier";
    const std::string type = metadata.value("type", std::string{});
    if ((type == "vector2" || type == "vector3") && metadata.value("normalize", false))
    {
        throw std::invalid_argument(path_name + " vector normalization is not supported");
    }
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
    for (int component = 0; component < arity; ++component)
    {
        std::vector<double> values;
        for (const std::vector<double> &point : control_points)
        {
            values.push_back(point[component]);
        }
        const auto [minimum, maximum] = std::minmax_element(values.begin(), values.end());
        const std::pair<double, double> bounds =
            catmull_rom ? catmull_rom_bounds(values) : std::pair<double, double>{*minimum, *maximum};
        const std::string suffix = arity == 1 ? "" : "[" + std::to_string(component) + "]";
        const timeline::Attributes attributes{{"parameter", parameter}, {"layer", layer}, {"track", id},
            {"path", path.dump()}, {"component", std::to_string(component)}};
        const auto evaluate = [values, start, end, catmull_rom](timeline::Time time)
        {
            const double fraction =
                static_cast<double>((time - start).ticks()) / static_cast<double>((end - start).ticks());
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
        };
        timeline::Lane lane(id + suffix, label + suffix, "curve", start, grid.end_time());
        lane.add(timeline::Curve(id + suffix + "-path", "procedural-path", start, end, evaluate, label + suffix,
            clean_path_value(bounds.first), clean_path_value(bounds.second), attributes));
        lanes.push_back(std::move(lane));
    }
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

void animation_key_lanes(const Json &track, const Json &metadata, const std::string &id, const std::string &label,
    const std::string &parameter, const std::string &layer, const timeline::FrameGrid &grid,
    std::vector<timeline::Lane> &lanes)
{
    const Json keys = track.contains("path") ? animation_path_keys(track.at("path"), grid) : track.at("keys");
    if (!keys.is_array() || keys.empty())
    {
        throw std::invalid_argument("track keys must be a nonempty array");
    }
    std::vector<std::vector<double>> values;
    bool categorical = false;
    timeline::Ticks previous = -1;
    for (const Json &key : keys)
    {
        const timeline::Ticks frame = source_frame(key);
        if (frame <= previous || frame >= grid.frame_count())
        {
            throw std::invalid_argument("key frames must be strictly increasing and within num-frames");
        }
        previous = frame;
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
            const std::string value =
                key.at("value").is_string() ? key.at("value").get<std::string>() : key.at("value").dump();
            const timeline::Time start = grid.frame_start(source_frame(key));
            const timeline::Time end = index + 1 < timeline::size_cast(keys)
                ? grid.frame_start(source_frame(keys[index + 1]))
                : grid.end_time();
            timeline::Attributes attributes{{"parameter", parameter}, {"layer", layer}, {"value", value},
                {"curve", curve}, {"outgoing-curve", outgoing}, {"track", id}};
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
    for (const std::vector<double> &value : values)
    {
        if (value.empty() || timeline::size_cast(value) != timeline::size_cast(values.front()))
        {
            throw std::invalid_argument("keyframe component counts must agree");
        }
    }
    const int components = timeline::size_cast(values.front());
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
            if (metadata.value("type", std::string{}) == "center-mag" && outgoing_curve == "geometric" &&
                component != 2 && component != 3)
            {
                outgoing_curve = "linear";
            }
            timeline::Attributes attributes{{"parameter", parameter}, {"layer", layer},
                {"value", key.at("value").dump()}, {"curve", authored_curve}, {"outgoing-curve", outgoing_curve},
                {"track", id}};
            if (track.contains("path"))
            {
                attributes["path"] = track.at("path").dump();
            }
            if (key.at("value").is_string())
            {
                attributes["value"] = key.at("value").get<std::string>();
            }
            lane.add(timeline::Keyframe(id + "-key-" + std::to_string(index), grid.frame_start(source_frame(key)),
                values[index][component], animation_interpolation(outgoing_curve), std::move(attributes)));
        }
        lanes.push_back(std::move(lane));
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
        animation_control_point_lanes(look.at("path"), Json{{"type", "point2"}}, id + "-look-at", label + " / look-at",
            "look-at", layer, grid, signals);
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
    for (int component = 0; component < 2; ++component)
    {
        if (camera2d_sample(signals[component], start) != camera2d_sample(signals[component], end))
        {
            throw std::invalid_argument("Camera2D eye path with moving look-at is not supported yet");
        }
    }
    if (control_points)
    {
        animation_control_point_lanes(
            path, Json{{"type", "point2"}}, id + "-eye", label + " / eye", "eye", layer, grid, signals);
        camera2d_validate_control_eye(path, {camera2d_sample(signals[0], start), camera2d_sample(signals[1], start)});
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

void animation_camera2d_lanes(const Json &track, const Json &catalog, const std::filesystem::path &source_path,
    const Json &config, const std::string &video, const std::string &id, const std::string &layer,
    const timeline::FrameGrid &grid, std::vector<timeline::Lane> &lanes)
{
    const std::string name = track.at("name").get<std::string>();
    if (name.empty() || grid.frame_count() < 2)
    {
        throw std::invalid_argument("Camera2D requires a name and at least two frames");
    }
    if (track.value("mode", std::string("keyframes")) != "keyframes" || track.contains("skew"))
    {
        throw std::invalid_argument("Camera2D skew and non-keyframe modes are not supported yet");
    }
    const std::string output = track.at("output").get<std::string>();
    const Json &parameters = catalog.at("parameters");
    if (output != "center-mag" || !parameters.contains(output) || parameters.at(output).at("type") != "center-mag")
    {
        throw std::invalid_argument("Camera2D requires a catalog-declared center-mag output");
    }
    if (track.at("aspect") != "source" || video != "F6")
    {
        throw std::invalid_argument("Camera2D requires source aspect and supported video mode F6");
    }
    const std::map<std::string, std::string> source = animation_source(source_path, config);
    if (source.count(output) == 0)
    {
        throw std::invalid_argument("Camera2D output is missing from the source PAR entry");
    }
    const std::vector<double> source_view = animation_value(source.at(output));
    if (timeline::size_cast(source_view) < 3 || timeline::size_cast(source_view) > 6)
    {
        throw std::invalid_argument("Camera2D source center-mag requires three through six components");
    }
    const double stretch = source_view.size() < 4 || source_view[3] == 0 ? 1 : source_view[3];
    const double aspect = (4.0 / 3.0) / std::abs(stretch);
    const std::string label = layer.empty() ? name : layer + " / " + name;
    std::vector<timeline::Lane> signals;
    camera2d_look_lanes(track.at("look-at"), id, label, layer, grid, signals);
    const bool eye = track.contains("eye");
    if (eye)
    {
        if (std::holds_alternative<timeline::Curve>(signals[0].items().front()))
        {
            throw std::invalid_argument("Camera2D curved look-at with eye is not supported yet");
        }
        camera2d_eye_lanes(track.at("eye"), id, label, layer, grid, signals);
    }
    else
    {
        camera2d_key_lanes(track.at("view-up"), "view-up", "vector2", id, label, layer, grid, signals);
    }
    camera2d_key_lanes(track.at("height"), "height", "double", id, label, layer, grid, signals);

    const timeline::Time start = grid.offset();
    const timeline::Time end = grid.frame_start(grid.frame_count() - 1);
    const auto direction = [x = signals[2], y = signals[3], look_x = signals[0], look_y = signals[1], eye](
                               timeline::Time time)
    {
        return std::array<double, 2>{
            camera2d_direction_component(camera2d_sample(x, time), camera2d_sample(look_x, time), eye),
            camera2d_direction_component(camera2d_sample(y, time), camera2d_sample(look_y, time), eye)};
    };
    static_cast<void>(camera2d_normalize(direction(end)));
    if (std::holds_alternative<timeline::Keyframe>(signals[2].items().front()))
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
    const double from_mag = magnification(start);
    const double to_mag = magnification(end);
    if (!std::isfinite(from_mag) || !std::isfinite(to_mag) || from_mag <= 0 || to_mag <= 0)
    {
        throw std::invalid_argument("Camera2D magnification must be finite and positive");
    }
    const std::array<std::string, 6> components{
        "center-x", "center-y", "magnification", "x-mag-factor", "rotation", "skew"};
    const std::array<std::pair<double, double>, 6> bounds{camera2d_bounds(signals[0], start, end),
        camera2d_bounds(signals[1], start, end), std::minmax(from_mag, to_mag), {stretch, stretch}, {-180, 180},
        {0, 0}};
    const timeline::Attributes attributes{{"camera2d", track.dump()}, {"track", id}, {"layer", layer}, {"camera", name},
        {"source-value", source.at(output)}, {"aspect", std::to_string(aspect)},
        {"source-entry", config.at("source").at("name").get<std::string>()},
        {"source-file", (source_path.parent_path() / config.at("source").at("file").get<std::string>()).string()}};
    for (int component = 0; component < 6; ++component)
    {
        const auto evaluate = [x = signals[0], y = signals[1], normalized_up, magnification, stretch, component](
                                  timeline::Time time)
        {
            const std::array<double, 2> up = normalized_up(time);
            constexpr double PI = 3.141592653589793238462643383279502884;
            const std::array<double, 6> values{camera2d_sample(x, time), camera2d_sample(y, time), magnification(time),
                stretch, std::atan2(up[0], up[1]) * 180 / PI, 0};
            return clean_path_value(values[component]);
        };
        const std::string suffix = "-center-mag[" + std::to_string(component) + "]";
        timeline::Attributes output_attributes = attributes;
        output_attributes["parameter"] = output;
        output_attributes["component"] = components[component];
        timeline::Lane lane(id + suffix, label + " / " + components[component], "curve", start, grid.end_time());
        lane.add(timeline::Curve(id + suffix + "-camera", "camera2d", start, end, evaluate, lane.label(),
            clean_path_value(bounds[component].first), clean_path_value(bounds[component].second), output_attributes));
        lanes.push_back(std::move(lane));
    }
    for (int component = 0; component < 5; ++component)
    {
        const std::string member = component < 2 ? "look-at" : component < 4 ? (eye ? "eye" : "view-up") : "height";
        timeline::Attributes input_attributes = attributes;
        input_attributes["parameter"] = name + "." + member;
        input_attributes["signal"] = track.at(member).dump();
        input_attributes["component"] = std::to_string(component == 4 ? 0 : component % 2);
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
            const std::array<double, 2> from{
                camera2d_sample(authored_up[0], start), camera2d_sample(authored_up[1], start)};
            const std::array<double, 2> to{
                camera2d_segment_end(authored_up[0], start, end), camera2d_segment_end(authored_up[1], start, end)};
            camera2d_validate_segment(from, to);
            static_cast<void>(
                camera2d_normalize({camera2d_sample(authored_up[0], end), camera2d_sample(authored_up[1], end)}));
            for (int component = 0; component < 2; ++component)
            {
                const auto evaluate = [x = authored_up[0], y = authored_up[1], component](timeline::Time time)
                {
                    return camera2d_normalize({camera2d_sample(x, time), camera2d_sample(y, time)})[component];
                };
                timeline::Attributes up_attributes = attributes;
                up_attributes["parameter"] = name + ".view-up";
                up_attributes["component"] = std::to_string(component);
                up_attributes["signal"] = track.at("view-up").dump();
                up_attributes["normalize"] = "true";
                timeline::Lane lane(
                    authored_up[component].id(), authored_up[component].label(), "curve", start, grid.end_time());
                lane.add(timeline::Curve(lane.id() + "-normalized", "camera2d-input", start, end, evaluate,
                    lane.label(), -1, 1, up_attributes));
                lanes.push_back(std::move(lane));
            }
        }
    }
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
            const Json &parameters = catalog.at("parameters");
            Json metadata = parameters.contains(parameter) ? parameters.at(parameter) : Json::object();
            timeline::Attributes source_attributes;
            if (mode == "pwm" && parameter.substr(0, 9) == "function[")
            {
                const int slot = function_slot(parameter);
                const std::map<std::string, std::string> source = animation_source(source_path, config);
                metadata = function_slot_metadata(catalog, source, slot);
                source_attributes = {{"slot", std::to_string(slot)}, {"output-parameter", "function"},
                    {"source-value", source.count("function") != 0 ? source.at("function") : ""},
                    {"source-entry", config.at("source").at("name").get<std::string>()},
                    {"source-file",
                        (source_path.parent_path() / config.at("source").at("file").get<std::string>()).string()}};
            }
            if (metadata.value("extrapolate", std::string("clamp")) != "clamp")
            {
                throw std::invalid_argument("only clamp extrapolation is supported for realized keyframes");
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
                animation_planar_lanes(track.at("path"), metadata, id, label, parameter, layer, grid, track_lanes);
            }
            else if (path_kind == "bezier" || path_kind == "catmull-rom")
            {
                animation_control_point_lanes(
                    track.at("path"), metadata, id, label, parameter, layer, grid, track_lanes);
            }
            else
            {
                animation_key_lanes(track, metadata, id, label, parameter, layer, grid, track_lanes);
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
