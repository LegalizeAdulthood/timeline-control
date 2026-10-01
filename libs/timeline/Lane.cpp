// Copyright (c) 2026 Richard Thomson

#include <timeline/Lane.h>

#include <cmath>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace timeline
{

Time item_start(const Item &item)
{
    return std::visit(
        [](const auto &value)
        {
            using Value = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, Instant> || std::is_same_v<Value, Keyframe>)
            {
                return value.time();
            }
            else
            {
                return value.start();
            }
        },
        item);
}

Time item_end(const Item &item)
{
    return std::visit(
        [](const auto &value)
        {
            using Value = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, Instant> || std::is_same_v<Value, Keyframe>)
            {
                return value.time();
            }
            else
            {
                return value.end();
            }
        },
        item);
}

Lane::Lane(std::string id, std::string label, std::string kind, Time start, Time end) :
    m_id(std::move(id)),
    m_label(std::move(label)),
    m_kind(std::move(kind)),
    m_start(start),
    m_end(end)
{
    if (m_id.empty() || m_kind.empty())
    {
        throw std::invalid_argument("timeline lanes require an id and kind");
    }
    if (m_end <= m_start)
    {
        throw std::invalid_argument("timeline lanes require a positive time range");
    }
}

void Lane::add(Instant instant)
{
    add_item(std::move(instant));
}

void Lane::add(Interval interval)
{
    add_item(std::move(interval));
}

void Lane::add(Envelope envelope)
{
    add_item(std::move(envelope));
}

void Lane::add(Curve curve)
{
    add_item(std::move(curve));
}

void Lane::add(PaletteCurve curve)
{
    add_item(std::move(curve));
}

void Lane::add(Keyframe keyframe)
{
    const KeyframeNeighbors neighbors = neighboring_keyframes(keyframe.time());
    if ((neighbors.before() && neighbors.before()->get().interpolation() == KeyframeInterpolation::GEOMETRIC &&
            keyframe.value() <= 0.0) ||
        (neighbors.after() && keyframe.interpolation() == KeyframeInterpolation::GEOMETRIC &&
            neighbors.after()->get().value() <= 0.0))
    {
        throw std::invalid_argument("geometric segments require positive endpoints");
    }
    for (const Item &item : m_items)
    {
        const auto existing = std::get_if<Keyframe>(&item);
        if (existing && existing->time() == keyframe.time())
        {
            throw std::invalid_argument("timeline keyframe times must be unique within a lane");
        }
    }
    add_item(std::move(keyframe));
}

std::vector<Item> Lane::items_in_range(Time start, Time end) const
{
    if (end < start)
    {
        throw std::invalid_argument("timeline query range is reversed");
    }

    std::vector<Item> result{};
    for (const Item &item : m_items)
    {
        if (item_start(item) <= end && start <= item_end(item))
        {
            result.push_back(item);
        }
    }
    return result;
}

KeyframeNeighbors Lane::neighboring_keyframes(Time time) const
{
    std::optional<KeyframeNeighbors::Reference> before{};
    std::optional<KeyframeNeighbors::Reference> after{};
    for (const Item &item : m_items)
    {
        const auto keyframe = std::get_if<Keyframe>(&item);
        if (!keyframe)
        {
            continue;
        }
        if (keyframe->time() <= time && (!before || before->get().time() < keyframe->time()))
        {
            before = std::cref(*keyframe);
        }
        if (time <= keyframe->time() && (!after || keyframe->time() < after->get().time()))
        {
            after = std::cref(*keyframe);
        }
    }
    return KeyframeNeighbors(before, after);
}

std::optional<double> Lane::evaluate_keyframes(Time time) const
{
    if (m_keyframe_evaluator)
    {
        const std::optional<double> value = m_keyframe_evaluator(time);
        if (value && !std::isfinite(*value))
        {
            throw std::invalid_argument("keyframe evaluator must return finite values");
        }
        return value;
    }
    const KeyframeNeighbors neighbors = neighboring_keyframes(time);
    if (!neighbors.before() && !neighbors.after())
    {
        return std::nullopt;
    }
    if (!neighbors.before())
    {
        return neighbors.after()->get().value();
    }
    if (!neighbors.after())
    {
        return neighbors.before()->get().value();
    }

    const Keyframe &before = neighbors.before()->get();
    const Keyframe &after = neighbors.after()->get();
    if (before.time() == after.time() || before.interpolation() == KeyframeInterpolation::HOLD)
    {
        return before.value();
    }

    const double elapsed = static_cast<double>((time - before.time()).ticks());
    const double duration = static_cast<double>((after.time() - before.time()).ticks());
    if (before.interpolation() == KeyframeInterpolation::GEOMETRIC)
    {
        return std::exp(
            std::log(before.value()) + (std::log(after.value()) - std::log(before.value())) * elapsed / duration);
    }
    return before.value() + (after.value() - before.value()) * elapsed / duration;
}

void Lane::set_keyframe_evaluator(KeyframeEvaluator evaluator)
{
    if (!evaluator || !neighboring_keyframes(m_start).after())
    {
        throw std::invalid_argument("keyframe evaluator requires an owned recipe and authored keys");
    }
    m_keyframe_evaluator = std::move(evaluator);
}

void Lane::add_item(Item item)
{
    const Time start = item_start(item);
    const Time end = item_end(item);
    const bool instant = start == end;
    if (start < m_start || m_end < end || (instant && start == m_end))
    {
        throw std::out_of_range("timeline item is outside its lane range");
    }
    m_items.push_back(std::move(item));
}

} // namespace timeline
