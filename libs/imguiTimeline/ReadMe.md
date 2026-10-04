<!-- Copyright (c) 2026 Richard Thomson -->

# Dear ImGui Timeline Adapter

`timeline-imgui` presents a
[`timeline-core`](../timeline/ReadMe.md) document as a Dear ImGui item. It
translates immediate-mode input, sizing, font metrics, focus, and theme
values into toolkit-neutral core operations, then delegates the resulting
display list to an `ImDrawList`.

The adapter contains no source-format semantics. Applications construct or
import a `timeline::Document` elsewhere and give ownership of that document
to a retained `timeline_imgui::Control`.

## Data Flow

```text
host-retained Control
        |
        +-- owns Document, Interaction, and Navigation
        |
        v
draw_timeline(id, control, size) each ImGui frame
        |
        +-- item size + font + style -> LayoutMetrics + Viewport
        +-- mouse + keyboard --------> Interaction + Navigation
        |                                      |
        +--------------------------------------+
                                               v
                                            Layout
                                               |
                                               v
                                          DisplayList
                                               |
                                               v
                                           ImDrawList

Control accessors -> inspection, hit, layout, viewport, and interaction
```

Dear ImGui drawing is immediate-mode, but the timeline session is retained.
The host keeps the `Control` alive between frames; the adapter keeps no
global timeline state.

## Public API

| Symbol | Responsibility |
| --- | --- |
| `Control` | Owned document and persistent view-only state. |
| `draw_timeline` | Submit one timeline item for the current frame. |
| `style_colour` | Map a core semantic role to an ImGui color. |
| `draw_display_list` | Delegate core primitives to an `ImDrawList`. |

All public symbols are in the `timeline_imgui` namespace.

## Host Responsibilities

The host owns the Dear ImGui context, frame lifecycle, fonts, platform
backend, and renderer backend. It calls `draw_timeline` inside an active
`ImGui::Begin` and `ImGui::End` pair on every frame where the item exists.

Each submitted timeline needs:

- A `Control` whose lifetime spans frames.
- A stable, unique string ID.
- An explicit size or the remaining-region overload.

Separate controls and IDs produce independent timeline items. Omitting an
item for a frame or clipping it out ends any active range drag.

## Control Ownership

`Control` may be default constructed without a document or constructed
with a `timeline::Document`. `set_document` takes a document by value and
moves it into the control.

Replacing the document resets all document-dependent state:

- `timeline::Interaction` is recreated for playhead and selection.
- `timeline::Navigation` is recreated when content has a finite extent.
- Frame inspection starts at frame zero when a frame grid exists.
- Hover, drag, layout, viewport, and metric state are cleared.

The control exposes optional-reference accessors for the owned document,
interaction, inspection, hit result, latest layout, viewport, and layout
metrics. Hosts poll these values after submitting the item; there is no
adapter-specific event or callback type.

## Per-Frame Submission

`draw_timeline` reserves the requested region with an `InvisibleButton`.
That item supplies hover, active, focus, navigation, and mouse ownership to
the adapter without introducing visible ImGui widgets around the timeline.

Each submission performs the following work:

1. Clamp the requested size to a positive region.
2. Push the caller's stable ID and submit the invisible item.
3. Recompute host-dependent layout from the current font and style.
4. Translate input into core interaction and navigation operations.
5. Recompute layout if input changed the view or interaction state.
6. Clip to the item rectangle and render the core display list.

When no document is loaded or the document has no usable content, the item
draws the corresponding empty-state text.

## Layout

The adapter derives row height, ruler height, padding, and a minimum label
column from the current ImGui font and `FramePadding`. It expands the label
column to fit document lane labels and caps it at half the item width.

The current item dimensions and core `Navigation` produce a
`timeline::Viewport`. The adapter clamps vertical scrolling to the number
of complete visible lanes, then constructs `timeline::Layout` from the
document, viewport, metrics, and interaction state.

Layout is regenerated on every visible submission, so font, style, size,
and host-window changes are reflected without native invalidation events.
The latest generated objects remain available through the control
accessors.

## Input And Navigation

The adapter translates ImGui input into core operations:

- A click selects the topmost hit; Ctrl adds to the selection.
- Dragging in the timeline begins and extends an exact time range.
- Left and Right move the playhead by frames.
- Shift with Left or Right extends the selected frame range.
- Escape clears selection.
- Ctrl with the mouse wheel zooms around the pointer time.
- Shift-wheel or a horizontal wheel scrolls through time.
- An unmodified vertical wheel moves between lanes.

The hovered item claims wheel and directional-key ownership so these inputs
operate on the timeline instead of its containing ImGui window. Moving the
playhead reveals it without changing the current zoom.

The `zoom_in`, `zoom_out`, `fit_view`, and `clear_selection` methods expose
the same operations to host menus and commands.

## Inspection And Hits

The control updates `FrameInspection` from the playhead after interaction
changes. While hovering framed content, it instead follows the nearest
frame under the pointer. `hit_result` identifies the topmost semantic
display item under that pointer.

Hosts read `inspection`, `hit_result`, and `interaction` after
`draw_timeline`. Their state remains expressed entirely in core types, so
host code does not need an ImGui-specific model.

The `layout` accessor also permits toolkit-neutral snapshot export through
`timeline::render_snapshot(layout->display_list())`.

## Rendering

`style_colour` maps `timeline::StyleRole` values to the current
`ImGuiStyle`. It uses text, window, plot, header, slider, and navigation
colors while respecting global style alpha and focus state.

`draw_display_list` visits primitives in their original paint order:

| Core Primitive | ImGui Operation |
| --- | --- |
| `Line` | `AddLine` |
| `Rectangle` and `Marker` | `AddRectFilled` |
| `Text` | `AddText` |
| `Polyline` | `AddPolyline` |
| `Swatch` | `AddRectFilled` with source RGB |

Geometry is translated from item-local coordinates to screen coordinates.
Lane labels are clipped to the label column. Stroke width follows the
current font size, and source-color swatches retain their RGB values while
applying the current ImGui alpha.

## Header Guide

| Header | Main Responsibility |
| --- | --- |
| `TimelineControl.h` | Retained state and per-frame item submission. |
| `TimelineRenderer.h` | Theme mapping and display-list delegation. |

Consumers link the `timeline-imgui` CMake target and include
`<imguiTimeline/TimelineControl.h>`. The target publicly links both
`timeline-core` and `imgui::imgui`.
