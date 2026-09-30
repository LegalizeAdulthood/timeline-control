// Copyright (c) 2026 Richard Thomson

#include <timeline/Interaction.h>
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
    const Ticks ticks = std::clamp(time.ticks(), viewport.start().ticks(), viewport.end().ticks());
    const double elapsed = static_cast<double>(ticks - viewport.start().ticks());
    const double duration = static_cast<double>(viewport.end().ticks() - viewport.start().ticks());
    const int timeline_width = viewport.width() - metrics.lane_label_width();
    return metrics.lane_label_width() + static_cast<int>(std::lround(elapsed * timeline_width / duration));
}

void add_span(DisplayList &display_list, Time start, Time end, StyleRole style, DisplayId id, int y, int height,
    const Viewport &viewport, const LayoutMetrics &metrics)
{
    if (end <= viewport.start() || viewport.end() <= start || end <= start)
    {
        return;
    }
    const int x1 = time_x(start, viewport, metrics);
    const int x2 = time_x(end, viewport, metrics);
    display_list.add(Rectangle{x1, y, std::max(1, x2 - x1), height, style, std::move(id)});
}

Ticks phase_ticks(const std::optional<Duration> &phase)
{
    return phase ? phase->ticks() : 0;
}

template <typename Bounds>
bool contains(const Bounds &bounds, Point point, int tolerance)
{
    const double x = static_cast<double>(point.x) - bounds.x;
    const double y = static_cast<double>(point.y) - bounds.y;
    return -tolerance <= x && x < static_cast<double>(bounds.width) + tolerance && -tolerance <= y &&
        y < static_cast<double>(bounds.height) + tolerance;
}

bool contains(const Polyline &polyline, Point point, int tolerance)
{
    for (int index = 1; index < size_cast(polyline.points); ++index)
    {
        const Point &start = polyline.points[index - 1];
        const Point &end = polyline.points[index];
        const double dx = static_cast<double>(end.x) - start.x;
        const double dy = static_cast<double>(end.y) - start.y;
        const double px = static_cast<double>(point.x) - start.x;
        const double py = static_cast<double>(point.y) - start.y;
        const double length_squared = dx * dx + dy * dy;
        const double fraction =
            length_squared == 0.0 ? 0.0 : std::clamp((px * dx + py * dy) / length_squared, 0.0, 1.0);
        const double distance_x = px - fraction * dx;
        const double distance_y = py - fraction * dy;
        if (distance_x * distance_x + distance_y * distance_y <= static_cast<double>(tolerance) * tolerance)
        {
            return true;
        }
    }
    return false;
}

std::pair<double, double> curve_range(const Curve &curve)
{
    const auto [sample_minimum, sample_maximum] = std::minmax_element(curve.samples().begin(), curve.samples().end(),
        [](const CurveSample &lhs, const CurveSample &rhs) { return lhs.value() < rhs.value(); });
    return {curve.minimum().value_or(sample_minimum->value()), curve.maximum().value_or(sample_maximum->value())};
}

void add_curve(DisplayList &display_list, const Curve &curve, int y, int height, const Viewport &viewport,
    const LayoutMetrics &metrics, const std::optional<FrameGrid> &frame_grid, const std::string &lane_id)
{
    const std::vector<CurveSample> samples = frame_grid ? curve.sample(*frame_grid) : curve.samples();
    const auto [minimum, maximum] = curve_range(curve);
    std::vector<Point> points{};
    for (const CurveSample &sample : samples)
    {
        if (sample.time() < curve.start() || curve.end() < sample.time() || sample.time() < viewport.start() ||
            viewport.end() < sample.time())
        {
            continue;
        }
        const double normalized = maximum == minimum ? 0.5 : (sample.value() - minimum) / (maximum - minimum);
        const int point_y = y + height - 1 - static_cast<int>(std::lround(normalized * (height - 1)));
        points.push_back(Point{time_x(sample.time(), viewport, metrics), point_y});
    }
    if (size_cast(points) >= 2)
    {
        display_list.add(Polyline{std::move(points), StyleRole::CURVE, DisplayId{lane_id, curve.id()}});
    }
}

