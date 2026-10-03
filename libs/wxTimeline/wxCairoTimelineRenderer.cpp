// Copyright (c) 2026 Richard Thomson

#include <wxTimeline/wxCairoTimelineRenderer.h>

#include <algorithm>
#include <cairo.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <timeline/size_cast.h>
#include <wx/bitmap.h>

namespace
{

/// Exclusive Cairo resource ownership, including error-valued native handles.
using Surface = std::unique_ptr<cairo_surface_t, decltype(&cairo_surface_destroy)>;
/// Exclusive Cairo drawing context lifetime.
using Context = std::unique_ptr<cairo_t, decltype(&cairo_destroy)>;

wxRect curve_bounds(const timeline::Polyline &curve, wxPoint origin, int margin, wxRect clip)
{
    if (curve.points.empty())
    {
        return {};
    }
    int left = curve.points.front().x;
    int right = left;
    int top = curve.points.front().y;
    int bottom = top;
    for (const timeline::Point &point : curve.points)
    {
        left = std::min(left, point.x);
        right = std::max(right, point.x);
        top = std::min(top, point.y);
        bottom = std::max(bottom, point.y);
    }
    wxRect bounds(origin.x + left - margin, origin.y + top - margin, right - left + 2 * margin + 1,
        bottom - top + 2 * margin + 1);
    return bounds.Intersect(clip);
}

} // namespace

wxImage render_cairo_curve(const timeline::Polyline &curve, wxSize size, wxPoint origin, wxRect clip,
    const wxColour &colour, int stroke_width, double device_scale)
{
    const double width = std::ceil(size.x * device_scale);
    const double height = std::ceil(size.y * device_scale);
    if (size.x <= 0 || size.y <= 0 || !std::isfinite(device_scale) || device_scale <= 0.0 ||
        width > std::numeric_limits<int>::max() || height > std::numeric_limits<int>::max())
    {
        return {};
    }
    Surface surface(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, static_cast<int>(width), static_cast<int>(height)),
        &cairo_surface_destroy);
    if (cairo_surface_status(surface.get()) != CAIRO_STATUS_SUCCESS)
    {
        return {};
    }
    Context context(cairo_create(surface.get()), &cairo_destroy);
    cairo_scale(context.get(), device_scale, device_scale);
    cairo_rectangle(context.get(), clip.x, clip.y, clip.width, clip.height);
    cairo_clip(context.get());
    cairo_set_antialias(context.get(), CAIRO_ANTIALIAS_DEFAULT);
    cairo_set_line_width(context.get(), 2.0 * std::max(1, stroke_width));
    cairo_set_line_join(context.get(), CAIRO_LINE_JOIN_ROUND);
    cairo_set_source_rgb(context.get(), colour.Red() / 255.0, colour.Green() / 255.0, colour.Blue() / 255.0);
    if (timeline::size_cast(curve.points) >= 2)
    {
        cairo_move_to(context.get(), origin.x + curve.points.front().x, origin.y + curve.points.front().y);
        for (int index = 1; index < timeline::size_cast(curve.points); ++index)
        {
            const timeline::Point &point = curve.points[index];
            cairo_line_to(context.get(), origin.x + point.x, origin.y + point.y);
        }
        cairo_stroke(context.get());
    }
    if (cairo_status(context.get()) != CAIRO_STATUS_SUCCESS)
    {
        return {};
    }
    cairo_surface_flush(surface.get());
    wxImage image(static_cast<int>(width), static_cast<int>(height));
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

void draw_cairo_timeline_display_list(wxDC &dc, const timeline::DisplayList &display_list, wxPoint origin,
    const wxTimelinePalette &palette, int stroke_width, bool focused, wxSize size, double device_scale)
{
    wxRect clip;
    dc.GetClippingBox(clip);
    clip.Intersect(wxRect(wxPoint(0, 0), size));
    for (const timeline::Primitive &primitive : display_list.primitives())
    {
        if (const timeline::Polyline *curve = std::get_if<timeline::Polyline>(&primitive))
        {
            const wxRect bounds = curve_bounds(*curve, origin, std::max(1, stroke_width) + 1, clip);
            if (bounds.IsEmpty())
            {
                continue;
            }
            const wxColour colour = timeline_style_colour(curve->style, palette, focused);
            const wxImage image = render_cairo_curve(*curve, bounds.GetSize(), origin - bounds.GetPosition(),
                wxRect(wxPoint(0, 0), bounds.GetSize()), colour, stroke_width, device_scale);
            if (image.IsOk())
            {
                const wxBitmap bitmap(image, wxBITMAP_SCREEN_DEPTH, device_scale);
                if (bitmap.IsOk())
                {
                    dc.DrawBitmap(bitmap, bounds.GetPosition(), true);
                    continue;
                }
            }
        }
        draw_timeline_primitive(dc, primitive, origin, palette, stroke_width, focused);
    }
}
