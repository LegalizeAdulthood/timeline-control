// Copyright (c) 2026 Richard Thomson

#include <timeline/Query.h>

#include <stdexcept>
#include <type_traits>
#include <utility>

namespace timeline
{
namespace
{

InspectionItem inspect_item(const Item &item, InspectionItemRole role, std::optional<Time> sample_time)
{
    return std::visit(
        [role, sample_time](const auto &value) -> InspectionItem
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
                return {value.id(), "keyframe", InspectionItemType::KEYFRAME, role,
                    std::optional<double>{value.value()}, value.attributes()};
            }
        },
        item);
}

InspectionItem inspect_keyframe(const Keyframe &keyframe, InspectionItemRole role)
{
    return {keyframe.id(), "keyframe", InspectionItemType::KEYFRAME, role, std::optional<double>{keyframe.value()},
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
        for (const Item &item : lane.items())
        {
            if (!std::holds_alternative<Keyframe>(item) && active_in_frame(item, start, end))
            {
                const InspectionItemRole role =
                    std::holds_alternative<Curve>(item) || std::holds_alternative<PaletteCurve>(item)
                    ? InspectionItemRole::SAMPLED
                    : InspectionItemRole::ACTIVE;
                lane_inspection.items.push_back(inspect_item(item, role, start));
            }
        }

        const KeyframeNeighbors neighbors = lane.neighboring_keyframes(start);
        if (neighbors.before() && neighbors.after() && neighbors.before()->get().id() == neighbors.after()->get().id())
        {
            lane_inspection.items.push_back(inspect_keyframe(neighbors.before()->get(), InspectionItemRole::EXACT));
        }
        else
        {
            if (neighbors.before())
            {
                lane_inspection.items.push_back(
                    inspect_keyframe(neighbors.before()->get(), InspectionItemRole::BEFORE));
            }
            if (neighbors.after())
            {
                lane_inspection.items.push_back(inspect_keyframe(neighbors.after()->get(), InspectionItemRole::AFTER));
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
                lane_inspection.items.push_back(inspect_item(item, InspectionItemRole::ACTIVE, std::nullopt));
            }
        }
        inspection.lanes.push_back(std::move(lane_inspection));
    }
    return inspection;
}

} // namespace timeline
