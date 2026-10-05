// Copyright (c) 2026 Richard Thomson

#include <timeline/Query.h>

#include <timeline/size_cast.h>

#include <locale>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace timeline
{

std::string_view to_string(InspectionItemType value)
{
    switch (value)
    {
    case InspectionItemType::INSTANT:
        return "instant";
    case InspectionItemType::INTERVAL:
        return "interval";
    case InspectionItemType::ENVELOPE:
        return "envelope";
    case InspectionItemType::CURVE:
        return "curve";
    case InspectionItemType::KEYFRAME:
        return "keyframe";
    case InspectionItemType::PALETTE:
        return "palette";
    }
    throw std::invalid_argument("unknown inspection item type");
}

std::string_view to_string(InspectionItemRole value)
{
    switch (value)
    {
    case InspectionItemRole::ACTIVE:
        return "active";
    case InspectionItemRole::SAMPLED:
        return "sampled";
    case InspectionItemRole::BEFORE:
        return "before";
    case InspectionItemRole::AFTER:
        return "after";
    case InspectionItemRole::EXACT:
        return "exact";
    }
    throw std::invalid_argument("unknown inspection item role");
}

std::string to_string(const Document &document, const FrameInspection &inspection)
{
    std::ostringstream text;
    text.imbue(std::locale::classic());
    text << "Frame: " << inspection.frame << "\nTime: " << document.timebase().seconds(inspection.time) << " seconds";
    for (const LaneInspection &lane : inspection.lanes)
    {
        text << "\n\n"
             << document.strings().lookup(lane.label) << " [" << document.strings().lookup(lane.kind)
             << "]\nSource items: " << lane.item_count;
        if (lane.value)
        {
            text << "\nFrame value: " << *lane.value;
        }
        if (lane.output_value)
        {
            text << "\nParameter output: " << *lane.output_value;
        }
        if (lane.items.empty())
        {
            text << "\nNo activity";
        }
        for (const InspectionItem &item : lane.items)
        {
            text << "\nItem: " << document.strings().lookup(item.id) << " [" << document.strings().lookup(item.kind)
                 << "] (" << to_string(item.role) << ')';
            if (item.value)
            {
                text << " Value: " << *item.value;
            }
            for (const Attribute &attribute : item.attributes.values())
            {
                text << '\n'
                     << document.strings().lookup(attribute.key()) << ": "
                     << document.strings().lookup(attribute.value());
            }
            if (item.palette)
            {
                text << "\nPalette: " << size_cast(*item.palette) << " colors";
                for (int index = 0; index < size_cast(*item.palette); ++index)
                {
                    const RgbColor &color = (*item.palette)[index];
                    text << "\n[" << index << "] RGB " << color.red() << '/' << color.green() << '/' << color.blue();
                }
            }
        }
    }
    return text.str();
}

namespace
{

InspectionItem inspect_item(
    const Item &item, InspectionItemRole role, std::optional<Time> sample_time, StringId keyframe_kind)
{
    return std::visit(
        [role, sample_time, keyframe_kind](const auto &value) -> InspectionItem
        {
            using Value = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, Instant>)
            {
                return {
                    value.id(), value.kind(), InspectionItemType::INSTANT, role, value.strength(), value.attributes()};
            }
            else if constexpr (std::is_same_v<Value, Interval>)
            {
                return {
                    value.id(), value.kind(), InspectionItemType::INTERVAL, role, value.strength(), value.attributes()};
            }
            else if constexpr (std::is_same_v<Value, Envelope>)
            {
                return {
                    value.id(), value.kind(), InspectionItemType::ENVELOPE, role, value.strength(), value.attributes()};
            }
            else if constexpr (std::is_same_v<Value, Curve>)
            {
                const std::optional<double> sample =
                    sample_time ? std::optional{value.sample(*sample_time)} : std::nullopt;
                return {value.id(), value.kind(), InspectionItemType::CURVE, role, sample, value.attributes()};
            }
            else if constexpr (std::is_same_v<Value, PaletteCurve>)
            {
                const std::optional<Palette> sample =
                    sample_time ? std::optional<Palette>{value.sample(*sample_time)} : std::nullopt;
                return {value.id(), value.kind(), InspectionItemType::PALETTE, role, std::nullopt, value.attributes(),
                    sample};
            }
            else
            {
                return {value.id(), keyframe_kind, InspectionItemType::KEYFRAME, role,
                    std::optional<double>{value.value()}, value.attributes()};
            }
        },
        item);
}

InspectionItem inspect_keyframe(const Keyframe &keyframe, InspectionItemRole role, StringId kind)
{
    return {keyframe.id(), kind, InspectionItemType::KEYFRAME, role, std::optional<double>{keyframe.value()},
        keyframe.attributes()};
}

LaneInspection lane_summary(const Lane &lane)
{
    return {lane.id(), lane.label(), lane.kind(), lane.item_count(), {}, std::nullopt};
}

bool active_in_frame(const Item &item, Time start, Time end)
{
    return std::visit(
        [start, end](const auto &value)
        {
            using Value = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, Instant>)
            {
                return start <= value.time() && value.time() < end;
            }
            else if constexpr (std::is_same_v<Value, Interval> || std::is_same_v<Value, Envelope>)
            {
                return value.start() < end && start < value.end();
            }
            else if constexpr (std::is_same_v<Value, Curve> || std::is_same_v<Value, PaletteCurve>)
            {
                return value.start() <= start && start <= value.end();
            }
            else
            {
                return false;
            }
        },
        item);
}

