<!-- Copyright (c) 2026 Richard Thomson -->

# Timeline Core Library

`timeline-core` provides the UI-framework-agnostic model and algorithms
for displaying a timeline. Source adapters translate application data
into a `Document`; GUI adapters translate native input into core
operations and render the resulting `DisplayList`.

The core owns timeline mechanism, not application or GUI semantics. It
does not read files, report import errors, choose native colors or fonts,
or draw through a toolkit API.

## Data Flow

```text
source adapter
      |
      v
DocumentBuilder + StringTableBuilder
      |
      v
   Document -----------------------> inspect_*() -> inspections
      |
      +---------------------------> Interaction
      |
      +-- Navigation -> Viewport --+
                                      \
LayoutMetrics -----------------------> Layout -> DisplayList -> renderer
Interaction --------------------------/   |
                                          +-> hit_test() -> HitResult
```

The source adapter supplies application semantics while constructing the
document. The GUI adapter owns native controls and rendering resources.
The types between them contain only exact time, generic timeline content,
toolkit-neutral geometry, and view state.

## Time

| Type | Responsibility |
| --- | --- |
| `Ticks` | Signed integer storage for exact timeline coordinates. |
| `Duration` | An elapsed number of ticks without an origin. |
| `Time` | An absolute tick coordinate on a timeline. |
| `Timebase` | Converts exact ticks to and from seconds. |
| `TimeRounding` | Selects rounding for conversions to integral ticks. |
| `FrameGrid` | Projects a finite sequence of frames onto exact time. |

Time is continuous in the domain model even though its representation is
an exact integer count. A `FrameGrid` adds frame rate, frame count, and
offset without making frames the fundamental unit. Curves remain
definitions in continuous time and can be sampled at frame boundaries
when required.

## Document Model

`Document` is the root model. It owns a `Timebase`, optional `FrameGrid`,
metadata, optional source information, a `StringTable`, and an ordered
set of `Lane` objects. An empty document is valid.

`Metadata` contains human-facing document text. `SourceSummary`,
`GenerationSummary`, `SourceReference`, and `NamedCount` preserve shallow
facts about imported or generated content without embedding a source
format in the core model.

A `Lane` is one normalized display row with a finite time range. It owns
an ordered collection of `Item` values. Source formats may call their
authored structures tracks; adapters decide how those structures become
core lanes.

`Item` is a variant of the following content types:

| Type | Meaning |
| --- | --- |
| `Instant` | An event at one exact time. |
| `Interval` | Content with an exact start and end. |
| `Envelope` | An interval divided into attack, sustain, and decay. |
| `Curve` | A sampled or analytically evaluated numeric signal. |
| `Keyframe` | An authored value and outgoing interpolation policy. |
| `PaletteCurve` | An analytically evaluated palette over time. |

`CurveSample` stores one numeric sample. `CurveInterpolation` describes
how stored samples are joined. `CurveEvaluator` supplies an owned analytic
definition. `RgbColor` and `Palette` are the values returned by a
`PaletteCurve` evaluator.

Keyframes are interpreted in lane context. `KeyframeNeighbors` identifies
the keys around a query time. A lane may own a `KeyframeEvaluator` that
replaces default interpolation and a separate `KeyframeOutputEvaluator`
that converts the continuous value to application output. The adapter
supplies those semantics; the core supplies storage, evaluation, and
display.

## Identity And Construction

`StringId` is a compact integer identity. It is meaningful only with the
`StringTable` that created it; zero identifies the empty string. Tables
are immutable after construction, and copies share their string storage.

`Attribute` stores one key and value as `StringId` values. `Attributes`
owns an immutable vector sorted by key identity, rejects empty or
duplicate keys, and supports allocation-free lookup. Repeated values and
empty values are valid. Source adapters intern attribute text while
translating their input; presentation code resolves the IDs only when it
needs text.

Adapters normally construct documents through `DocumentBuilder`. Its
`StringTableBuilder` interns lane and item identities together with lane
and item kinds, labels, attribute keys, and attribute values. `add_lane`
collects completed lanes and verifies that every string ID belongs to the
document table. `build` seals the strings and lanes into a `Document`.