int keyframe_y(double value, double minimum, double maximum, int y, int height)
{
    const double normalized = maximum == minimum ? 0.5 : (value - minimum) / (maximum - minimum);
    return y + height - 1 - static_cast<int>(std::lround(normalized * (height - 1)));
}

void add_keyframes(DisplayList &display_list, const Lane &lane, int y, int height, const Viewport &viewport,
    const LayoutMetrics &metrics)
{
    std::vector<std::reference_wrapper<const Keyframe>> keyframes{};
    for (const Item &item : lane.items())
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
        [](const std::reference_wrapper<const Keyframe> &lhs, const std::reference_wrapper<const Keyframe> &rhs)
        { return lhs.get().time() < rhs.get().time(); });
    const auto [minimum, maximum] = std::minmax_element(keyframes.begin(), keyframes.end(),
        [](const std::reference_wrapper<const Keyframe> &lhs, const std::reference_wrapper<const Keyframe> &rhs)
        { return lhs.get().value() < rhs.get().value(); });
    const double minimum_value = minimum->get().value();
    const double maximum_value = maximum->get().value();

    for (int index = 1; index < size_cast(keyframes); ++index)
    {
        const Keyframe &left = keyframes[index - 1].get();
        const Keyframe &right = keyframes[index].get();
        if (right.time() < viewport.start() || viewport.end() < left.time())
        {
            continue;
        }
        const Point left_point{
            time_x(left.time(), viewport, metrics), keyframe_y(left.value(), minimum_value, maximum_value, y, height)};
        const Point right_point{time_x(right.time(), viewport, metrics),
            keyframe_y(right.value(), minimum_value, maximum_value, y, height)};
        std::vector<Point> points{left_point};
        if (left.interpolation() == KeyframeInterpolation::HOLD)
        {
            points.push_back(Point{right_point.x, left_point.y});
        }
        points.push_back(right_point);
        display_list.add(Polyline{std::move(points), StyleRole::KEYFRAME_SEGMENT, DisplayId{lane.id(), left.id()}});
    }

    for (const std::reference_wrapper<const Keyframe> &reference : keyframes)
    {
        const Keyframe &keyframe = reference.get();
        if (keyframe.time() < viewport.start() || viewport.end() < keyframe.time())
        {
            continue;
        }
        const int x = time_x(keyframe.time(), viewport, metrics);
        const int point_y = keyframe_y(keyframe.value(), minimum_value, maximum_value, y, height);
        const int marker_width = std::min(5, viewport.width() - metrics.lane_label_width());
        const int marker_height = std::min(5, height);
        const int marker_x =
            std::clamp(x - marker_width / 2, metrics.lane_label_width(), viewport.width() - marker_width);
        const int marker_y = std::clamp(point_y - marker_height / 2, y, y + height - marker_height);
        display_list.add(Marker{marker_x, marker_y, marker_width, marker_height, StyleRole::KEYFRAME_MARKER,
            DisplayId{lane.id(), keyframe.id()}});
    }
}

} // namespace

Viewport::Viewport(int width, int height, Time start, Time end) :
    Viewport(width, height, start, end, 0)
{
}

Viewport::Viewport(int width, int height, Time start, Time end, int first_lane) :
    m_width(width),
    m_height(height),
    m_start(start),
    m_end(end),
    m_first_lane(first_lane)
{
    if (m_width <= 0 || m_height <= 0 || m_end <= m_start || m_first_lane < 0)
    {
        throw std::invalid_argument(
            "timeline viewport requires positive dimensions and duration, and a nonnegative lane offset");
    }
}

