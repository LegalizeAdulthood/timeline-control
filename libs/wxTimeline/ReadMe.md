<!-- Copyright (c) 2026 Richard Thomson -->

# wxWidgets Timeline Adapter

`timeline-wx` presents a
[`timeline-core`](../timeline/ReadMe.md) document as a native wxWidgets
control. It translates wxWidgets input, sizing, fonts, colors, focus, and
scrollbars into toolkit-neutral core operations, then renders the resulting
core display list.

The adapter contains no ParAnimator or par-beatdown semantics. Applications
construct or import a `timeline::Document` elsewhere and give ownership of
that document to the control.

## Data Flow

```text
timeline::Document
        |
        v
 wxTimelineControl owns
        |
        +-- wx input ----------> Interaction + Navigation
        |                              |
        +-- client size + font --------+
                                       v
                                    Layout
                                       |
                                       v
                                  DisplayList
                                       |
                      +----------------+----------------+
                      v                                 v
              native wx renderer                Cairo renderer
                      |                                 |
                      +---------------> wxDC <----------+

Interaction or hover -> FrameInspection -> wx command event -> host
```

The control owns view state and delegates timeline behavior to the core.
Renderers interpret semantic display-list roles but do not inspect source
tracks or application data.

## Public Types

| Type | Responsibility |
| --- | --- |
| `wxTimelineControl` | Document-owning native timeline control. |
| `wxTimelinePalette` | Native colors used for semantic roles. |
| `wxCairoTimeline` | Control with selectable Cairo rendering. |

The public renderer functions operate on core display lists and wxWidgets
drawing types. These symbols are in the global namespace to follow
wxWidgets naming conventions.

## Control Ownership

`wxTimelineControl` derives from `wxPanel`. A newly constructed control has
no document and displays an empty-state message. `set_document` takes a
`timeline::Document` by value and moves it into the control.

Replacing the document also creates fresh core state:

- `timeline::Interaction` for playhead and selection.
- `timeline::Navigation` for zoom and scrolling when content has extent.
- Initial frame inspection at frame zero when a frame grid exists.
- New layout, viewport, hit-test, hover, and metric state.

The control therefore owns the complete display session for its document.
Callers may inspect that state through optional-reference accessors for the
document, interaction, frame inspection, and current hit result.

## Layout And Painting

During layout, the control measures lane labels and character height with
the current `wxDC`. It converts those measurements and DPI-scaled padding
into `timeline::LayoutMetrics`. Client dimensions and core navigation
produce a `timeline::Viewport`.

The control passes the document, viewport, metrics, and interaction state
to `timeline::Layout`. The resulting `timeline::DisplayList` contains all
ordered drawing operations. Hit testing uses the corresponding core layout
geometry.

Painting uses `wxAutoBufferedPaintDC`. The control clears its background,
rebuilds layout when necessary, delegates the display list through the
virtual `draw_display_list` hook, and draws a native focus rectangle.

Resize, DPI, font-dependent layout, and system-color changes invalidate
presentation state without replacing the document or interaction state.

## Input And Navigation

The adapter translates native events into core operations:

- A click selects the topmost hit; Ctrl adds to the selection.
- Dragging in the timeline begins and extends an exact time range.
- Left and Right move the playhead by frames.
- Shift with Left or Right extends the selected frame range.
- Escape clears selection and Tab follows native focus traversal.
- Ctrl with the mouse wheel zooms around the pointer time.
- Shift with the wheel scrolls horizontally.
- An unmodified wheel and the vertical scrollbar move between lanes.
- The horizontal scrollbar represents the visible fraction of time.

The `zoom_in`, `zoom_out`, `fit_view`, `clear_selection`, and
`step_playhead` methods expose the same operations to host menus and
commands.

## Inspection Event

`wxEVT_TIMELINE_INSPECTION_CHANGED` is a `wxCommandEvent` emitted when the
host-visible inspection state changes. This includes document replacement,
selection and playhead changes, and changes to the hovered frame or hit.

