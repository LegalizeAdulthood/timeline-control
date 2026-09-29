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
                return {value.id(), value.kind(), InspectionItemType::INSTANT, role, value.strength()};
            }
            else if constexpr (std::is_same_v<Value, Interval>)
            {
                return {value.id(), value.kind(), InspectionItemType::INTERVAL, role, value.strength()};
            }
            else if constexpr (std::is_same_v<Value, Envelope>)
            {
                return {value.id(), value.kind(), InspectionItemType::ENVELOPE, role, value.strength()};
            }
            else if constexpr (std::is_same_v<Value, Curve>)
            {
                const auto sample = sample_time ? std::optional<double>{value.sample(*sample_time)} : std::nullopt;
                return {value.id(), value.kind(), InspectionItemType::CURVE, role, sample};
            }
            else
            {
                return {
                    value.id(), "keyframe", InspectionItemType::KEYFRAME, role, std::optional<double>{value.value()}};
            }
        },
        item);
}

InspectionItem inspect_keyframe(const Keyframe &keyframe, InspectionItemRole role)
{
    return {keyframe.id(), "keyframe", InspectionItemType::KEYFRAME, role, std::optional<double>{keyframe.value()}};
}

LaneInspection lane_summary(const Lane &lane)
{
    return {lane.id(), lane.label(), lane.kind(), lane.item_count(), {}};
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
            else if constexpr (std::is_same_v<Value, Curve>)
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

    const auto &frame_grid = *document.frame_grid();
    const auto start = frame_grid.frame_start(frame);
    const auto end = frame + 1 < frame_grid.frame_count() ? frame_grid.frame_start(frame + 1) : frame_grid.end_time();
    auto inspection = FrameInspection{frame, start, {}};
    inspection.lanes.reserve(document.lanes().size());

    for (const auto &lane : document.lanes())
    {
        auto lane_inspection = lane_summary(lane);
        for (const auto &item : lane.items())
        {
            if (!std::holds_alternative<Keyframe>(item) && active_in_frame(item, start, end))
            {
                const auto role =
                    std::holds_alternative<Curve>(item) ? InspectionItemRole::SAMPLED : InspectionItemRole::ACTIVE;
                lane_inspection.items.push_back(inspect_item(item, role, start));
            }
        }

        const auto neighbors = lane.neighboring_keyframes(start);
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

    auto inspection = RangeInspection{start, end, {}};
    inspection.lanes.reserve(document.lanes().size());
    for (const auto &lane : document.lanes())
    {
        auto lane_inspection = lane_summary(lane);
        for (const auto &item : lane.items())
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