The `with_id`, `with_strings`, `with_kind`, and `with_attributes`
functions return copies with translated identities and semantic strings.
They allow `DocumentBuilder::append` and `combine_documents` to move
content between documents whose string IDs belong to different tables,
including attribute keys and values. Item IDs remain qualified by their
lane; `DisplayId` therefore contains both a lane ID and an item ID.

## Viewing And Rendering

`Navigation` owns toolkit-neutral zoom and scroll state. Given host pixel
dimensions, it produces a `Viewport` containing the visible time range and
first visible lane. `LayoutMetrics` supplies host-measured row and padding
dimensions without introducing native GUI types.

`Layout` combines a `Document`, `Viewport`, `LayoutMetrics`, and optional
`Interaction`. It produces two related results:

- A `DisplayList` containing ordered, toolkit-neutral drawing primitives.
- Hit regions queried through `hit_test`, which returns a `HitResult`.

`Primitive` is a variant of `Line`, `Rectangle`, `Text`, `Marker`,
`Polyline`, and `Swatch`. Each primitive has a semantic `StyleRole` and a
`DisplayId`. GUI adapters map roles to native colors and fonts, then
delegate each primitive to the toolkit's drawing API. Static `Text`
content is a `StringId`; the display list owns the immutable string table
used by renderers to resolve it at the presentation boundary.

`render_snapshot` serializes a display list into deterministic text. It is
used for testing and export without depending on a GUI toolkit.

## Interaction And Queries

`Interaction` stores the playhead, selected lane, selected items, and
selected time range. It is initialized from a document but remains
separate from it; replacing a document creates fresh interaction state.
Frame-based documents snap interaction times through their `FrameGrid`.

`TimeRange` represents an exact selected range and `FrameRange` represents
an inclusive frame-index range. `HitResult` connects layout hit testing
back to selection through stable `DisplayId` values.

Read-only query functions do not expose rendering details:

- `inspect_frame` returns a `FrameInspection` for every lane at one frame.
- `inspect_range` returns a `RangeInspection` over exact time bounds.

Both use `LaneInspection` and `InspectionItem` records. An
`InspectionItemType` identifies the content category, while
`InspectionItemRole` explains whether it is active, sampled, neighboring,
or exact at the query location. Numeric samples, application output,
attributes, and palette samples remain distinct values for the
presentation layer to format.

## Snapshot Format

`timeline::render_snapshot` writes one record per primitive. Each record
contains the primitive kind, semantic style role, quoted lane and item
IDs, and integral geometry. Text includes its quoted value, polylines
include a point count and coordinate pairs, and swatches include bounds
and RGB components.

Strings escape quotes, backslashes, control bytes, and non-ASCII bytes.
Output uses locale-independent numbers and LF separators.

## Header Guide

| Header | Main Responsibility |
| --- | --- |
| `Time.h` | Exact time, conversion, and frame projection. |
| `StringTable.h` | Interned identities and immutable string lookup. |
| `Attributes.h` | Immutable interned attribute keys and values. |
| `Document.h` | Root model, provenance, construction, and composition. |
| `Lane.h` | Lane ownership, item variants, and keyframe evaluation. |
| `Event.h` | Instant events. |
| `Interval.h` | Events with duration. |
| `Envelope.h` | Attack, sustain, and decay intervals. |
| `Curve.h` | Sampled and analytic numeric curves. |
| `Keyframe.h` | Authored values and interpolation. |
| `Palette.h` | Colors and time-dependent palettes. |
| `Layout.h` | Navigation, viewports, layout, and hit testing. |
| `DisplayList.h` | Semantic drawing primitives. |
| `Interaction.h` | View-only playhead and selection state. |
| `Query.h` | Frame and range inspection results. |
| `Snapshot.h` | Deterministic display-list serialization. |
| `size_cast.h` | Container-size conversion used by public counts. |

Consumers link the `timeline-core` CMake target and include public headers
as `<timeline/Document.h>`, `<timeline/Layout.h>`, and so on.