FrameRange::FrameRange(Ticks first, Ticks last) :
    m_first(first),
    m_last(last)
{
    if (m_first < 0 || m_last < m_first)
    {
        throw std::invalid_argument("timeline frame range is invalid");
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

Navigation::Navigation(Time content_start, Time content_end, int lane_count) :
    m_content_start(content_start),
    m_content_end(content_end),
    m_start(content_start),
    m_end(content_end),
    m_lane_count(lane_count)
{
    if (m_content_end <= m_content_start || m_lane_count < 0)
    {
        throw std::invalid_argument("timeline navigation requires a positive extent and lane count");
    }
}

Viewport Navigation::viewport(int width, int height) const
{
    return Viewport(width, height, m_start, m_end, m_first_lane);
}

double Navigation::zoom_scale() const
{
    const double content_duration = static_cast<double>(m_content_end.ticks() - m_content_start.ticks());
    const double visible_duration = static_cast<double>(m_end.ticks() - m_start.ticks());
    return content_duration / visible_duration;
}

Duration Navigation::horizontal_offset() const
{
    return m_start - m_content_start;
}

double Navigation::horizontal_fraction() const
{
    const Ticks visible_duration = m_end.ticks() - m_start.ticks();
    const Ticks maximum_offset = m_content_end.ticks() - m_content_start.ticks() - visible_duration;
    if (maximum_offset == 0)
    {
        return 0.0;
    }
    return static_cast<double>(horizontal_offset().ticks()) / static_cast<double>(maximum_offset);
}

void Navigation::fit()
{
    m_start = m_content_start;
    m_end = m_content_end;
    m_first_lane = 0;
}

void Navigation::zoom_by(double factor, Time anchor)
{
    if (!std::isfinite(factor) || factor <= 0.0)
    {
        throw std::invalid_argument("timeline zoom factor must be finite and positive");
    }

    const Ticks content_duration = m_content_end.ticks() - m_content_start.ticks();
    const Ticks visible_duration = m_end.ticks() - m_start.ticks();
    const Ticks next_duration = std::clamp(
        static_cast<Ticks>(std::llround(static_cast<double>(visible_duration) / factor)), Ticks{1}, content_duration);
    const Ticks anchor_ticks = std::clamp(anchor.ticks(), m_start.ticks(), m_end.ticks());
    const double anchor_fraction =
        static_cast<double>(anchor_ticks - m_start.ticks()) / static_cast<double>(visible_duration);
    const Ticks anchored_start = anchor_ticks - static_cast<Ticks>(std::llround(anchor_fraction * next_duration));
    const Ticks maximum_start = m_content_end.ticks() - next_duration;
    const Ticks next_start = std::clamp(anchored_start, m_content_start.ticks(), maximum_start);
    m_start = Time::from_ticks(next_start);
    m_end = Time::from_ticks(next_start + next_duration);
}

void Navigation::scroll_to(Time start)
{
    const Ticks visible_duration = m_end.ticks() - m_start.ticks();
    const Ticks maximum_start = m_content_end.ticks() - visible_duration;
    const Ticks next_start = std::clamp(start.ticks(), m_content_start.ticks(), maximum_start);
    m_start = Time::from_ticks(next_start);
    m_end = Time::from_ticks(next_start + visible_duration);
}

void Navigation::reveal(Time time)
{
    const Ticks ticks = std::clamp(time.ticks(), m_content_start.ticks(), m_content_end.ticks());
    if (ticks < m_start.ticks())
    {
        scroll_to(Time::from_ticks(ticks));
    }
    else if (m_end.ticks() <= ticks)
    {
        scroll_to(Time::from_ticks(ticks - (m_end.ticks() - m_start.ticks()) + 1));
    }
}

void Navigation::scroll_to_fraction(double fraction)
{
    if (!std::isfinite(fraction))
    {
        throw std::invalid_argument("timeline scroll fraction must be finite");
    }

    const Ticks visible_duration = m_end.ticks() - m_start.ticks();
    const Ticks maximum_offset = m_content_end.ticks() - m_content_start.ticks() - visible_duration;
    const auto offset = static_cast<Ticks>(std::llround(std::clamp(fraction, 0.0, 1.0) * maximum_offset));
    scroll_to(Time::from_ticks(m_content_start.ticks() + offset));
}

void Navigation::scroll_to_lane(int first_lane, int visible_lanes)
{
    if (visible_lanes <= 0)
    {
        throw std::invalid_argument("timeline navigation requires a visible lane count");
    }
    m_first_lane = std::clamp(first_lane, 0, std::max(0, m_lane_count - visible_lanes));
}

Layout::Layout(const Document &document, Viewport viewport, LayoutMetrics metrics) :
    Layout(document, viewport, metrics, Interaction(document))
{
}

Layout::Layout(const Document &document, Viewport viewport, LayoutMetrics metrics, const Interaction &interaction) :
    m_width(viewport.width()),
    m_height(viewport.height()),
    m_content_left(metrics.lane_label_width())
{
    if (metrics.lane_label_width() >= viewport.width())
    {
        throw std::invalid_argument("timeline lane labels leave no content width");
    }

    m_display_list.add(Line{metrics.lane_label_width(), metrics.ruler_height() - 1, viewport.width() - 1,
        metrics.ruler_height() - 1, StyleRole::RULER, DisplayId{"", "ruler"}});
    m_display_list.add(Text{4, 4, "Time", StyleRole::RULER_LABEL, DisplayId{"", "ruler"}});

    for (int lane_index = viewport.first_lane(); lane_index < document.lane_count(); ++lane_index)
    {
        const std::optional<int> y = lane_y(lane_index, viewport, metrics);
        if (!y)
        {
            break;
        }
        const Lane &lane = document.lanes()[lane_index];
        const int row_height = metrics.lane_height();
        m_display_list.add(Rectangle{metrics.lane_label_width(), *y, viewport.width() - metrics.lane_label_width(),
            row_height, StyleRole::LANE_BACKGROUND, DisplayId{lane.id(), ""}});
        m_display_list.add(
            Text{4, *y + metrics.item_padding(), lane.label(), StyleRole::LANE_LABEL, DisplayId{lane.id(), ""}});

        const int item_y = *y + metrics.item_padding();
        const int item_height = std::max(1, row_height - metrics.item_padding() * 2);
        add_keyframes(m_display_list, lane, item_y, item_height, viewport, metrics);
        for (const Item &item : lane.items())
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
                        const int x = time_x(value.time(), viewport, metrics);
                        const int marker_width = std::min(2, viewport.width() - metrics.lane_label_width());
                        const int marker_x = std::clamp(
                            x - marker_width / 2, metrics.lane_label_width(), viewport.width() - marker_width);
                        m_display_list.add(Marker{marker_x, item_y, marker_width, item_height,
                            StyleRole::INSTANT_MARKER, DisplayId{lane.id(), value.id()}});
                    }
                    else if constexpr (std::is_same_v<Value, Interval>)
                    {
                        add_span(m_display_list, value.start(), value.end(), StyleRole::INTERVAL_SPAN,
                            DisplayId{lane.id(), value.id()}, item_y, item_height, viewport, metrics);
                    }
                    else if constexpr (std::is_same_v<Value, Envelope>)
                    {
                        Time phase_start = value.start();
                        Time phase_end = phase_start + Duration::from_ticks(phase_ticks(value.attack()));
                        add_span(m_display_list, phase_start, phase_end, StyleRole::ENVELOPE_ATTACK,
                            DisplayId{lane.id(), value.id()}, item_y, item_height, viewport, metrics);
                        phase_start = phase_end;
                        phase_end = phase_start + Duration::from_ticks(phase_ticks(value.sustain()));
                        add_span(m_display_list, phase_start, phase_end, StyleRole::ENVELOPE_SUSTAIN,
                            DisplayId{lane.id(), value.id()}, item_y, item_height, viewport, metrics);
                        phase_start = phase_end;
                        phase_end = phase_start + Duration::from_ticks(phase_ticks(value.decay()));
                        add_span(m_display_list, phase_start, phase_end, StyleRole::ENVELOPE_DECAY,
                            DisplayId{lane.id(), value.id()}, item_y, item_height, viewport, metrics);
                    }
                    else if constexpr (std::is_same_v<Value, Curve>)
                    {
                        add_curve(m_display_list, value, item_y, item_height, viewport, metrics, document.frame_grid(),
                            lane.id());
                    }
                },
                item);
        }
    }

    m_hit_regions.emplace_back(
        Rectangle{0, 0, viewport.width(), metrics.ruler_height(), StyleRole::RULER, DisplayId{"", "ruler"}});
    for (const Primitive &primitive : m_display_list.primitives())
    {
        std::visit(
            [&](const auto &value)
            {
                using Value = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, Rectangle>)
                {
                    if (value.style == StyleRole::LANE_BACKGROUND)
                    {
                        m_hit_regions.emplace_back(Rectangle{
                            0, value.y, metrics.lane_label_width(), value.height, StyleRole::LANE_LABEL, value.id});
                    }
                    m_hit_regions.emplace_back(value);
                }
                else if constexpr (std::is_same_v<Value, Marker> || std::is_same_v<Value, Polyline>)
                {
                    m_hit_regions.emplace_back(value);
                }
            },
            primitive);
    }

    DisplayList decorated{};
    const auto add_range = [&](int y, int height)
    {
        if (interaction.selected_range())
        {
            const TimeRange &range = *interaction.selected_range();
            const Time end = document.frame_grid() && document.frame_grid()->frame_count() > 0
                ? range.end() + document.frame_grid()->frame_duration()
                : range.end();
            add_span(decorated, range.start(), end, StyleRole::SELECTED_RANGE, DisplayId{"", ""}, y, height, viewport,
                metrics);
        }
    };
    add_range(0, metrics.ruler_height());
    for (const Primitive &primitive : m_display_list.primitives())
    {
        std::visit(
            [&](auto value)
            {
                using Value = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, Rectangle>)
                {
                    if (value.style == StyleRole::LANE_BACKGROUND)
                    {
                        if (interaction.selected_lane() && *interaction.selected_lane() == value.id.lane_id)
                        {
                            decorated.add(Rectangle{0, value.y, metrics.lane_label_width(), value.height,
                                StyleRole::SELECTED_LANE, value.id});
                            value.style = StyleRole::SELECTED_LANE;
                        }
                        decorated.add(value);
                        add_range(value.y, value.height);
                        return;
                    }
                }
                if (!value.id.item_id.empty() && interaction.is_selected(value.id))
                {
                    value.style = StyleRole::SELECTED_ITEM;
                }
                decorated.add(std::move(value));
            },
            primitive);
    }
    if (interaction.playhead() && viewport.start() <= *interaction.playhead() &&
        *interaction.playhead() <= viewport.end())
    {
        const int x = std::min(viewport.width() - 1, time_x(*interaction.playhead(), viewport, metrics));
        const DisplayId id{"", "playhead"};
        decorated.add(Line{x, 0, x, viewport.height() - 1, StyleRole::PLAYHEAD, id});
        // Restrict the playhead hit to the ruler so it cannot hide item hits.
        m_hit_regions.emplace_back(Marker{x, 0, 1, metrics.ruler_height(), StyleRole::PLAYHEAD, id});
    }
    m_display_list = std::move(decorated);
}

