// Copyright (c) 2026 Richard Thomson

#include <wxTimeline/wxCairoTimelineRenderer.h>

#include <timeline/size_cast.h>

#include <cairo.h>
#include <wx/bitmap.h>
#include <wx/dcmemory.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string_view>
#include <type_traits>

namespace
{

/// Exclusive Cairo resource ownership, including error-valued native handles.
using Surface = std::unique_ptr<cairo_surface_t, decltype(&cairo_surface_destroy)>;
/// Exclusive Cairo drawing context lifetime.
using Context = std::unique_ptr<cairo_t, decltype(&cairo_destroy)>;

Surface image_surface(wxSize size, double device_scale)
{
    const double width = std::ceil(size.x * device_scale);
    const double height = std::ceil(size.y * device_scale);
    if (size.x <= 0 || size.y <= 0 || !std::isfinite(device_scale) || device_scale <= 0.0 ||
        width > std::numeric_limits<int>::max() || height > std::numeric_limits<int>::max())
    {
        return Surface(nullptr, &cairo_surface_destroy);
    }
    return Surface(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, static_cast<int>(width), static_cast<int>(height)),
        &cairo_surface_destroy);
}

Context drawing_context(const Surface &surface, wxRect clip, double device_scale)
{
    Context context(cairo_create(surface.get()), &cairo_destroy);
    cairo_scale(context.get(), device_scale, device_scale);
    cairo_rectangle(context.get(), clip.x, clip.y, clip.width, clip.height);
    cairo_clip(context.get());
    cairo_set_antialias(context.get(), CAIRO_ANTIALIAS_DEFAULT);
    cairo_set_line_join(context.get(), CAIRO_LINE_JOIN_ROUND);
    return context;
}

void draw_curve(const Context &context, const timeline::Polyline &curve, wxPoint origin, int stroke_width)
{
    if (timeline::size_cast(curve.points) < 2)
    {
        return;
    }
    cairo_set_line_width(context.get(), 2.0 * std::max(1, stroke_width));
    cairo_move_to(context.get(), origin.x + curve.points.front().x, origin.y + curve.points.front().y);
    for (int index = 1; index < timeline::size_cast(curve.points); ++index)
    {
        const timeline::Point &point = curve.points[index];
        cairo_line_to(context.get(), origin.x + point.x, origin.y + point.y);
    }
    cairo_stroke(context.get());
}

Surface text_mask(const wxString &text, const wxFont &font, double device_scale)
{
    wxFont scaled_font(font);
    scaled_font.SetFractionalPointSize(font.GetFractionalPointSize() * device_scale);
    wxBitmap measure_bitmap(1, 1, 24);
    wxMemoryDC dc(measure_bitmap);
    dc.SetFont(scaled_font);
    const wxSize extent = dc.GetTextExtent(text);
    dc.SelectObject(wxNullBitmap);
    if (extent.x <= 0 || extent.y <= 0 || extent.x > std::numeric_limits<int>::max() - 4 ||
        extent.y > std::numeric_limits<int>::max() - 4)
    {
        return Surface(nullptr, &cairo_surface_destroy);
    }
    // Native glyph shaping and coverage retain the wx font and logical metrics.
    wxBitmap bitmap(extent.x + 4, extent.y + 4, 24);
    if (!bitmap.IsOk())
    {
        return Surface(nullptr, &cairo_surface_destroy);
    }
    dc.SelectObject(bitmap);
    dc.SetFont(scaled_font);
    dc.SetBackground(*wxBLACK_BRUSH);
    dc.Clear();
    dc.SetTextForeground(*wxWHITE);
    dc.SetBackgroundMode(wxTRANSPARENT);
    dc.DrawText(text, 2, 2);
    dc.SelectObject(wxNullBitmap);
    const wxImage image = bitmap.ConvertToImage();
    if (!image.IsOk())
    {
        return Surface(nullptr, &cairo_surface_destroy);
    }
    Surface mask(
        cairo_image_surface_create(CAIRO_FORMAT_A8, image.GetWidth(), image.GetHeight()), &cairo_surface_destroy);
    if (cairo_surface_status(mask.get()) != CAIRO_STATUS_SUCCESS)
    {
        return Surface(nullptr, &cairo_surface_destroy);
    }
    cairo_surface_flush(mask.get());
    unsigned char *data = cairo_image_surface_get_data(mask.get());
    const int stride = cairo_image_surface_get_stride(mask.get());
    for (int y = 0; y < image.GetHeight(); ++y)
    {
        for (int x = 0; x < image.GetWidth(); ++x)
        {
            data[static_cast<std::size_t>(y) * stride + x] =
                static_cast<unsigned char>((image.GetRed(x, y) + image.GetGreen(x, y) + image.GetBlue(x, y) + 2) / 3);
        }
    }
    cairo_surface_mark_dirty(mask.get());
    cairo_surface_set_device_scale(mask.get(), device_scale, device_scale);
    return mask;
}

