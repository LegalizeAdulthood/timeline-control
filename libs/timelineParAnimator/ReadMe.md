<!-- Copyright (c) 2026 Richard Thomson -->

# ParAnimator Timeline Adapter

`timeline-paranimator` translates JSON produced or consumed by
[ParAnimator](https://github.com/LegalizeAdulthood/paranimator) and
[par-beatdown](https://github.com/LegalizeAdulthood/par-beatdown) into the
UI-framework-agnostic model provided by
[`timeline-core`](../timeline/ReadMe.md).

This library owns source-format semantics. It understands JSON schemas,
parameter catalogs, track recipes, frame synchronization, and beat-keys
mappings. The core library remains unaware of those concepts and receives
only generic documents, lanes, items, evaluators, and source summaries.

## Data Flow

```text
JSON file + JsonImportOptions
             |
             v
    import_timeline_json
             |
             +-- ParAnimator config
             +-- tracker timeline
             +-- beat-keys overlay
             +-- beat-keys config ------> BeatKeysMapping
             |                                  |
             v                                  v
 private validation and translation ------> materialize
             |                                  |
             +----------------+-----------------+
                              v
              DocumentBuilder + StringTableBuilder
                              |
                              v
                    timeline::Document
```

The returned document is ready for layout, inspection, and display by any
core presentation adapter. Import diagnostics remain in this adapter and
are not stored in the document.

## Public Import Types

| Type | Responsibility |
| --- | --- |
| `JsonImportOptions` | Timing and companion-input policy. |
| `JsonImportResult` | Document, mapping source truth, and diagnostics. |
| `MappingRecipe` | One feature-to-parameter transformation. |
| `MappingInput` | One frame-addressed measurement or event pulse. |
| `MappingOutput` | Output mode and namespace from beat-keys. |
| `BeatKeysMapping` | Retained mapping model and materialization logic. |

All public types are in the `timeline_par_animator` namespace.

## Import Options And Results

`JsonImportOptions` supplies the exact core tick rate and a rational frame
rate. ParAnimator files use that frame rate directly. A par-beatdown import
may instead obtain frame rate and synchronization offset from the optional
beat-keys companion path.

`import_timeline_json` has an overload using default options and another
accepting explicit `JsonImportOptions`. It reads the file, identifies its
format, validates source structure, and translates valid content.

`JsonImportResult` separates three outcomes:

- `document` owns the imported or materialized core document.
- `mapping` retains beat-keys recipes and inputs when applicable.
- `diagnostics` reports file, schema, record, and translation errors.

`succeeded` means that `document` contains a value. A successful import may
still contain diagnostics when unsupported tracks or malformed individual
par-beatdown records were skipped.

## Supported Inputs

### ParAnimator Config

A JSON object without a `schema` field is treated as a ParAnimator config.
The adapter creates a finite `FrameGrid`, resolves parameter catalogs, and
translates top-level tracks or layered tracks into core lanes.

The translation preserves authored keys and source metadata while
attaching source-specific evaluators where generic keyframes are not
enough. Supported forms include parameter keyframes, PWM, analytic and
control-point paths, color maps, camera and view tracks, tuple components,
normalization, and extrapolation policies.

One source track may produce several core lanes. This is deliberate:
source tracks retain ParAnimator semantics, while lanes are normalized
rows that the core can display and query.

### Tracker Timeline

The `par-beatdown.tracker-timeline` schema imports measured music features
and detected events. Valid events become `timeline::Instant` values in an
event lane. RMS measurements become `timeline::CurveSample` values in a
numeric curve lane.

Timing may come from seconds, frames, render metadata, or a beat-keys
companion. The adapter converts all forms to exact core time and preserves
frame and synchronization facts in `timeline::SourceSummary`.

Individual invalid feature or event records are diagnosed and skipped. A
schema or timing failure prevents document construction.

### Beat-Keys Overlay

The `par-beatdown.beat-keys-overlay` schema is already materialized output.
The adapter groups keyframes by target and creates one hold-interpolated
lane per target. Generator identity, source references, and counts become
a `timeline::GenerationSummary` within the document source summary.

### Beat-Keys Config

The `par-beatdown.beat-keys` schema refers to a source tracker timeline and
contains mapping bindings. The adapter imports that source, captures the
used measurements and event pulses, and constructs a `BeatKeysMapping`.

`JsonImportResult::mapping` retains this source-truth model. Its
`materialize` function computes generic hold-keyframe lanes and returns the
document placed in `JsonImportResult::document`.

## Mapping Model

`MappingRecipe` describes one transformation:

- `source` names a measured feature or counted event pulse.
- `target` names the animation parameter receiving the result.
- `operation` records how the downstream application applies the value.
- `scale`, `offset`, and optional `clamp` transform numeric values.
- `decay_seconds` defines the finite decay of event pulses.

`MappingInput` preserves each original source name, frame, and value.
Events have value one and simultaneous events remain separate inputs so
materialization can count them.

`MappingOutput` retains the requested overlay or merge mode and namespace.
It does not merge values into authored animation inside the core model.

`BeatKeysMapping` owns the imported source document, recipes, inputs,
output policy, and config path. `materialize` recomputes disposable
keyframe lanes:

- Continuous feature inputs are transformed at their source frames.
- Event pulses are counted, decayed, summed, and returned to zero.
- Generated lanes use hold interpolation and retain source, target, and
  operation attributes.
- The original source music lanes remain unmodified beside generated lanes.

Keeping recipes and inputs separate from the materialized document allows
the display cache to be regenerated without losing source semantics.

## Core Translation

The adapter uses `timeline::StringTableBuilder` to intern lane and item
identities, then seals them with lanes through `timeline::DocumentBuilder`.
The resulting document may contain:

| Core Type | Adapter Use |
| --- | --- |
| `FrameGrid` | Animation and synchronized music timing. |
| `Lane` | A normalized parameter, component, feature, or event row. |
| `Instant` | Detected music events. |
| `Curve` | Analytic paths and sampled music features. |
| `Keyframe` | Authored or generated parameter values. |
| `PaletteCurve` | Time-dependent ParAnimator color maps. |
| `SourceSummary` | Schema, extent, and record counts. |
| `GenerationSummary` | Generator, source, target, and grouping facts. |

ParAnimator interpolation, extrapolation, normalization, and parameter
output rules are attached as owned core evaluators. Their captured state
retains the adapter semantics after import without adding source-specific
types to `timeline-core`.

## Compatibility Details

### Beat-Keys

Supported sources are RMS, peak, and counted note, effect, and row pulses.
Generated keys preserve beat-keys scaling, offsets, clamping, six-decimal
rounding, overlapping exponential decay, and literal zero returns. Source
and generated lanes share the same frame rate and synchronization offset.

### Scalar, Tuple, And Categorical Tracks

Catalog numeric and integer tuples, points, and vectors require numeric
array keys. Tuples use one lane per component and retain the original
authored value. Catalog defaults and destination-key curve declarations
become outgoing segments. Ordinary numeric targets support linear, hold,
and step curves.

Keyed scalar and tuple targets require catalog default curves even when a
destination key overrides the curve. Raw tuples validate catalog arity and
component bounds. Doubles require scalar values within bounds.

Categorical values use key instants and held spans without inventing
numeric values. Missing catalogs or unusable documents fail import.
Invalid tracks are diagnosed independently when other tracks remain valid.

### Paths And Normalization

Constant and line paths use exact hold and linear endpoint definitions.
Circle, ellipse, Lissajous, and spiral paths use owned analytic component
evaluators. Bezier paths use de Casteljau evaluation. Catmull-Rom paths use
uniform segments with extrapolated endpoint neighbors.

Path definitions remain owned by the document rather than expanded into
per-frame caches. Component bounds include analytic extrema or equivalent
cubic hulls so intermediate overshoot is not clipped. Original recipes
remain available as inspection attributes.

Catalog-normalized vector paths clean tiny components, normalize after
interpolation, and clean the result. Bounded subdivision rejects zero
crossings, tangencies, and cleanup-induced singularities between frames.
Numeric-array keys retain their raw interpolation and expose normalized
parameter output separately. Unnormalized vectors and points remain
unchanged.

Constant and line vector paths remain importer errors because ParAnimator
resolves them to string endpoint keys while its numeric tuple evaluator
requires numeric arrays.

### PWM Tracks

Unslotted PWM supports yes-no, enum, inside, outside, and
integer-or-enum targets. Two mix keys span the full frame range. `mix` and
`duty` are aliases for finite values in `[0, 1]`. Each track produces held
output spans and a numeric mix lane without increasing the authored key
count.

Function-slot PWM uses zero-based `function[index]` targets. Formula
sources resolve catalog function-list metadata; other fractal types require
a declared slot. Source PAR entries are read relative to the JSON file.
Only the selected slot is replaced, and missing slots are padded with
`ident`.

### Camera Tracks

Camera2D center-mag and corners tracks expose derived output components
beside nested look-at, view-up, height, eye, and optional skew inputs.
Look-at and eye accept keyed values and supported analytic paths. View-up
normalizes after interpolation. Height supports linear, hold, step, and
geometric interpolation.

Center-mag aspect comes from source stretch and video mode. Corners aspect
comes from source extents or edge lengths independently of video mode.
Corner bounds enclose every orientation over the complete camera interval.

Eye paths are checked for collisions and singular directions across the
continuous interval, including between frame boundaries. Eye takes
precedence over authored view-up and supplies a separate derived view-up
curve. Optional skew is checked for geometric-sign consistency and tangent
poles.

Id 3D view tracks support all twelve explicit keyed members. Shared camera
input supports keyed point3 eye and look-at signals plus normalized vector3
hints. Derived output includes Euler rotation, perspective distance, and
xyshift. Explicit members override corresponding derived outputs.

Julibrot view tracks support mode, six-component geometry, eyes, and
four-component from-to signals. Continuous checks reject vertical
crossings, zero directions, reversed or singular hints, unsafe
normalization, and perspective overflow. Camera3D paths are not part of
the source format.

### Color Maps

Color-map tracks support static files, gradients, and two keyed source
files with linear or hold interpolation. Gradients accept RGB, HSV, HSL,
and fixed CSS named colors. Effects include adjustments, indexed
transforms, and masks with numeric amount signals.

Imported palettes own all 256 ordered RGB entries. Referenced source files
are read during import, so document evaluation never reopens files or
writes output maps. Effect order, ranges, masks, and seeded sparkle state
remain part of the retained recipe.

### Extrapolation And Parameter Output

Numeric scalar tracks support `base`, `omit`, `cycle`, and `ping-pong`
extrapolation. `base` uses the source PAR value outside the keyed range.
`omit` produces an absent value and a visible gap. `cycle` and `ping-pong`
retain exact tick periods without quantizing the authored curve.

Integer, integer-tuple, and numeric integer-or-enum keyframes expose
parameter output separately from continuous values. Output rounds to the
nearest integer with halfway values away from zero after extrapolation.
The core evaluator carries this adapter policy while layout continues to
draw the unquantized curve.

Other specialized track kinds and parameter forms remain unsupported.
Unsupported tracks receive importer diagnostics and contribute no partial
lanes. The adapter neither executes ParAnimator nor merges imported values
into an output PAR document.

See the
[catalog compatibility audit](../../docs/catalog-compatibility.md) for
reference evidence, rejected forms, and unresolved review items.

## Compatibility Fixtures

Fixtures under `tests/test-timeline-paranimator/fixtures` include copied
ParAnimator integration cases, source-generated golden PAR output, color
maps, and representative par-beatdown mappings. They cover supported path,
PWM, camera, palette, extrapolation, normalization, and integer-output
policies together with transactional importer failures.

Beat-keys fixtures are under the `beat-keys` subdirectory, with input paths
adjusted to copied music fixtures. Snapshot fixtures use fixed viewport
dimensions and layout metrics to exercise empty, event, RMS curve, and
keyframe documents.

## Implementation Files

Only headers under `include/timelineParAnimator` are public.

| File | Responsibility |
| --- | --- |
| `TimelineJson.h` | Import options, result, and entry points. |
| `BeatKeysMapping.h` | Public mapping source-truth model. |
| `TimelineJson.cpp` | Dispatch, validation, and lane translation. |
| `BeatKeysMapping.cpp` | Mapping validation and materialization. |
| `ParameterCatalog.*` | Catalog validation and composition. |
| `ColorMap.*` | ParAnimator color-map translation. |
| `NamedColors.*` | Accepted CSS named-color definitions. |

Consumers link the `timeline-paranimator` CMake target and include
`<timelineParAnimator/TimelineJson.h>`.
