// Copyright (c) 2026 Richard Thomson

#include <timeline/DisplayListRenderer.h>

#include <variant>

namespace timeline
{

namespace
{

void render_primitive(DisplayListRenderer &renderer, const StringTable &, const Line &line)
{
    renderer.draw_line(line);
}

void render_primitive(DisplayListRenderer &renderer, const StringTable &, const Rectangle &rectangle)
{
    renderer.fill_rectangle(rectangle);
}

void render_primitive(DisplayListRenderer &renderer, const StringTable &strings, const Text &text)
{
    renderer.draw_text(text, strings.lookup(text.value));
}

void render_primitive(DisplayListRenderer &renderer, const StringTable &, const Marker &marker)
{
    renderer.draw_marker(marker);
}

void render_primitive(DisplayListRenderer &renderer, const StringTable &, const Polyline &polyline)
{
    renderer.draw_polyline(polyline);
}

void render_primitive(DisplayListRenderer &renderer, const StringTable &, const Swatch &swatch)
{
    renderer.draw_swatch(swatch);
}

} // namespace

void render_display_list(DisplayListRenderer &renderer, const DisplayList &display_list)
{
    const StringTable &strings = display_list.strings();
    for (const Primitive &primitive : display_list.primitives())
    {
        std::visit([&renderer, &strings](const auto &value) { render_primitive(renderer, strings, value); }, primitive);
    }
}

} // namespace timeline