std::optional<HitResult> Layout::hit_test(Point point, int tolerance) const
{
    if (tolerance < 0)
    {
        throw std::invalid_argument("timeline hit tolerance cannot be negative");
    }
    if (point.x < 0 || m_width <= point.x || point.y < 0 || m_height <= point.y)
    {
        return std::nullopt;
    }
    for (std::vector<HitRegion>::const_reverse_iterator region = m_hit_regions.rbegin(); region != m_hit_regions.rend();
        ++region)
    {
        const std::optional<HitResult> hit = std::visit(
            [&](const auto &value) -> std::optional<HitResult>
            {
                using Value = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, Marker> || std::is_same_v<Value, Polyline>)
                {
                    if (point.x < m_content_left)
                    {
                        return std::nullopt;
                    }
                }
                const int hit_tolerance = std::is_same_v<Value, Rectangle> ? 0 : tolerance;
                return contains(value, point, hit_tolerance)
                    ? std::optional<HitResult>{HitResult{value.style, value.id}}
                    : std::nullopt;
            },
            *region);
        if (hit)
        {
            return hit;
        }
    }
    return std::nullopt;
}

Time time_at_x(int x, const Viewport &viewport, const LayoutMetrics &metrics)
{
    if (metrics.lane_label_width() >= viewport.width())
    {
        throw std::invalid_argument("timeline lane labels leave no content width");
    }

    const int left = metrics.lane_label_width();
    const int position = std::clamp(x, left, viewport.width()) - left;
    const int width = viewport.width() - left;
    const Ticks duration = viewport.end().ticks() - viewport.start().ticks();
    const auto elapsed = static_cast<Ticks>(std::llround(static_cast<double>(position) * duration / width));
    return Time::from_ticks(viewport.start().ticks() + elapsed);
}