Event handlers read `inspection`, `hit_result`, and `interaction` from the
control. The event carries the control as its event object; detailed model
data stays in the typed accessors instead of being copied into the event.

## Snapshots

`snapshot` rebuilds pending layout using the control's current font and
returns `timeline::render_snapshot` output for the displayed primitives.
The result captures core geometry and semantic roles, not native pixels or
host-side inspection content.

## Native Renderer

`wxTimelinePalette` supplies the control background and foreground colors
plus the current system highlight color. `timeline_style_colour` maps each
`timeline::StyleRole` to those native colors and readable blends of them.
Applications use the inherited `SetBackgroundColour` and
`SetForegroundColour` widget methods. Selection follows the system theme
and focus state. `set_style_color` overrides any individual role with a
`wxColour`, while `style_color` returns the effective override or default.

The renderer API has three levels:

- `draw_timeline_display_list` draws the display list in paint order.
- `draw_timeline_primitive` draws one core primitive.

Lines, rectangles, text, markers, polylines, and source-color swatches map
directly to wxWidgets drawing primitives. The renderer applies an origin,
palette, stroke width, and focus state; it does not recompute layout.
Swatches retain their authored RGB values unless the application overrides
the `PALETTE` role.

## Cairo Renderer

Enabling `TIMELINE_CONTROL_WITH_CAIRO` adds the Cairo sources and
dependency to `timeline-wx`. It requires wxWidgets support.

`wxCairoTimeline` derives from `wxTimelineControl` and overrides only the
display-list rendering hook. `set_cairo_enabled` switches between Cairo and
the base native renderer while preserving the owned document, navigation,
interaction, inspection, and layout state.

The Cairo renderer creates a device-scaled image surface for the viewport,
clips drawing, and renders the same ordered core primitives with
antialiased geometry. Text uses coverage generated from the current native
wx font, so layout and glyph selection remain consistent with the
control's metrics.

The surface is converted from premultiplied Cairo pixels to a `wxImage`,
then presented as a scale-aware `wxBitmap`. If surface creation, rendering,
image conversion, or bitmap creation fails, drawing falls back to the
complete native wx display list.

The Cairo public functions are:

| Function | Responsibility |
| --- | --- |
| `render_cairo_curve` | Render one transparent antialiased polyline. |
| `render_cairo_display_list` | Render primitives into a `wxImage`. |
| `draw_cairo_timeline_display_list` | Present Cairo with wx fallback. |

## Validation And Platform Notes

Native theme mapping has headless adapter coverage. Actual multi-display
DPI transitions and OS-theme changes still require manual verification on
the target platforms.

Cairo rasterization remains anchored to the complete viewport during
partial repaints, keeping fractional-scale coverage stable. Surfaces and
native-font text masks are recreated for every draw from the current font,
palette, size, and content scale; no renderer cache retains presentation
resources.

Tests exercise 100%, 125%, 150%, and 200% scaling, repeated font and theme
changes, resizing, renderer switching, document replacement, and control
destruction. Non-Windows font behavior still requires manual verification.
Linux CI uses Xvfb and xauth for native control tests; image checks require
no GPU.

See the
[Cairo image-surface API](https://www.cairographics.org/manual/cairo-Image-Surfaces.html)
for the underlying pixel representation.

## Header Guide

| Header | Main Responsibility |
| --- | --- |
| `wxTimelineControl.h` | Control ownership, commands, and state access. |
| `wxTimelineRenderer.h` | Native colors and display-list rendering. |
| `wxCairoTimeline.h` | Cairo-selectable control subclass. |
| `wxCairoTimelineRenderer.h` | Cairo image rendering and presentation. |

Consumers link the `timeline-wx` CMake target and include
`<wxTimeline/wxTimelineControl.h>`. Cairo-enabled hosts may additionally
include `<wxTimeline/wxCairoTimeline.h>`.
