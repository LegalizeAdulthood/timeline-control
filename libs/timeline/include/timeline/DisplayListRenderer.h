// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/DisplayList.h>

#include <string_view>

namespace timeline
{

/// Toolkit-neutral port for rendering display-list primitives.
///
/// Implementations translate semantic primitives into native drawing
/// operations while retaining responsibility for styling and geometry.
///
class DisplayListRenderer
{
public:
    virtual ~DisplayListRenderer() = default;

    virtual void draw_line(const Line &line) = 0;
    virtual void fill_rectangle(const Rectangle &rectangle) = 0;
    virtual void draw_text(const Text &text, std::string_view value) = 0;
    virtual void draw_marker(const Marker &marker) = 0;
    virtual void draw_polyline(const Polyline &polyline) = 0;
    virtual void draw_swatch(const Swatch &swatch) = 0;
};

/// Dispatches a display list in paint order and resolves its text values.
void render_display_list(DisplayListRenderer &renderer, const DisplayList &display_list);

} // namespace timeline