std::optional<FrameRange> visible_frame_range(const FrameGrid &grid, const Viewport &viewport)
{
    if (grid.frame_count() == 0 || viewport.end() < grid.offset() || grid.end_time() < viewport.start())
    {
        return std::nullopt;
    }

    const auto start = Time::from_ticks(std::max(viewport.start().ticks(), grid.offset().ticks()));
    const auto end = Time::from_ticks(std::min(viewport.end().ticks(), grid.end_time().ticks()));
    return FrameRange(*grid.frame_at_or_before(start), *grid.frame_at_or_before(end));
}

int frame_x(Ticks frame, const FrameGrid &grid, const Viewport &viewport, const LayoutMetrics &metrics)
{
    return time_x(grid.frame_start(frame), viewport, metrics);
}

std::optional<Ticks> frame_at_x(int x, const FrameGrid &grid, const Viewport &viewport, const LayoutMetrics &metrics)
{
    return grid.nearest_frame(time_at_x(x, viewport, metrics));
}

int visible_lane_count(const Viewport &viewport, const LayoutMetrics &metrics)
{
    return std::max(0, viewport.height() - metrics.ruler_height()) / metrics.lane_height();
}

std::optional<int> lane_y(int lane, const Viewport &viewport, const LayoutMetrics &metrics)
{
    const int relative_lane = lane - viewport.first_lane();
    if (relative_lane < 0 || visible_lane_count(viewport, metrics) <= relative_lane)
    {
        return std::nullopt;
    }
    return metrics.ruler_height() + relative_lane * metrics.lane_height();
}

std::optional<int> lane_at_y(int y, int lane_count, const Viewport &viewport, const LayoutMetrics &metrics)
{
    if (lane_count < 0)
    {
        throw std::invalid_argument("timeline lane count cannot be negative");
    }
    if (y < metrics.ruler_height())
    {
        return std::nullopt;
    }

    const int relative_lane = (y - metrics.ruler_height()) / metrics.lane_height();
    if (relative_lane < 0 || visible_lane_count(viewport, metrics) <= relative_lane)
    {
        return std::nullopt;
    }
    const int lane = viewport.first_lane() + relative_lane;
    return lane < lane_count ? std::optional<int>{lane} : std::nullopt;
}

} // namespace timeline
