// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/Curve.h>
#include <timeline/Envelope.h>
#include <timeline/Interval.h>
#include <timeline/Keyframe.h>
#include <timeline/size_cast.h>

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace timeline
{

/// One generic piece of timeline content.
using Item = std::variant<Instant, Interval, Envelope, Curve, Keyframe>;

/// Ordered collection of timeline content within a finite range.
///
/// A lane owns its items, retains insertion order, and rejects items that do
/// not fit completely inside its half-open time range.
///
class Lane
{
public:
    Lane(std::string id, std::string label, std::string kind, Time start, Time end);

    const std::string &id() const
    {
        return m_id;
    }
    const std::string &label() const
    {
        return m_label;
    }
    const std::string &kind() const
    {
        return m_kind;
    }
    Time start() const
    {
        return m_start;
    }
    Time end() const
    {
        return m_end;
    }
    const std::vector<Item> &items() const
    {
        return m_items;
    }
    int item_count() const
    {
        return size_cast(m_items);
    }

    void add(Instant instant);
    void add(Interval interval);
    void add(Envelope envelope);
    void add(Curve curve);
    void add(Keyframe keyframe);
    std::vector<Item> items_in_range(Time start, Time end) const;
    KeyframeNeighbors neighboring_keyframes(Time time) const;
    std::optional<double> evaluate_keyframes(Time time) const;

private:
    void add_item(Item item);

    std::string m_id;
    std::string m_label;
    std::string m_kind;
    Time m_start;
    Time m_end;
    std::vector<Item> m_items;
};

Time item_start(const Item &item);
Time item_end(const Item &item);

} // namespace timeline