wxImage surface_image(const Surface &surface)
{
    cairo_surface_flush(surface.get());
    wxImage image(cairo_image_surface_get_width(surface.get()), cairo_image_surface_get_height(surface.get()));
    if (!image.IsOk())
    {
        return {};
    }
    image.InitAlpha();
    const unsigned char *data = cairo_image_surface_get_data(surface.get());
    const int stride = cairo_image_surface_get_stride(surface.get());
    // Cairo stores native-endian premultiplied ARGB; wx images use straight RGB.
    for (int y = 0; y < image.GetHeight(); ++y)
    {
        for (int x = 0; x < image.GetWidth(); ++x)
        {
            std::uint32_t pixel{};
            std::memcpy(&pixel, data + static_cast<std::size_t>(y) * stride + 4 * x, sizeof(pixel));
            const unsigned int alpha = pixel >> 24;
            const unsigned int red = (pixel >> 16) & 255;
            const unsigned int green = (pixel >> 8) & 255;
            const unsigned int blue = pixel & 255;
            image.SetRGB(x, y, static_cast<unsigned char>(alpha ? (red * 255 + alpha / 2) / alpha : 0),
                static_cast<unsigned char>(alpha ? (green * 255 + alpha / 2) / alpha : 0),
                static_cast<unsigned char>(alpha ? (blue * 255 + alpha / 2) / alpha : 0));
            image.SetAlpha(x, y, static_cast<unsigned char>(alpha));
        }
    }
    return image;
}

bool draw_primitive(const Context &context, const timeline::Primitive &primitive, wxPoint origin,
    const timeline::StringTable &strings, const wxTimelinePalette &palette, const wxTimelineStyleColors &style_colors,
    int stroke_width, bool focused, const wxFont &font, double device_scale)
{
    return std::visit(
        [&](const auto &value)
        {
            using Value = std::decay_t<decltype(value)>;
            const wxColour colour = timeline_style_colour(value.style, palette, style_colors, focused);
            const bool overridden = style_colors.find(value.style) != style_colors.end();
            cairo_set_source_rgb(context.get(), colour.Red() / 255.0, colour.Green() / 255.0, colour.Blue() / 255.0);
            if constexpr (std::is_same_v<Value, timeline::Line>)
            {
                cairo_set_line_width(context.get(), std::max(1, stroke_width));
                cairo_move_to(context.get(), origin.x + value.x1, origin.y + value.y1);
                cairo_line_to(context.get(), origin.x + value.x2, origin.y + value.y2);
                cairo_stroke(context.get());
            }
            else if constexpr (std::is_same_v<Value, timeline::Rectangle> || std::is_same_v<Value, timeline::Marker> ||
                std::is_same_v<Value, timeline::Swatch>)
            {
                if constexpr (std::is_same_v<Value, timeline::Swatch>)
                {
                    if (!overridden)
                    {
                        cairo_set_source_rgb(context.get(), value.color.red() / 255.0, value.color.green() / 255.0,
                            value.color.blue() / 255.0);
                    }
                }
                if (value.width > 0 && value.height > 0)
                {
                    cairo_rectangle(context.get(), origin.x + value.x, origin.y + value.y, value.width, value.height);
                    cairo_fill(context.get());
                }
            }
            else if constexpr (std::is_same_v<Value, timeline::Polyline>)
            {
                draw_curve(context, value, origin, stroke_width);
            }
            else
            {
                const std::string_view text = strings.lookup(value.value);
                if (!text.empty())
                {
                    const Surface mask = text_mask(wxString(text.data(), text.size()), font, device_scale);
                    if (!mask)
                    {
                        return false;
                    }
                    cairo_mask_surface(context.get(), mask.get(), origin.x + value.x - 2.0 / device_scale,
                        origin.y + value.y - 2.0 / device_scale);
                }
            }
            return cairo_status(context.get()) == CAIRO_STATUS_SUCCESS;
        },
        primitive);
}

} // namespace

