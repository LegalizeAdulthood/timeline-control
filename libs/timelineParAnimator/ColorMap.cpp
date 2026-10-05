// Copyright (c) 2026 Richard Thomson

#include <ColorMap.h>
#include <NamedColors.h>

#include <timeline/size_cast.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
#include <functional>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace timeline_par_animator::detail
{
namespace
{

/// JSON representation of the imported ParAnimator definition.
using Json = nlohmann::json;

std::string trimmed(std::string value)
{
    const auto not_space = [](unsigned char character)
    {
        return !std::isspace(character);
    };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string lowercase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

int rgb_component(const std::string &text)
{
    std::size_t consumed = 0;
    const int value = std::stoi(text, &consumed);
    if (consumed != text.size())
    {
        throw std::invalid_argument("RGB component must be an integer");
    }
    if (value < 0 || value > 255)
    {
        throw std::invalid_argument("RGB component out of range [0, 255]");
    }
    return value;
}

double unit_component(const std::string &text, double maximum)
{
    std::size_t consumed = 0;
    const double value = std::stod(text, &consumed);
    if (consumed != text.size() || !std::isfinite(value) || value < 0 || value > maximum)
    {
        throw std::invalid_argument("color component out of range");
    }
    return value;
}

timeline::RgbColor hsv_or_hsl_color(bool hsv, double hue, double saturation, double third)
{
    const double chroma = hsv ? saturation * third : (1 - std::abs(2 * third - 1)) * saturation;
    const double match = hsv ? third - chroma : third - chroma / 2;
    const double sector = hue == 360 ? 0 : hue / 60;
    const double second = chroma * (1 - std::abs(std::fmod(sector, 2) - 1));
    const std::array<std::array<double, 3>, 6> channels{{{chroma, second, 0}, {second, chroma, 0}, {0, chroma, second},
        {0, second, chroma}, {second, 0, chroma}, {chroma, 0, second}}};
    const std::array<double, 3> &selected = channels[static_cast<int>(sector)];
    const auto byte = [match](double channel)
    {
        return static_cast<int>(std::lround(std::clamp(channel + match, 0.0, 1.0) * 255));
    };
    return timeline::RgbColor(byte(selected[0]), byte(selected[1]), byte(selected[2]));
}

timeline::RgbColor color_spec(const std::string &text)
{
    const std::string value = trimmed(text);
    const std::string lower = lowercase(value);
    for (const NamedColor &color : NAMED_COLORS)
    {
        if (color.name == lower)
        {
            return timeline::RgbColor(color.red, color.green, color.blue);
        }
    }
    const std::size_t colon = value.find(':');
    const std::string format = colon == std::string::npos ? "rgb" : lower.substr(0, colon);
    if (format != "rgb" && format != "hsv" && format != "hsl")
    {
        throw std::invalid_argument("unsupported color format");
    }
    std::istringstream input(colon == std::string::npos ? value : value.substr(colon + 1));
    std::vector<std::string> components;
    std::string component;
    while (std::getline(input, component, '/'))
    {
        components.push_back(trimmed(component));
    }
    if (timeline::size_cast(components) != 3 || value.back() == '/')
    {
        throw std::invalid_argument("color requires three components");
    }
    if (format == "rgb")
    {
        try
        {
            return timeline::RgbColor(
                rgb_component(components[0]), rgb_component(components[1]), rgb_component(components[2]));
        }
        catch (const std::invalid_argument &error)
        {
            throw std::invalid_argument("invalid color: " + std::string(error.what()));
        }
    }
    const double hue = unit_component(components[0], 360);
    const double saturation = unit_component(components[1], 1);
    const double third = unit_component(components[2], 1);
    return hsv_or_hsl_color(format == "hsv", hue, saturation, third);
}

timeline::Palette read_palette(const std::filesystem::path &path)
{
    std::ifstream input(path);
    if (!input)
    {
        throw std::invalid_argument("cannot read color-map file: " + path.string());
    }
    timeline::Palette colors;
    std::string line;
    for (int index = 0; index < 256; ++index)
    {
        if (!std::getline(input, line))
        {
            throw std::invalid_argument("color-map file requires 256 entries");
        }
        std::istringstream entry(line);
        std::array<std::string, 3> components;
        if (!(entry >> components[0] >> components[1] >> components[2]))
        {
            throw std::invalid_argument("color-map entry requires three RGB components");
        }
        colors.emplace_back(rgb_component(components[0]), rgb_component(components[1]), rgb_component(components[2]));
    }
    while (std::getline(input, line))
    {
        if (!trimmed(line).empty())
        {
            throw std::invalid_argument("color-map file requires exactly 256 entries");
        }
    }
    return colors;
}

timeline::RgbColor interpolate_color(const timeline::RgbColor &from, const timeline::RgbColor &to, double fraction)
{
    const auto channel = [fraction](int first, int last)
    {
        const double scaled_delta = fraction * static_cast<double>(last - first);
        return static_cast<int>(std::lround(first + scaled_delta));
    };
    return timeline::RgbColor(
        channel(from.red(), to.red()), channel(from.green(), to.green()), channel(from.blue(), to.blue()));
}

timeline::Palette gradient_palette(const Json &source)
{
    if (!source.is_object() || source.value("kind", std::string()) != "gradient")
    {
        throw std::invalid_argument("unsupported color-map gradient definition");
    }
    for (const std::string &field : {"kind", "stops"})
    {
        if (!source.contains(field))
        {
            throw std::invalid_argument("gradient requires kind and stops");
        }
    }
    if (source.size() != 2)
    {
        throw std::invalid_argument("unsupported gradient field");
    }
    const Json &stops = source.at("stops");
    if (!stops.is_array() || stops.size() < 2)
    {
        throw std::invalid_argument("gradient requires at least two stops");
    }
    std::vector<std::pair<int, timeline::RgbColor>> colors;
    for (const Json &stop : stops)
    {
        if (!stop.is_object() || stop.size() != 2 || !stop.contains("index") || !stop.contains("color") ||
            !stop.at("index").is_number_integer())
        {
            throw std::invalid_argument("gradient stop requires an integer index and color");
        }
        const timeline::Ticks index = stop.at("index").get<timeline::Ticks>();
        if (index < 0 || index > 255)
        {
            throw std::invalid_argument("gradient index out of range");
        }
        if (!colors.empty() && index <= colors.back().first)
        {
            throw std::invalid_argument("gradient indices must be strictly increasing");
        }
        colors.emplace_back(static_cast<int>(index), color_spec(stop.at("color").get<std::string>()));
    }
    timeline::Palette palette;
    int stop = 0;
    for (int index = 0; index < 256; ++index)
    {
        while (stop + 1 < timeline::size_cast(colors) && colors[stop + 1].first < index)
        {
            ++stop;
        }
        if (index <= colors[stop].first || stop + 1 == timeline::size_cast(colors))
        {
            palette.push_back(colors[stop].second);
        }
        else
        {
            const double fraction =
                static_cast<double>(index - colors[stop].first) / (colors[stop + 1].first - colors[stop].first);
            palette.push_back(interpolate_color(colors[stop].second, colors[stop + 1].second, fraction));
        }
    }
    return palette;
}

timeline::Ticks key_frame(const Json &key, const timeline::FrameGrid &grid)
{
    if (!key.at("frame").is_number_integer())
    {
        throw std::invalid_argument("color-map key frame must be an integer");
    }
    const timeline::Ticks frame = key.at("frame").get<timeline::Ticks>();
    if (frame < 0 || frame >= grid.frame_count())
    {
        throw std::invalid_argument("color-map key frame out of range");
    }
    return frame;
}

std::string key_curve(const Json &key)
{
    const std::string curve = key.value("curve", std::string("linear"));
    if (curve != "linear" && curve != "step" && curve != "hold" && curve != "geometric")
    {
        throw std::invalid_argument("unsupported color-map key curve");
    }
    return curve;
}

/// HSL coordinates used only while evaluating a palette adjustment.
///
struct HslColor
{
    double m_hue;
    double m_saturation;
    double m_lightness;
};

double wrapped_hue(double hue)
{
    const double wrapped = std::fmod(hue, 360);
    return wrapped < 0 ? wrapped + 360 : wrapped;
}

HslColor hsl_from_rgb(const timeline::RgbColor &color)
{
    const double red = color.red() / 255.0;
    const double green = color.green() / 255.0;
    const double blue = color.blue() / 255.0;
    const double maximum = std::max({red, green, blue});
    const double minimum = std::min({red, green, blue});
    const double delta = maximum - minimum;
    const double lightness = (maximum + minimum) / 2;
    if (delta == 0)
    {
        return {0, 0, lightness};
    }
    const double hue = maximum == red ? 60 * std::fmod((green - blue) / delta, 6)
        : maximum == green            ? 60 * ((blue - red) / delta + 2)
                                      : 60 * ((red - green) / delta + 4);
    return {wrapped_hue(hue), delta / (1 - std::abs(2 * lightness - 1)), lightness};
}

timeline::RgbColor adjusted_color(const timeline::RgbColor &color, const std::string &kind, double amount)
{
    if (kind == "hue-shift" || kind == "saturation")
    {
        const HslColor hsl = hsl_from_rgb(color);
        return hsv_or_hsl_color(false, wrapped_hue(hsl.m_hue + (kind == "hue-shift" ? amount : 0)),
            std::clamp(hsl.m_saturation * (kind == "saturation" ? amount : 1), 0.0, 1.0), hsl.m_lightness);
    }
    const auto channel = [&kind, amount](int value)
    {
        const double transformed = kind == "brightness" ? value * amount
            : kind == "contrast"                        ? (value - 128.0) * amount + 128
                                                        : 255 * std::pow(value / 255.0, amount);
        return static_cast<int>(std::lround(std::clamp(transformed, 0.0, 255.0)));
    };
    return timeline::RgbColor(channel(color.red()), channel(color.green()), channel(color.blue()));
}

timeline::Lane effect_signal_lane(const Json &effect, const std::string &kind, const std::string &member,
    const std::string &id, const std::string &label, const timeline::FrameGrid &grid,
    const timeline::Attributes &attributes, timeline::StringTableBuilder &strings)
{
    if (!effect.contains(member))
    {
        throw std::invalid_argument("effect requires " + member);
    }
    const Json &signal = effect.at(member);
    if (!signal.is_object() || signal.size() != 1 || !signal.contains("keys") || !signal.at("keys").is_array() ||
        signal.at("keys").size() != 2)
    {
        throw std::invalid_argument(member + " signal requires exactly two keys");
    }
    const Json &keys = signal.at("keys");
    const timeline::Ticks first = key_frame(keys[0], grid);
    const timeline::Ticks last = key_frame(keys[1], grid);
    if (first >= last)
    {
        throw std::invalid_argument(member + " key frames must be strictly increasing");
    }
    const std::string destination = key_curve(keys[1]);
    if (destination == "geometric")
    {
        throw std::invalid_argument("geometric interpolation is not supported for effect " + member);
    }
    std::array<double, 2> values{};
    for (int index = 0; index < 2; ++index)
    {
        const Json &key = keys[index];
        if (!key.is_object() || !key.at("value").is_number())
        {
            throw std::invalid_argument(member + " key requires a numeric value");
        }
        if (key.size() != (key.contains("curve") ? 3 : 2))
        {
            throw std::invalid_argument("unsupported " + member + " key field");
        }
        values[index] = key.at("value").get<double>();
        if (!std::isfinite(values[index]))
        {
            throw std::invalid_argument(member + " values must be finite");
        }
        if (kind == "gamma" && values[index] <= 0)
        {
            throw std::invalid_argument("gamma amount must be positive");
        }
        if ((kind == "mask-blend" || kind == "pulse" || kind == "sparkle") &&
            (values[index] < 0 || values[index] > (kind == "sparkle" ? 255 : 1)))
        {
            throw std::invalid_argument(kind + " amount out of range");
        }
        if (member == "offset" &&
            (values[index] <= std::numeric_limits<int>::min() - 0.5 ||
                values[index] >= std::numeric_limits<int>::max() + 0.5))
        {
            throw std::invalid_argument("offset range exceeds safe source rounding");
        }
    }
    const double maximum = std::max(std::abs(values[0]), std::abs(values[1]));
    if (!std::isfinite(values[1] - values[0]) ||
        ((kind == "brightness" || kind == "contrast") && maximum > (std::numeric_limits<int>::max() - 128.0) / 255.0))
    {
        throw std::invalid_argument(member + " range exceeds safe source evaluation");
    }
    timeline::Lane lane(
        strings.intern(id), strings.intern(label), strings.intern("keyframes"), grid.offset(), grid.end_time());
    for (int index = 0; index < 2; ++index)
    {
        timeline::Attributes key_attributes = attributes;
        key_attributes["signal"] = signal.dump();
        key_attributes["source-key"] = keys[index].dump();
        key_attributes["value"] = keys[index].at("value").dump();
        key_attributes["curve"] = key_curve(keys[index]);
        key_attributes["outgoing-curve"] = index == 0 ? destination : "hold";
        const timeline::KeyframeInterpolation interpolation = index == 0 && destination == "linear"
            ? timeline::KeyframeInterpolation::LINEAR
            : timeline::KeyframeInterpolation::HOLD;
        lane.add(timeline::Keyframe(strings.intern(id + "-key-" + std::to_string(index)),
            grid.frame_start(index == 0 ? first : last), values[index], interpolation, std::move(key_attributes)));
    }
    return lane;
}

/// Owned pure transformation of a palette at an exact timeline time.
using PaletteTransform = std::function<timeline::Palette(const timeline::Palette &, timeline::Time)>;

int palette_index(const Json &value)
{
    if (!value.is_number_integer())
    {
        throw std::invalid_argument("palette index must be an integer");
    }
    const timeline::Ticks index = value.get<timeline::Ticks>();
    if (index < 0 || index > 255)
    {
        throw std::invalid_argument("palette index out of range [0, 255]");
    }
    return static_cast<int>(index);
}

std::pair<int, int> palette_range(const Json &range)
{
    if (!range.is_array() || range.size() != 2)
    {
        throw std::invalid_argument("effect range requires two indices");
    }
    const int first = palette_index(range[0]);
    const int last = palette_index(range[1]);
    if (first > last)
    {
        throw std::invalid_argument("effect range must be increasing");
    }
    return {first, last};
}

std::pair<int, int> effect_range(const Json &effect)
{
    return effect.contains("range") ? palette_range(effect.at("range")) : std::pair<int, int>{0, 255};
}

PaletteTransform masked_effect(
    const Json &effect, const std::string &kind, const std::filesystem::path &source_path, const timeline::Lane &signal)
{
    std::vector<std::pair<int, int>> ranges;
    timeline::Palette target;
    int seed = 0;
    if (kind == "mask-blend")
    {
        if (!effect.contains("ranges"))
        {
            throw std::invalid_argument("mask-blend requires ranges");
        }
        const Json &definitions = effect.at("ranges");
        if (!definitions.is_array() || definitions.empty())
        {
            throw std::invalid_argument("mask ranges must be a nonempty array");
        }
        for (const Json &range : definitions)
        {
            ranges.push_back(palette_range(range));
        }
        target = read_palette(source_path.parent_path() / effect.at("source").get<std::string>());
    }
    else
    {
        if (!effect.contains("range"))
        {
            throw std::invalid_argument(kind + " requires range");
        }
        ranges.push_back(palette_range(effect.at("range")));
        if (kind == "pulse")
        {
            target.assign(256, color_spec(effect.at("color").get<std::string>()));
        }
        else
        {
            if (!effect.contains("seed"))
            {
                throw std::invalid_argument("sparkle requires seed");
            }
            const Json &value = effect.at("seed");
            if (!value.is_number_integer())
            {
                throw std::invalid_argument("sparkle seed must be an integer");
            }
            const timeline::Ticks authored = value.get<timeline::Ticks>();
            if (authored < 0 || authored > std::numeric_limits<int>::max())
            {
                throw std::invalid_argument("sparkle seed out of range [0, 2147483647]");
            }
            seed = static_cast<int>(authored);
        }
    }
    return [kind, ranges, target, seed, signal](const timeline::Palette &colors, timeline::Time time)
    {
        const double amount = *signal.evaluate_keyframes(time);
        timeline::Palette result = colors;
        if (kind == "sparkle")
        {
            const int rounded = static_cast<int>(std::lround(amount));
            std::mt19937 engine(static_cast<std::mt19937::result_type>(seed));
            std::uniform_int_distribution<int> distribution(-rounded, rounded);
            for (int index = ranges[0].first; index <= ranges[0].second; ++index)
            {
                const int red = std::clamp(colors[index].red() + distribution(engine), 0, 255);
                const int green = std::clamp(colors[index].green() + distribution(engine), 0, 255);
                const int blue = std::clamp(colors[index].blue() + distribution(engine), 0, 255);
                result[index] = timeline::RgbColor(red, green, blue);
            }
        }
        else
        {
            for (const auto &[first, last] : ranges)
            {
                for (int index = first; index <= last; ++index)
                {
                    // Overlapping ranges blend the incoming palette, never a previous range's result.
                    result[index] = interpolate_color(colors[index], target[index], amount);
                }
            }
        }
        return result;
    };
}

timeline::Palette ping_pong_palette(const timeline::Palette &colors, int first, int last, double offset)
{
    if (first == last)
    {
        return colors;
    }
    const int span = last - first;
    const int period = span * 2;
    const int rounded = static_cast<int>(std::lround(offset));
    int phase = rounded % period;
    if (phase < 0)
    {
        phase += period;
    }
    if (phase > span)
    {
        phase = period - phase;
    }
    timeline::Palette result = colors;
    const int length = span + 1;
    for (int index = first; index <= last; ++index)
    {
        result[index] = colors[first + (index - first - phase + length) % length];
    }
    return result;
}

std::vector<PaletteTransform> palette_effects(const Json &effects, const std::string &id, const std::string &label,
    const std::filesystem::path &source_path, const timeline::FrameGrid &grid, const timeline::Attributes &attributes,
    timeline::StringTableBuilder &strings, std::vector<timeline::Lane> &lanes)
{
    if (!effects.is_array() || effects.empty())
    {
        throw std::invalid_argument("effects must be a nonempty array");
    }
    std::vector<PaletteTransform> transforms;
    int index = 0;
    for (const Json &effect : effects)
    {
        try
        {
            const std::string kind = effect.at("kind").get<std::string>();
            const bool adjustment = kind == "brightness" || kind == "contrast" || kind == "gamma" ||
                kind == "hue-shift" || kind == "saturation";
            const bool masked = kind == "mask-blend" || kind == "pulse" || kind == "sparkle";
            if (!adjustment && !masked && kind != "reverse" && kind != "remap" && kind != "ping-pong")
            {
                throw std::invalid_argument("unsupported color-map effects: " + kind);
            }
            for (const auto &[field, value] : effect.items())
            {
                if (field != "kind" && !((adjustment || masked) && field == "amount") &&
                    !((kind == "reverse" || kind == "ping-pong" || kind == "pulse" || kind == "sparkle") &&
                        field == "range") &&
                    !(kind == "ping-pong" && field == "offset") && !(kind == "remap" && field == "indices") &&
                    !(kind == "mask-blend" && (field == "ranges" || field == "source")) &&
                    !(kind == "pulse" && field == "color") && !(kind == "sparkle" && field == "seed"))
                {
                    throw std::invalid_argument("unsupported effect field: " + field);
                }
            }
            if (kind == "remap")
            {
                const Json &indices = effect.at("indices");
                if (!indices.is_array() || indices.size() != 256)
                {
                    throw std::invalid_argument("remap requires 256 indices");
                }
                std::vector<int> table;
                for (const Json &value : indices)
                {
                    table.push_back(palette_index(value));
                }
                transforms.push_back(
                    [table](const timeline::Palette &colors, timeline::Time)
                    {
                        timeline::Palette result;
                        result.reserve(table.size());
                        for (int source : table)
                        {
                            result.push_back(colors[source]);
                        }
                        return result;
                    });
                ++index;
                continue;
            }
            const std::pair<int, int> range = adjustment || masked ? std::pair<int, int>{0, 255} : effect_range(effect);
            if (kind == "reverse")
            {
                transforms.push_back(
                    [range](const timeline::Palette &colors, timeline::Time)
                    {
                        timeline::Palette result = colors;
                        for (int destination = range.first; destination <= range.second; ++destination)
                        {
                            result[destination] = colors[range.second - destination + range.first];
                        }
                        return result;
                    });
                ++index;
                continue;
            }
            const std::string member = kind == "ping-pong" ? "offset" : "amount";
            timeline::Attributes effect_attributes = attributes;
            effect_attributes["effect"] = effect.dump();
            effect_attributes["effect-kind"] = kind;
            effect_attributes["effect-index"] = std::to_string(index);
            effect_attributes["member"] = member;
            timeline::Lane signal = effect_signal_lane(effect, kind, member,
                id + "-effect-" + std::to_string(index) + "-" + member,
                label + " / " + std::to_string(index) + " " + kind + " " + member, grid, effect_attributes, strings);
            if (masked)
            {
                transforms.push_back(masked_effect(effect, kind, source_path, signal));
                lanes.push_back(std::move(signal));
                ++index;
                continue;
            }
            transforms.push_back(
                [signal, kind, range, adjustment](const timeline::Palette &colors, timeline::Time time)
                {
                    const double amount = *signal.evaluate_keyframes(time);
                    if (!adjustment)
                    {
                        return ping_pong_palette(colors, range.first, range.second, amount);
                    }
                    timeline::Palette transformed;
                    transformed.reserve(colors.size());
                    for (const timeline::RgbColor &color : colors)
                    {
                        transformed.push_back(adjusted_color(color, kind, amount));
                    }
                    return transformed;
                });
            lanes.push_back(std::move(signal));
        }
        catch (const std::exception &error)
        {
            throw std::invalid_argument("color-map effect " + std::to_string(index) + ": " + error.what());
        }
        ++index;
    }
    return transforms;
}

} // namespace

void color_map_lanes(const nlohmann::json &track, const std::filesystem::path &source_path, const std::string &id,
    const std::string &layer, const timeline::FrameGrid &grid, timeline::StringTableBuilder &strings,
    std::vector<timeline::Lane> &lanes)
{
    for (const auto &[field, value] : track.items())
    {
        if (field != "parameter" && field != "type" && field != "format" && field != "output" && field != "source" &&
            field != "keys" && field != "effects")
        {
            throw std::invalid_argument("unsupported color-map field: " + field);
        }
    }
    if (track.at("format") != "at-file")
    {
        throw std::invalid_argument("color-map format must be at-file");
    }
    if (track.contains("effects") && !track.contains("source"))
    {
        throw std::invalid_argument("effects require a source, not keyed map files");
    }
    if (track.contains("source") == track.contains("keys"))
    {
        throw std::invalid_argument("color-map requires either source or keys, not both");
    }
    const std::string output = track.at("output").get<std::string>();
    if (output.empty() || output.find_first_of("/\\:") != std::string::npos)
    {
        throw std::invalid_argument("color-map output must be a filename without a directory");
    }
    const std::string parameter = track.at("parameter").get<std::string>();
    if (parameter.empty())
    {
        throw std::invalid_argument("color-map parameter must not be empty");
    }
    const std::string label = layer.empty() ? parameter : layer + " / " + parameter;
    timeline::Attributes attributes{{"parameter", parameter}, {"layer", layer}, {"color-map", track.dump()},
        {"format", "at-file"}, {"output", output}};
    timeline::PaletteCurve::Evaluator evaluator;
    std::vector<timeline::Lane> staged;
    if (track.contains("source"))
    {
        const Json &source = track.at("source");
        const timeline::Palette colors = source.is_string()
            ? read_palette(source_path.parent_path() / source.get<std::string>())
            : gradient_palette(source);
        attributes["source"] = source.is_string() ? source.get<std::string>() : source.dump();
        evaluator = [colors](timeline::Time)
        {
            return colors;
        };
    }
    else
    {
        const Json &keys = track.at("keys");
        if (!keys.is_array() || keys.size() != 2)
        {
            throw std::invalid_argument("color-map requires exactly two keys");
        }
        const timeline::Ticks first = key_frame(keys[0], grid);
        const timeline::Ticks last = key_frame(keys[1], grid);
        if (first >= last)
        {
            throw std::invalid_argument("color-map key frames must be strictly increasing");
        }
        const std::string destination_curve = key_curve(keys[1]);
        if (destination_curve == "geometric")
        {
            throw std::invalid_argument("geometric interpolation is not supported for color maps");
        }
        const timeline::Palette from = read_palette(source_path.parent_path() / keys[0].at("value").get<std::string>());
        const timeline::Palette to = read_palette(source_path.parent_path() / keys[1].at("value").get<std::string>());
        const timeline::Time start = grid.frame_start(first);
        const timeline::Time end = grid.frame_start(last);
        const bool held = destination_curve == "step" || destination_curve == "hold";
        evaluator = [from, to, start, end, held](timeline::Time time)
        {
            const double fraction = time <= start ? 0
                : end <= time                     ? 1
                : held
                ? 0
                : static_cast<double>(time.ticks() - start.ticks()) / static_cast<double>(end.ticks() - start.ticks());
            timeline::Palette colors;
            colors.reserve(from.size());
            for (int index = 0; index < timeline::size_cast(from); ++index)
            {
                colors.push_back(interpolate_color(from[index], to[index], fraction));
            }
            return colors;
        };
        timeline::Lane definitions(strings.intern(id + "-keys"), strings.intern(label + " / keys"),
            strings.intern("keyframe"), grid.offset(), grid.end_time());
        for (int index = 0; index < 2; ++index)
        {
            timeline::Attributes key_attributes = attributes;
            const std::string filename = keys[index].at("value").get<std::string>();
            key_attributes["value"] = filename;
            key_attributes["curve"] = key_curve(keys[index]);
            key_attributes["source-key"] = keys[index].dump();
            const std::string key_id = id + "-key-" + std::to_string(index);
            const timeline::Time time = index == 0 ? start : end;
            definitions.add(timeline::Instant(strings.intern(key_id), strings.intern("keyframe"), time,
                strings.intern(filename), std::nullopt, key_attributes));
            definitions.add(timeline::Interval(strings.intern(key_id + "-hold"), strings.intern("keyframe-value"),
                index == 0 ? grid.offset() : end, index == 0 ? end : grid.end_time(), strings.intern(filename),
                std::nullopt, key_attributes));
        }
        staged.push_back(std::move(definitions));
    }
    if (track.contains("effects"))
    {
        const std::vector<PaletteTransform> transforms =
            palette_effects(track.at("effects"), id, label, source_path, grid, attributes, strings, staged);
        evaluator = [source = std::move(evaluator), transforms](timeline::Time time)
        {
            timeline::Palette colors = source(time);
            for (const PaletteTransform &transform : transforms)
            {
                colors = transform(colors, time);
            }
            return colors;
        };
    }
    timeline::Lane palette(
        strings.intern(id), strings.intern(label), strings.intern("palette"), grid.offset(), grid.end_time());
    palette.add(timeline::PaletteCurve(strings.intern(id + "-palette"), strings.intern("color-map"), grid.offset(),
        grid.end_time(), std::move(evaluator), std::move(attributes)));
    lanes.push_back(std::move(palette));
    for (timeline::Lane &lane : staged)
    {
        lanes.push_back(std::move(lane));
    }
}

} // namespace timeline_par_animator::detail
