// Copyright (c) 2026 Richard Thomson

#include "ColorMap.h"
#include "NamedColors.h"

#include <timeline/size_cast.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
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
    const double chroma = format == "hsv" ? saturation * third : (1 - std::abs(2 * third - 1)) * saturation;
    const double match = format == "hsv" ? third - chroma : third - chroma / 2;
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
        return static_cast<int>(std::lround(first + fraction * (last - first)));
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

} // namespace

void color_map_lanes(const nlohmann::json &track, const std::filesystem::path &source_path, const std::string &id,
    const std::string &layer, const timeline::FrameGrid &grid, std::vector<timeline::Lane> &lanes)
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
    if (track.contains("effects"))
    {
        throw std::invalid_argument("color-map effects are not supported yet");
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
        timeline::Lane definitions(id + "-keys", label + " / keys", "keyframe", grid.offset(), grid.end_time());
        for (int index = 0; index < 2; ++index)
        {
            timeline::Attributes key_attributes = attributes;
            const std::string filename = keys[index].at("value").get<std::string>();
            key_attributes["value"] = filename;
            key_attributes["curve"] = key_curve(keys[index]);
            key_attributes["source-key"] = keys[index].dump();
            const std::string key_id = id + "-key-" + std::to_string(index);
            const timeline::Time time = index == 0 ? start : end;
            definitions.add(timeline::Instant(key_id, "keyframe", time, filename, std::nullopt, key_attributes));
            definitions.add(timeline::Interval(key_id + "-hold", "keyframe-value", index == 0 ? grid.offset() : end,
                index == 0 ? end : grid.end_time(), filename, std::nullopt, key_attributes));
        }
        staged.push_back(std::move(definitions));
    }
    timeline::Lane palette(id, label, "palette", grid.offset(), grid.end_time());
    palette.add(timeline::PaletteCurve(
        id + "-palette", "color-map", grid.offset(), grid.end_time(), std::move(evaluator), std::move(attributes)));
    lanes.push_back(std::move(palette));
    for (timeline::Lane &lane : staged)
    {
        lanes.push_back(std::move(lane));
    }
}

} // namespace timeline_par_animator::detail