wxImage render_cairo_curve(const timeline::Polyline &curve, wxSize size, wxPoint origin, wxRect clip,
    const wxColour &colour, int stroke_width, double device_scale)
{
    const Surface surface = image_surface(size, device_scale);
    if (!surface || cairo_surface_status(surface.get()) != CAIRO_STATUS_SUCCESS)
    {
        return {};
    }
    const Context context = drawing_context(surface, clip, device_scale);
    cairo_set_source_rgb(context.get(), colour.Red() / 255.0, colour.Green() / 255.0, colour.Blue() / 255.0);
    draw_curve(context, curve, origin, stroke_width);
    return cairo_status(context.get()) == CAIRO_STATUS_SUCCESS ? surface_image(surface) : wxImage{};
}

wxImage render_cairo_display_list(const timeline::DisplayList &display_list, wxSize size, wxPoint origin, wxRect clip,
    const wxTimelinePalette &palette, int stroke_width, bool focused, const wxFont &font, double device_scale)
{
    return render_cairo_display_list(
        display_list, size, origin, clip, palette, wxTimelineStyleColors{}, stroke_width, focused, font, device_scale);
}

wxImage render_cairo_display_list(const timeline::DisplayList &display_list, wxSize size, wxPoint origin, wxRect clip,
    const wxTimelinePalette &palette, const wxTimelineStyleColors &style_colors, int stroke_width, bool focused,
    const wxFont &font, double device_scale)
{
    const Surface surface = image_surface(size, device_scale);
    if (!surface || cairo_surface_status(surface.get()) != CAIRO_STATUS_SUCCESS)
    {
        return {};
    }
    const Context context = drawing_context(surface, clip, device_scale);
    for (const timeline::Primitive &primitive : display_list.primitives())
    {
        if (!draw_primitive(context, primitive, origin, display_list.strings(), palette, style_colors, stroke_width,
                focused, font, device_scale))
        {
            return {};
        }
    }
    return cairo_status(context.get()) == CAIRO_STATUS_SUCCESS ? surface_image(surface) : wxImage{};
}

void draw_cairo_timeline_display_list(wxDC &dc, const timeline::DisplayList &display_list, wxPoint origin,
    const wxTimelinePalette &palette, int stroke_width, bool focused, wxSize size, double device_scale)
{
    draw_cairo_timeline_display_list(
        dc, display_list, origin, palette, wxTimelineStyleColors{}, stroke_width, focused, size, device_scale);
}

void draw_cairo_timeline_display_list(wxDC &dc, const timeline::DisplayList &display_list, wxPoint origin,
    const wxTimelinePalette &palette, const wxTimelineStyleColors &style_colors, int stroke_width, bool focused,
    wxSize size, double device_scale)
{
    wxRect clip;
    dc.GetClippingBox(clip);
    clip.Intersect(wxRect(wxPoint(0, 0), size));
    if (clip.IsEmpty())
    {
        return;
    }
    // Keep rasterization anchored to the viewport, even for partial repaints.
    const wxImage image = render_cairo_display_list(display_list, size, origin, wxRect(wxPoint(0, 0), size), palette,
        style_colors, stroke_width, focused, dc.GetFont(), device_scale);
    if (image.IsOk())
    {
        const wxBitmap bitmap(image, wxBITMAP_SCREEN_DEPTH, device_scale);
        if (bitmap.IsOk())
        {
            dc.DrawBitmap(bitmap, wxPoint(0, 0), true);
            return;
        }
    }
    draw_timeline_display_list(dc, display_list, origin, palette, style_colors, stroke_width, focused);
}
