// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/DisplayList.h>
#include <timeline/Document.h>

#include <optional>

namespace timeline
{

class Interaction;

/// Visible timeline time range and pixel extent.
///
/// A viewport maps one nonempty exact time range into positive integral pixel
/// dimensions supplied by the host control.
///
class Viewport
{
public:
    Viewport(int width, int height, Time start, Time end);
    Viewport(int width, int height, Time start, Time end, int first_lane);

    int width() const
    {
        return m_width;
    }
    int height() const
    {
        return m_height;
    }
    Time start() const
    {
        return m_start;
    }
    Time end() const
    {
        return m_end;
    }
    int first_lane() const
    {
        return m_first_lane;
    }

private:
    int m_width;
    int m_height;
    Time m_start;
    Time m_end;
    int m_first_lane;
};

/// Inclusive frame-index range for visibility or selection.
///
/// The range contains ordered, nonnegative first and last frame indices.
///
class FrameRange
{
public:
    FrameRange(Ticks first, Ticks last);

    Ticks first() const
    {
        return m_first;
    }
    Ticks last() const
    {
        return m_last;
    }

private:
    Ticks m_first;
    Ticks m_last;
};

/// Host-provided geometry used to lay out timeline rows and items.
///
/// Layout metrics contain only toolkit-neutral dimensions. Native font and
/// color values remain responsibilities of the GUI adapter.
///
class LayoutMetrics
{
public:
    LayoutMetrics(int lane_label_width, int ruler_height, int lane_height, int item_padding);

    int lane_label_width() const
    {
        return m_lane_label_width;
    }
    int ruler_height() const
    {
        return m_ruler_height;
    }
    int lane_height() const
    {
        return m_lane_height;
    }
    int item_padding() const
    {
        return m_item_padding;
    }

private:
    int m_lane_label_width;
    int m_ruler_height;
    int m_lane_height;
    int m_item_padding;
};

/// Toolkit-neutral zoom and scroll state for one finite timeline extent.
///
/// Navigation stores exact visible time bounds and a lane offset. Hosts
/// translate native input into these operations and request a Viewport for
/// their current pixel dimensions.
///
class Navigation
{
public:
    Navigation(Time content_start, Time content_end, int lane_count);

    Viewport viewport(int width, int height) const;
    double zoom_scale() const;
    Duration horizontal_offset() const;
    double horizontal_fraction() const;
    void fit();
    void zoom_by(double factor, Time anchor);
    void scroll_to(Time start);
    void scroll_to_fraction(double fraction);
    void scroll_to_lane(int first_lane, int visible_lanes);

private:
    Time m_content_start;
    Time m_content_end;
    Time m_start;
    Time m_end;
    int m_lane_count;
    int m_first_lane{0};
};

/// Semantic role and stable source identity of the topmost hit region.
///
struct HitResult
{
    StyleRole style;
    DisplayId id;
};

inline bool operator==(const HitResult &lhs, const HitResult &rhs)
{
    return lhs.style == rhs.style && lhs.id.lane_id == rhs.id.lane_id && lhs.id.item_id == rhs.id.item_id;
}
inline bool operator!=(const HitResult &lhs, const HitResult &rhs)
{
    return !(lhs == rhs);
}

/// Toolkit-neutral hit geometry in display-list paint order.
using HitRegion = std::variant<Rectangle, Marker, Polyline>;

/// Computed toolkit-neutral rendering and hit geometry for a viewport.
///
/// Layout maps exact item times into stable row geometry and records the
/// corresponding semantic drawing operations and hit regions in paint order.
///
class Layout
{
public:
    Layout(const Document &document, Viewport viewport, LayoutMetrics metrics);
    Layout(const Document &document, Viewport viewport, LayoutMetrics metrics, const Interaction &interaction);

    const DisplayList &display_list() const
    {
        return m_display_list;
    }
    std::optional<HitResult> hit_test(Point point, int tolerance) const;

private:
    DisplayList m_display_list;
    std::vector<HitRegion> m_hit_regions;
    int m_width;
    int m_height;
    int m_content_left;
};

/// Maps a horizontal host position into the viewport's exact time range.
Time time_at_x(int x, const Viewport &viewport, const LayoutMetrics &metrics);

/// Returns the inclusive frame range intersecting a viewport.
std::optional<FrameRange> visible_frame_range(const FrameGrid &grid, const Viewport &viewport);

/// Maps a frame index to its horizontal host position.
int frame_x(Ticks frame, const FrameGrid &grid, const Viewport &viewport, const LayoutMetrics &metrics);

/// Maps a horizontal host position to its nearest frame index.
std::optional<Ticks> frame_at_x(int x, const FrameGrid &grid, const Viewport &viewport, const LayoutMetrics &metrics);

/// Returns the number of complete lane rows visible below the ruler.
int visible_lane_count(const Viewport &viewport, const LayoutMetrics &metrics);

/// Maps a visible document lane index to its row's top coordinate.
std::optional<int> lane_y(int lane, const Viewport &viewport, const LayoutMetrics &metrics);

/// Maps a host coordinate to a visible document lane index.
std::optional<int> lane_at_y(int y, int lane_count, const Viewport &viewport, const LayoutMetrics &metrics);

} // namespace timeline
