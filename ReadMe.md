[![CMake workflow](https://github.com/LegalizeAdulthood/timeline-control/actions/workflows/cmake.yml/badge.svg)](https://github.com/LegalizeAdulthood/timeline-control/actions/workflows/cmake.yml)

# Timeline Control

Sample code for the video Timelin Control.

# Obtaining the Source

Use git to clone this repository, then update the vcpkg submodule to bootstrap
the dependency process.

```
git clone https://github.com/LegalizeAdulthood/timeline-control
cd timeline-control
git submodule init
git submodule update --depth 1
```

# Building

A CMake preset has been provided to perform the usual CMake steps of
configure, build and test.

```
cmake --workflow --preset default
```

Places the build outputs in a sibling directory of the source code directory, e.g. up
and outside of the source directory.

# Beat-Keys Mappings

File > Open in `timeline-viewer` accepts `par-beatdown.beat-keys` mapping
configurations. Companion music paths are resolved relative to the
configuration. Source music and generated parameter lanes appear together;
the inspector retains the mapping recipes and output policy.

Supported sources are RMS, peak, and counted note, effect, and row pulses.
Generated keys match beat-keys scaling, offsets, clamping, six-decimal
rounding, overlapping exponential decay, and literal zero returns. Source
and generated lanes share the same frame rate and synchronization offset.

`timeline_par_animator::BeatKeysMapping` owns recipes and measured inputs.
Its `materialize()` method rebuilds a disposable generic document; the
viewer retains the mapping while the wx control owns the displayed
document. Mapping operations and output modes are preserved without
merging into authored animation. Already-realized overlays remain ordinary
imported keyframe lanes.

Fixtures under `tests/test-timeline-paranimator/fixtures/beat-keys` include
representative par-beatdown configurations and golden outputs, with input
paths adjusted to the copied music fixtures.

# ParAnimator Tracks

File > Open imports realized parameter keyframes from ParAnimator JSON,
including tracks inside layers. Catalog paths are relative to the JSON
file. Numeric scalars, arrays, and slash-separated tuples become numeric
keyframe lanes; tuples use one lane per component and retain the original
authored value. Catalog defaults and destination-key curve declarations
are translated to outgoing hold, linear, or geometric segments.

Categorical values use key instants and held spans without inventing
numeric values. The inspector shows source attributes and evaluated numeric
frame values. Missing catalogs or unusable documents fail import; invalid
tracks are diagnosed independently when other valid tracks remain.

Constant and line path recipes use exact hold and linear endpoint
definitions, matching ParAnimator's evaluator without per-frame caches.
Original path objects remain in inspection attributes. Constant paths
also support categorical values. Paths require at least two frames;
malformed recipes, unsupported kinds, and tracks with both keys and a
path receive importer diagnostics. Generated endpoints do not inflate
the authored keyframe count.

Circle and ellipse paths use owned analytic evaluators, with one curve
lane per component and no stored per-frame cache. Phase is in degrees;
turns span frame 0 through the final frame. Phase defaults to zero and
turns to one. Reverse turns and zero-radius paths are supported. Original
recipes remain in the inspector. Catalog-declared targets must be complex
or point2, centers must have two finite components, and radii must be
nonnegative. The control samples these definitions at frame boundaries;
without a frame grid, core layout samples at display resolution.

Lissajous paths use the same owned analytic component curves. Each
component has a required positive frequency and a nonnegative radius.
Phase defaults to zero and affects only the x component, matching
ParAnimator; the y component has no phase shift. Frequencies may be
fractional, and `turns` does not affect a Lissajous recipe.

Spiral paths interpolate the radius linearly from `from-radius` to
`to-radius` over the same frame domain as the angle. Both radii must be
finite and nonnegative. Turns default to one and phase to zero; reverse
turns, shrinking paths, constant radii, and zero radii are supported.
Component bounds use the larger endpoint radius. Definitions and recipes
remain owned by the document, without cached frame keys.

Bezier paths use owned de Casteljau evaluators with at least two control
points. Complex, numeric-tuple, point2/point3, and unnormalized
vector2/vector3 targets produce one curve lane per component. Numeric
tuples require a positive catalog arity; point/vector aliases validate any
declared arity. Control points must be finite slash-delimited strings of
the target arity. The original recipe remains inspectable; component
bounds use the control-point hull, not a cached set of frame samples.
Normalized vectors receive an explicit importer diagnostic until their
parameter-specific evaluation is supported.

Catmull-Rom paths share the control-point target validation and require
at least four points. Uniform segments visit every control point over
frame 0 through the final frame. Endpoint tangents use extrapolated
neighbors, matching ParAnimator. Component bounds include the equivalent
cubic Bezier hulls so overshoot is not clipped. The document owns the
analytic definitions and original recipes without storing frame samples.

File > Add imports another JSON beside the current document. Authored
animation inherits the current frame rate for comparison. The combined
document preserves exact item times and uses unique lane IDs; incompatible
timebases or rates are rejected without replacing the current display.
Open replaces all previously loaded inputs.

For manual comparison, open `fixtures/beat-keys/rms.beat-keys.json` under
`tests/test-timeline-paranimator`, then add `fixtures/multi-track.json`.
The source RMS curve, mapped outputs, and authored animation share one
control. Fixture catalogs contain the needed entries from ParAnimator's
catalog; `maxiter-step.json`, `single-layer.json`, and `inside-hold.json`
cover interpolation, layers, and categorical values.

Add `fixtures/path-generators.json` to compare constant and line paths
with source music and mapped outputs. This fixture and
`gold-path-generators.par` are copied from ParAnimator's integration
tests. At frame 1, `maxiter` is 321 and the `params.c` components are 1
and 2; the inspector retains the recipes beside their sampled values.

`ellipse-path.json` and `gold-ellipse-path.par` are copied from
ParAnimator's integration tests. Open the ellipse fixture directly, or
open the RMS mapping and add the ellipse and `circle-path.json` fixtures.
The resulting eight lanes compare source music, mapped output, and both
analytic paths. At frame 2 the ellipse is `-2/0`, and the reverse-turn
circle is `3/-2`. Clicking either curve inspects its identity and recipe.

`lissajous-path.json` and `gold-lissajous-path.par` are copied from
ParAnimator's integration tests. Open the Lissajous fixture and add
`beat-keys/rms.beat-keys.json` to compare six lanes, then add
`lissajous-phase.json` for eight lanes. At frame 2 the copied path is
`-2/0`; the phase-shifted path is approximately `-0.732051/-5`. Click a
component curve to inspect its distinct identity and original recipe.

`spiral-path.json` and `gold-spiral-path.par` are copied from ParAnimator's
integration tests. Open the spiral fixture and add
`beat-keys/rms.beat-keys.json` for six lanes, then add
`spiral-variants.json` for ten lanes. At frame 2 the copied path is `-2/0`.
At frame 4 the shrinking reverse-turn path is `1/-3`, and the
constant-radius path is `1/-4`. Curve inspection retains each recipe and
its distinct hit identity.

`bezier-path.json` and `gold-bezier-path.par` are copied from ParAnimator's
integration tests. Open the Bezier fixture and add
`beat-keys/rms.beat-keys.json` for six lanes. At frame 2 the path is `2/2`;
click either component to inspect its control points and hit identity.
Open `bezier-tuples.json` to replace the comparison with fifteen lanes
covering linear through quartic paths and tuple arities one through four.
At frame 2 the quartic tuple is `1/2/3/4`.

`catmull-rom-path.json` and `gold-catmull-rom-path.par` are copied from
ParAnimator's integration tests. Open the path fixture and add
`beat-keys/rms.beat-keys.json` for six lanes. At frame 3 the path is
`2/2.25`, including its overshoot. Click each component to inspect its
control points and distinct hit identity. Open `catmull-rom-tuples.json`
to replace the comparison with six lanes: a five-point position3 path,
a constant one-component tuple, and an unnormalized vector2 path. At
frame 3 the position is `2/2.25/6`.

PWM, specialized track kinds, non-clamp extrapolation, and
parameter-specific output quantization
remain unsupported.
Unsupported tracks receive importer diagnostics.
The viewer does not execute ParAnimator or merge values into its output.

# Native Timeline Control

The wx control uses its full client area for the timeline; document
metadata appears in the viewer's inspector. Tab and Shift+Tab move focus
between the control and inspector. Left and Right step the focused
control's playhead and keep it visible; Shift extends the selected range.
Selection remains visible with an inactive style when focus moves away.

Native colors, fonts, and DPI-scaled metrics feed the display-list
renderer. Resize, DPI, and system-color changes invalidate cached layout;
buffered painting keeps navigation repainting stable. Theme mapping has
headless adapter tests, while actual DPI and OS-theme transitions still
need manual verification on the target platforms.

# Display Snapshots

Open a JSON fixture through File > Open in `timeline-viewer`, then use
File > Export Snapshot to save the displayed timeline as text. The export
includes the current viewport, selection, and playhead geometry, but not
the viewer's metadata or inspector panels.

`timeline::render_snapshot` in `timeline/Snapshot.h` renders the same
display list without toolkit dependencies. Each line records a primitive
kind, semantic style role, quoted lane and item IDs, and integral geometry.
Text includes its quoted value; polylines include a point count followed
by coordinate pairs. Strings escape quotes, backslashes, control bytes,
and non-ASCII bytes. Output uses locale-independent numbers and LF line
separators.

Golden snapshots under `tests/test-timeline-paranimator/fixtures/snapshots`
exercise the existing empty, event, RMS curve, and keyframe JSON fixtures
at fixed viewport dimensions and layout metrics. Viewer exports reflect
native metrics and interaction state, so compare their visible structure
rather than expecting identical coordinates.

[Utah C++ Programmers](https://meetup.com/utah-cpp-programmers)\
[Past Topics](https://utahcpp.wordpress.com/past-meeting-topics/)\
[Future Topics](https://utahcpp.wordpress.com/future-meeting-topics/)
