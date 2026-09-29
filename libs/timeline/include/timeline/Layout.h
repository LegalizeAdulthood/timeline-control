// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/DisplayList.h>
#include <timeline/Document.h>

namespace timeline
{

/// Visible timeline time range and pixel extent.
///
/// A viewport maps one nonempty exact time range into positive integral pixel
/// dimensions supplied by the host control.
///
class Viewport
{
public:
    Viewport(int width, int height, Time start, Time end);

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

private:
    int m_width;
    int m_height;
    Time m_start;
    Time m_end;
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

/// Computed toolkit-neutral rendering for one document and viewport.
///
/// Layout maps exact item times into stable row geometry and records the
/// corresponding semantic drawing operations in a display list.
///
class Layout
{
public:
    Layout(const Document &document, Viewport viewport, LayoutMetrics metrics);

    const DisplayList &display_list() const
    {
        return m_display_list;
    }

private:
    DisplayList m_display_list;
};

} // namespace timeline
