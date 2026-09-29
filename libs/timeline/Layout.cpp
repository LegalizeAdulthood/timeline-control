// Copyright (c) 2026 Richard Thomson

#include <timeline/Layout.h>
#include <timeline/size_cast.h>

#include <algorithm>
#include <cmath>
#include <functional>
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

std::pair<double, double> curve_range(const Curve &curve)
{
    const auto [sample_minimum, sample_maximum] = std::minmax_element(curve.samples().begin(), curve.samples().end(),
        [](const CurveSample &lhs, const CurveSample &rhs) { return lhs.value() < rhs.value(); });
    return {curve.minimum().value_or(sample_minimum->value()), curve.maximum().value_or(sample_maximum->value())};
}

void add_curve(DisplayList &display_list, const Curve &curve, int y, int height, const Viewport &viewport,
    const LayoutMetrics &metrics, const std::optional<FrameGrid> &frame_grid)
{
    const auto samples = frame_grid ? curve.sample(*frame_grid) : curve.samples();
    const auto [minimum, maximum] = curve_range(curve);
    auto points = std::vector<Point>{};
    for (const auto &sample : samples)
    {
        if (sample.time() < curve.start() || curve.end() < sample.time() || sample.time() < viewport.start() ||
            viewport.end() < sample.time())
        {
            continue;
        }
        const auto normalized = maximum == minimum ? 0.5 : (sample.value() - minimum) / (maximum - minimum);
        const auto point_y = y + height - 1 - static_cast<int>(std::lround(normalized * (height - 1)));
        points.push_back(Point{time_x(sample.time(), viewport, metrics), point_y});
    }
    if (size_cast(points) >= 2)
    {
        display_list.add(Polyline{std::move(points), StyleRole::CURVE});
    }
}

int keyframe_y(double value, double minimum, double maximum, int y, int height)
{
    const auto normalized = maximum == minimum ? 0.5 : (value - minimum) / (maximum - minimum);
    return y + height - 1 - static_cast<int>(std::lround(normalized * (height - 1)));
}

void add_keyframes(DisplayList &display_list, const Lane &lane, int y, int height, const Viewport &viewport,
    const LayoutMetrics &metrics)
{
    auto keyframes = std::vector<std::reference_wrapper<const Keyframe>>{};
    for (const auto &item : lane.items())
    {
        const auto keyframe = std::get_if<Keyframe>(&item);
        if (keyframe)
        {
            keyframes.emplace_back(std::cref(*keyframe));
        }
    }
    if (keyframes.empty())
    {
        return;
    }
    std::sort(keyframes.begin(), keyframes.end(),
        [](const auto &lhs, const auto &rhs) { return lhs.get().time() < rhs.get().time(); });
    const auto [minimum, maximum] = std::minmax_element(keyframes.begin(), keyframes.end(),
        [](const auto &lhs, const auto &rhs) { return lhs.get().value() < rhs.get().value(); });
    const auto minimum_value = minimum->get().value();
    const auto maximum_value = maximum->get().value();

    for (auto index = 1; index < size_cast(keyframes); ++index)
    {
        const auto &left = keyframes[index - 1].get();
        const auto &right = keyframes[index].get();
        if (right.time() < viewport.start() || viewport.end() < left.time())
        {
            continue;
        }
        const auto left_point = Point{
            time_x(left.time(), viewport, metrics), keyframe_y(left.value(), minimum_value, maximum_value, y, height)};
        const auto right_point = Point{time_x(right.time(), viewport, metrics),
            keyframe_y(right.value(), minimum_value, maximum_value, y, height)};
        auto points = std::vector<Point>{left_point};
        if (left.interpolation() == KeyframeInterpolation::HOLD)
        {
            points.push_back(Point{right_point.x, left_point.y});
        }
        points.push_back(right_point);
        display_list.add(Polyline{std::move(points), StyleRole::KEYFRAME_SEGMENT});
    }

    for (const auto &reference : keyframes)
    {
        const auto &keyframe = reference.get();
        if (keyframe.time() < viewport.start() || viewport.end() < keyframe.time())
        {
            continue;
        }
        const auto x = time_x(keyframe.time(), viewport, metrics);
        const auto point_y = keyframe_y(keyframe.value(), minimum_value, maximum_value, y, height);
        const auto marker_width = std::min(5, viewport.width() - metrics.lane_label_width());
        const auto marker_height = std::min(5, height);
        const auto marker_x =
            std::clamp(x - marker_width / 2, metrics.lane_label_width(), viewport.width() - marker_width);
        const auto marker_y = std::clamp(point_y - marker_height / 2, y, y + height - marker_height);
        display_list.add(Rectangle{marker_x, marker_y, marker_width, marker_height, StyleRole::KEYFRAME_MARKER});
    }
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
        add_keyframes(m_display_list, lane, item_y, item_height, viewport, metrics);
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
                    else if constexpr (std::is_same_v<Value, Envelope>)
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
                    else if constexpr (std::is_same_v<Value, Curve>)
                    {
                        add_curve(m_display_list, value, item_y, item_height, viewport, metrics, document.frame_grid());
                    }
                },
                item);
        }
        y += metrics.lane_height();
    }
}

Time time_at_x(int x, const Viewport &viewport, const LayoutMetrics &metrics)
{
    if (metrics.lane_label_width() >= viewport.width())
    {
        throw std::invalid_argument("timeline lane labels leave no content width");
    }

    const auto left = metrics.lane_label_width();
    const auto position = std::clamp(x, left, viewport.width()) - left;
    const auto width = viewport.width() - left;
    const auto duration = viewport.end().ticks() - viewport.start().ticks();
    const auto elapsed = static_cast<Ticks>(std::llround(static_cast<double>(position) * duration / width));
    return Time::from_ticks(viewport.start().ticks() + elapsed);
}

} // namespace timeline
