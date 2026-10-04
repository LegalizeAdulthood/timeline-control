<!-- Copyright (c) 2026 Richard Thomson -->

# Qt Timeline Adapter

`timeline-qt` presents a
[`timeline-core`](../timeline/ReadMe.md) document as a Qt widget. It
translates Qt input, scrollbars, sizing, font metrics, focus, and palette
values into toolkit-neutral core operations, then renders the resulting
display list with `QPainter`.

The adapter contains no ParAnimator or par-beatdown semantics. Applications
construct or import a `timeline::Document` elsewhere and give ownership of
that document to `QTimelineWidget`.

## Data Flow

```text
timeline::Document
        |
        v
 QTimelineWidget owns
        |
        +-- Qt events ----------> Interaction + Navigation
        |                                |
        +-- viewport + font metrics -----+
                                         v
                                      Layout
                                         |
                                         v
                                    DisplayList
                                         |
                                         v
                                      QPainter

Interaction or hover -> FrameInspection -> inspection_changed -> host
```

The widget owns the display session and delegates timeline behavior to the
core. The renderer interprets semantic display-list roles but does not
inspect source tracks or application data.

## Public API

| Symbol | Responsibility |
| --- | --- |
| `QTimelineWidget` | Document-owning timeline scroll-area widget. |
| `timeline_qt::style_color` | Resolve a core role through a `QPalette`. |
| `timeline_qt::draw_display_list` | Delegate primitives to `QPainter`. |

`QTimelineWidget` follows Qt's global class naming convention. Renderer
functions are in the `timeline_qt` namespace.

## Widget Ownership

`QTimelineWidget` derives from `QAbstractScrollArea`. It may be constructed
without a parent or with a parent reference. The widget enables strong
focus, viewport mouse tracking, and persistent horizontal and vertical
scrollbars.

`set_document` takes a `timeline::Document` by value and moves it into the
widget. Replacing the document creates fresh core state:

- `timeline::Interaction` for playhead and selection.
- `timeline::Navigation` for zoom and scrolling when content has extent.
- Initial frame inspection at frame zero when a frame grid exists.
- New hover, hit-test, layout, viewport, and metric state.
- A cleared drag and wheel-accumulation state.

The widget exposes optional-reference accessors for the document,
interaction, inspection, hit result, latest layout, viewport, and layout
metrics. These provide model details without copying them into Qt signals.

## Layout And Scrollbars

`rebuild` measures lane labels and font height through `QFontMetrics`. It
combines those values with fixed logical padding to create
`timeline::LayoutMetrics`. The scroll-area viewport dimensions and core
navigation produce a `timeline::Viewport`.

The widget passes the document, viewport, metrics, and interaction state to
`timeline::Layout`. The resulting display list supplies drawing operations,
and the same layout supplies hit-test geometry.

Horizontal scroll position represents the visible fraction of the complete
time extent. Its page step represents the current zoom. Vertical scrollbar
values map directly to the first visible lane and number of visible rows.

Rebuild uses a guard against recursive rebuilding and blocks scrollbar
signals while synchronizing their ranges and values. User scrollbar changes
are translated back into core `Navigation`, followed by another rebuild.

Resize, font, palette, and Qt style changes rebuild presentation state
without replacing the document or interaction state. Very small viewports
clear layout and disable scrolling until usable geometry returns.

## Input And Navigation

The adapter translates Qt events into core operations:

- A click selects the topmost hit; Ctrl adds to the selection.
- Dragging in the timeline begins and extends an exact time range.
- Left and Right move the playhead by frames.
- Shift with Left or Right extends the selected frame range.
- Escape clears selection.
- Ctrl with the mouse wheel zooms around the pointer time.
- Shift-wheel or a horizontal wheel scrolls through time.
- An unmodified vertical wheel moves between lanes.

Wheel-angle remainders are accumulated for conventional wheel steps,
while pixel-delta events support high-resolution input. Moving the
playhead reveals it without changing the current zoom.

Losing focus or mouse capture ends an active range drag. Leaving the
viewport clears hover and hit state. Focus transitions repaint selection
with the appropriate active or inactive style.

The `zoom_in`, `zoom_out`, `fit_view`, and `clear_selection` methods expose
the same operations to host actions and menus.

## Inspection Signal

`inspection_changed` is emitted when host-visible inspection state changes.
This includes document replacement, selection and playhead changes, hover
frame or hit changes, and presentation changes that may alter hit geometry.

Connected slots read `inspection`, `hit_result`, and `interaction` from the
widget. Detailed core data remains in typed accessors rather than being
duplicated in signal parameters.

## Painting

`paintEvent` creates a `QPainter` for the scroll-area viewport, fills the
Qt base color, applies the widget font and viewport clip, and enables
antialiasing. It then delegates the current core display list to
`timeline_qt::draw_display_list`.

When no layout exists, the widget draws either `No timeline loaded.` or
`No timeline content.` using the current palette.

`style_color` maps `timeline::StyleRole` values through `QPalette` and
light or dark background variants. Selection uses the Qt highlight color
while focused and a readable foreground blend while inactive.

`draw_display_list` visits primitives in their original paint order:

| Core Primitive | Qt Operation |
| --- | --- |
| `Line` | `QPainter::drawLine` |
| `Rectangle` and `Marker` | `QPainter::fillRect` |
| `Text` | `QPainter::drawText` |
| `Polyline` | `QPainter::drawPolyline` |
| `Swatch` | `QPainter::fillRect` with source RGB |

Layout coordinates are already widget-local logical coordinates, so the
renderer does not recompute geometry. Lane labels are clipped to the label
column, and source-color swatches retain their authored RGB values.

## Snapshots

`snapshot` returns `timeline::render_snapshot` output for the current
display list. If no layout exists, it returns the snapshot of an empty
display list. The output records toolkit-neutral geometry and semantic
roles, not Qt pixels or host-side inspection content.

## Build And Validation

The optional `qt` vcpkg feature selects Qt Widgets and the Qt test library.
Linux additionally requires fontconfig and XCB support. CMake discovery,
the adapter, and its tests are gated on `TIMELINE_CONTROL_WITH_QT`;
disabled builds add no Qt library targets and require no Qt packages.

The build deploys Qt's `minimal` platform plugin beside test executables.
The smoke check creates and destroys a Qt Widgets application and renders
a widget into an image with the headless `minimal` platform.

Widget tests cover painting, native input, document ownership and
replacement, and output shared with the core. Linux CI installs the XCB
prerequisites in `.github/workflows/qt-requirements.txt` together with
the shared wxWidgets and SDL3 prerequisites.

## Header Guide

| Header | Main Responsibility |
| --- | --- |
| `QTimelineWidget.h` | Widget ownership, commands, and signals. |
| `Renderer.h` | Qt palette mapping and display-list delegation. |

Consumers link the `timeline-qt` CMake target and include
`<qtTimeline/QTimelineWidget.h>`. The target enables Qt `AUTOMOC` and
publicly links `timeline-core` and `Qt6::Widgets`.
