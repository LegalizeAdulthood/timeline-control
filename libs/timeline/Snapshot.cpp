// Copyright (c) 2026 Richard Thomson

#include <timeline/Snapshot.h>

#include <timeline/size_cast.h>

#include <locale>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace timeline
{
namespace
{

std::string_view style_name(StyleRole style)
{
    switch (style)
    {
    case StyleRole::RULER:
        return "RULER";
    case StyleRole::RULER_LABEL:
        return "RULER_LABEL";
    case StyleRole::LANE_BACKGROUND:
        return "LANE_BACKGROUND";
    case StyleRole::LANE_LABEL:
        return "LANE_LABEL";
    case StyleRole::INSTANT_MARKER:
        return "INSTANT_MARKER";
    case StyleRole::INTERVAL_SPAN:
        return "INTERVAL_SPAN";
    case StyleRole::ENVELOPE_ATTACK:
        return "ENVELOPE_ATTACK";
    case StyleRole::ENVELOPE_SUSTAIN:
        return "ENVELOPE_SUSTAIN";
    case StyleRole::ENVELOPE_DECAY:
        return "ENVELOPE_DECAY";
    case StyleRole::CURVE:
        return "CURVE";
    case StyleRole::KEYFRAME_SEGMENT:
        return "KEYFRAME_SEGMENT";
    case StyleRole::KEYFRAME_MARKER:
        return "KEYFRAME_MARKER";
    case StyleRole::SELECTED_LANE:
        return "SELECTED_LANE";
    case StyleRole::SELECTED_ITEM:
        return "SELECTED_ITEM";
    case StyleRole::SELECTED_RANGE:
        return "SELECTED_RANGE";
    case StyleRole::PLAYHEAD:
        return "PLAYHEAD";
    }
    throw std::invalid_argument("unknown snapshot style role");
}

std::string quoted(std::string_view text)
{
    constexpr std::string_view HEX = "0123456789ABCDEF";
    std::string result = "\"";
    for (const unsigned char value : text)
    {
        switch (value)
        {
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            if (value < 32 || value > 126)
            {
                result += "\\x";
                result += HEX[value >> 4];
                result += HEX[value & 15];
            }
            else
            {
                result += static_cast<char>(value);
            }
        }
    }
    result += '"';
    return result;
}

} // namespace

std::string render_snapshot(const DisplayList &display_list)
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    for (const Primitive &primitive : display_list.primitives())
    {
        std::visit(
            [&output](const auto &value)
            {
                using Value = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, Line>)
                {
                    output << "line";
                }
                else if constexpr (std::is_same_v<Value, Rectangle>)
                {
                    output << "rectangle";
                }
                else if constexpr (std::is_same_v<Value, Text>)
                {
                    output << "text";
                }
                else if constexpr (std::is_same_v<Value, Marker>)
                {
                    output << "marker";
                }
                else
                {
                    output << "polyline";
                }
                output << ' ' << style_name(value.style) << ' ' << quoted(value.id.lane_id) << ' '
                       << quoted(value.id.item_id);
                if constexpr (std::is_same_v<Value, Line>)
                {
                    output << ' ' << value.x1 << ' ' << value.y1 << ' ' << value.x2 << ' ' << value.y2;
                }
                else if constexpr (std::is_same_v<Value, Rectangle> || std::is_same_v<Value, Marker>)
                {
                    output << ' ' << value.x << ' ' << value.y << ' ' << value.width << ' ' << value.height;
                }
                else if constexpr (std::is_same_v<Value, Text>)
                {
                    output << ' ' << value.x << ' ' << value.y << ' ' << quoted(value.value);
                }
                else
                {
                    output << ' ' << size_cast(value.points);
                    for (const Point &point : value.points)
                    {
                        output << ' ' << point.x << ' ' << point.y;
                    }
                }
                output << '\n';
            },
            primitive);
    }
    return output.str();
}

} // namespace timeline
