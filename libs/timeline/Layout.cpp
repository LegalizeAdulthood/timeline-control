// Copyright (c) 2026 Richard Thomson

#include <timeline/Layout.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <type_traits>

namespace timeline
{
namespace
{

int time_x(Time time, const Viewport &viewport, const LayoutMetrics &metrics)
{
    const auto ticks = std::clamp(time.ticks(), viewport.start().ticks(), viewport.end().ticks());
    const auto elapsed = static_cast<double>(ticks - viewport.start().ticks());
    const auto duration = static_cast<double>(viewport.end().ticks() - viewport.start().ticks());
    const auto timeline_width = viewport.width() - metrics.lane_label_width();
    return metrics.lane_label_width() + static_cast<int>(std::lround(elapsed * timeline_width / duration));
}

void add_span(DisplayList &display_list, Time start, Time end, StyleRole style, int y, int height,
    const Viewport &viewport, const LayoutMetrics &metrics)
{
    if (end <= viewport.start() || viewport.end() <= start || end <= start)
    {
        return;
    }
    const auto x1 = time_x(start, viewport, metrics);
    const auto x2 = time_x(end, viewport, metrics);
    display_list.add(Rectangle{x1, y, std::max(1, x2 - x1), height, style});
}

Ticks phase_ticks(const std::optional<Duration> &phase)
{
    return phase ? phase->ticks() : 0;
}

} // namespace

Viewport::Viewport(int width, int height, Time start, Time end) :
    m_width(width),
    m_height(height),
    m_start(start),
    m_end(end)
{
    if (m_width <= 0 || m_height <= 0 || m_end <= m_start)
    {
        throw std::invalid_argument("timeline viewport requires positive dimensions and duration");
    }
}

LayoutMetrics::LayoutMetrics(int lane_label_width, int ruler_height, int lane_height, int item_padding) :
    m_lane_label_width(lane_label_width),
    m_ruler_height(ruler_height),
    m_lane_height(lane_height),
    m_item_padding(item_padding)
{
    if (m_lane_label_width < 0 || m_ruler_height <= 0 || m_lane_height <= 0 || m_item_padding < 0 ||
        m_item_padding * 2 >= m_lane_height)
    {
        throw std::invalid_argument("timeline layout metrics are invalid");
    }
}

Layout::Layout(const Document &document, Viewport viewport, LayoutMetrics metrics)
{
    if (metrics.lane_label_width() >= viewport.width())
    {
        throw std::invalid_argument("timeline lane labels leave no content width");
    }

    m_display_list.add(Line{metrics.lane_label_width(), metrics.ruler_height() - 1, viewport.width() - 1,
        metrics.ruler_height() - 1, StyleRole::RULER});
    m_display_list.add(Text{4, 4, "Time", StyleRole::RULER_LABEL});

    auto y = metrics.ruler_height();
    for (const auto &lane : document.lanes())
    {
        if (y >= viewport.height())
        {
            break;
        }
        const auto row_height = std::min(metrics.lane_height(), viewport.height() - y);
        m_display_list.add(Rectangle{metrics.lane_label_width(), y, viewport.width() - metrics.lane_label_width(),
            row_height, StyleRole::LANE_BACKGROUND});
        m_display_list.add(Text{4, y + metrics.item_padding(), lane.label(), StyleRole::LANE_LABEL});

        const auto item_y = y + metrics.item_padding();
        const auto item_height = std::max(1, row_height - metrics.item_padding() * 2);
        for (const auto &item : lane.items())
        {
            if (item_end(item) < viewport.start() || viewport.end() < item_start(item))
            {
                continue;
            }
            std::visit(
                [&](const auto &value)
                {
                    using Value = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<Value, Instant>)
                    {
                        const auto x = time_x(value.time(), viewport, metrics);
                        m_display_list.add(Line{x, item_y, x, item_y + item_height, StyleRole::INSTANT_MARKER});
                    }
                    else if constexpr (std::is_same_v<Value, Interval>)
                    {
                        add_span(m_display_list, value.start(), value.end(), StyleRole::INTERVAL_SPAN, item_y,
                            item_height, viewport, metrics);
                    }
                    else
                    {
                        auto phase_start = value.start();
                        auto phase_end = phase_start + Duration::from_ticks(phase_ticks(value.attack()));
                        add_span(m_display_list, phase_start, phase_end, StyleRole::ENVELOPE_ATTACK, item_y,
                            item_height, viewport, metrics);
                        phase_start = phase_end;
                        phase_end = phase_start + Duration::from_ticks(phase_ticks(value.sustain()));
                        add_span(m_display_list, phase_start, phase_end, StyleRole::ENVELOPE_SUSTAIN, item_y,
                            item_height, viewport, metrics);
                        phase_start = phase_end;
                        phase_end = phase_start + Duration::from_ticks(phase_ticks(value.decay()));
                        add_span(m_display_list, phase_start, phase_end, StyleRole::ENVELOPE_DECAY, item_y, item_height,
                            viewport, metrics);
                    }
                },
                item);
        }
        y += metrics.lane_height();
    }
}

} // namespace timeline
