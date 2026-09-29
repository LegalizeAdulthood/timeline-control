// Copyright (c) 2026 Richard Thomson

#include <timeline/Lane.h>

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
            if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Instant>)
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
            if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Instant>)
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

std::vector<Item> Lane::items_in_range(Time start, Time end) const
{
    if (end < start)
    {
        throw std::invalid_argument("timeline query range is reversed");
    }

    auto result = std::vector<Item>{};
    for (const auto &item : m_items)
    {
        if (item_start(item) <= end && start <= item_end(item))
        {
            result.push_back(item);
        }
    }
    return result;
}

void Lane::add_item(Item item)
{
    const auto start = item_start(item);
    const auto end = item_end(item);
    const auto instant = start == end;
    if (start < m_start || m_end < end || (instant && start == m_end))
    {
        throw std::out_of_range("timeline item is outside its lane range");
    }
    m_items.push_back(std::move(item));
}

} // namespace timeline