bool overlaps_range(const Item &item, Time start, Time end)
{
    return item_start(item) <= end && start <= item_end(item);
}

} // namespace

std::optional<FrameInspection> inspect_frame(const Document &document, Ticks frame)
{
    if (!document.frame_grid() || frame < 0 || document.frame_grid()->frame_count() <= frame)
    {
        return std::nullopt;
    }

    const FrameGrid &frame_grid = *document.frame_grid();
    const Time start = frame_grid.frame_start(frame);
    const Time end = frame + 1 < frame_grid.frame_count() ? frame_grid.frame_start(frame + 1) : frame_grid.end_time();
    FrameInspection inspection{frame, start, {}};
    inspection.lanes.reserve(document.lanes().size());

    for (const Lane &lane : document.lanes())
    {
        LaneInspection lane_inspection = lane_summary(lane);
        lane_inspection.value = lane.evaluate_keyframes(start);
        lane_inspection.output_value = lane.evaluate_keyframe_output(start);
        for (const Item &item : lane.items())
        {
            if (!std::holds_alternative<Keyframe>(item) && active_in_frame(item, start, end))
            {
                const InspectionItemRole role =
                    std::holds_alternative<Curve>(item) || std::holds_alternative<PaletteCurve>(item)
                    ? InspectionItemRole::SAMPLED
                    : InspectionItemRole::ACTIVE;
                lane_inspection.items.push_back(inspect_item(item, role, start, lane.kind()));
            }
        }

        const KeyframeNeighbors neighbors = lane.neighboring_keyframes(start);
        if (neighbors.before() && neighbors.after() && neighbors.before()->get().id() == neighbors.after()->get().id())
        {
            lane_inspection.items.push_back(
                inspect_keyframe(neighbors.before()->get(), InspectionItemRole::EXACT, lane.kind()));
        }
        else
        {
            if (neighbors.before())
            {
                lane_inspection.items.push_back(
                    inspect_keyframe(neighbors.before()->get(), InspectionItemRole::BEFORE, lane.kind()));
            }
            if (neighbors.after())
            {
                lane_inspection.items.push_back(
                    inspect_keyframe(neighbors.after()->get(), InspectionItemRole::AFTER, lane.kind()));
            }
        }
        inspection.lanes.push_back(std::move(lane_inspection));
    }
    return inspection;
}

RangeInspection inspect_range(const Document &document, Time start, Time end)
{
    if (end < start)
    {
        throw std::invalid_argument("timeline query range is reversed");
    }

    RangeInspection inspection{start, end, {}};
    inspection.lanes.reserve(document.lanes().size());
    for (const Lane &lane : document.lanes())
    {
        LaneInspection lane_inspection = lane_summary(lane);
        for (const Item &item : lane.items())
        {
            if (overlaps_range(item, start, end))
            {
                lane_inspection.items.push_back(
                    inspect_item(item, InspectionItemRole::ACTIVE, std::nullopt, lane.kind()));
            }
        }
        inspection.lanes.push_back(std::move(lane_inspection));
    }
    return inspection;
}

} // namespace timeline
