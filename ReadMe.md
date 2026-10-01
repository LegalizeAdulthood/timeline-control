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

Unslotted PWM tracks support yes-no, enum, inside, outside, and
integer-or-enum targets. Two mix keys must span frame 0 through the final
frame; `mix` and `duty` are aliases for finite values in `[0, 1]`. The mix
interpolates linearly. An integer window of at least two frames uses
`lround(mix * window)` output-b frames, followed by output-a frames, with
the phase repeating from frame 0. Endpoints use `a`/`off` and `b`/`on`
aliases. Yes-no endpoints are booleans defaulting to false and true;
other endpoints are catalog values, including bounded integer strings for
inside/outside targets.

Each PWM track produces an output lane of contiguous equal-value spans
and a numeric mix lane. Both retain the original recipe for inspection;
mix keys also retain their authored inputs. Generated transitions do not
increase the authored keyframe count.

Function-slot PWM uses zero-based `function[index]` targets. Formula
sources resolve the catalog's `function-list` metadata; other fractal
types require a declared function slot. Endpoints use ParAnimator's
`id-functions` set. The importer reads the named source PAR entry relative
to the JSON file, respecting comments and continued lines. Output replaces
only the selected slot and pads missing slots with `ident`. The inspector
retains the complete source function list, source entry, slot, and recipe.
Layer tracks resolve their own source entries.

Keyed Camera2D center-mag and corners tracks expose six output components
and five nested input components, plus optional skew. Look-at is point2,
view-up is vector2, and height is a positive double. Keyed inputs require
two full-range keys. Look-at and eye also accept constant and line paths,
represented by endpoint keys with the original path recipes retained.
View-up is normalized after interpolation; an authored normalize flag
must be true. Height supports linear, hold/step, and geometric
interpolation; vector inputs do not support geometric interpolation.

Center-mag aspect comes from the source PAR stretch and video mode F6.
A missing or zero x-magnification factor means one; negative stretch
is retained in output while aspect uses its magnitude. Output and
normalized view-up are owned analytic curves, not baked frame samples.
The inspector retains the complete camera recipe and nested signal keys.

Corners output derives aspect from the source PAR's four-value extents or
six-value edge lengths, independently of video mode. Six stable analytic
lanes represent top-left x, bottom-right x/y, top-left y, and bottom-left
x/y. Compact axis-aligned output expands to the same affine components;
rotated views retain all three corners. Bounds enclose every orientation
over the full camera interval. Degenerate source views are diagnosed by
the importer.

Eye inputs may also use circle/ellipse orbits with positive radii and a
fixed look-at, including offsets from the orbit center. The importer
checks the authored arc for singular directions, including collisions
between frames, reverse turns, phase wrapping, and stationary paths.
A look-at on the supporting ellipse is allowed if the traveled arc does
not reach it. Spiral eyes also support fixed look-at with positive start
and end radii. Expanding and shrinking spirals check direction at the
analytic radius-crossing time, including collisions between frames;
constant-radius spirals use the orbit check. Reverse and stationary
spirals retain their authored recipes and analytic component bounds.
Lissajous eyes support positive radii, independent positive axis
frequencies, and fixed look-at. Phase applies only to x. Analytic crossings
of the slower axis identify collision candidates over the traveled cycles,
including partial cycles and singular directions between frames.
Equal-frequency paths need only two candidates.
Bezier and Catmull-Rom eyes also support fixed look-at. Recursive Bezier
hulls validate the entire path, including tangencies and segment
boundaries, while preserving the original analytic definitions. Catmull-Rom
segments use equivalent cubic hulls. A hull containing look-at is refined,
not rejected merely for overlap. Validation has bounded work and depth;
extreme frequencies or unresolved possible singularities produce explicit
importer diagnostics rather than falling back to frame sampling.
Eye takes precedence over authored view-up. The viewer retains both
authored signals and separate
derived-view-up curves, normalized from eye minus look-at. Keyed motion
is rejected if its direction crosses zero, including mixed hold/linear
inputs and crossings between frame boundaries.

Without eye, look-at also accepts circle, ellipse, Lissajous, spiral,
Bezier, and Catmull-Rom paths with keyed view-up and height. Output center
curves reuse the owned analytic definitions and bounds, including interior
extrema. Nested look-at curves retain their original recipes and distinct
hit identities.

Optional Camera2D skew uses two full-range numeric keys, in degrees, with
linear, hold/step, or geometric interpolation. Geometric endpoints must
have the same nonzero sign and a finite positive ratio. Negative geometric
skew retains its authored keys in an owned analytic input definition.
Center-mag exposes skew directly; corners compose its tangent shear with
height and normalized direction. Corners reject tangent poles at endpoints
or between frames, but allow held jumps between valid tangent branches.
Bounds enclose the full sheared, rotating camera interval. Skew paths are
not part of the ParAnimator format and produce importer diagnostics.

Id 3D view tracks support all twelve explicit keyed members: rotation,
perspective, xyshift, scalexyz, roughness, sphere, longitude, latitude,
radius, stereo, interocular, and converge. Each has two full-range keys;
numeric tuples declare their arity and use slash-delimited strings.
Numeric members use linear or hold/step interpolation. Integer output
rounds halfway values away from zero and retains separate authored-key
lanes. Sphere uses boolean keys and held yes/no output intervals. Catalog
output aliases, component identities, layer sources, and complete recipes
remain inspectable. Invalid tracks contribute no partial lanes.

Shared camera3d input supports keyed point3 eye and look-at signals, plus
a positive world-up vector3. Centered look-at and nonvertical direction
produce analytic Euler rotation, rounded perspective distance, and zero
xyshift. Explicit members override their corresponding camera outputs;
fully overridden cameras retain unused raw inputs without evaluating
their geometry. Continuous interval checks reject vertical crossings,
zero directions, and perspective overflow, including between frames.
Other view-up directions receive an explicit unsupported-input diagnostic;
the remaining parameter-evaluation audit will address roll-free variants.
Camera3d paths are not part of the source format.

The Id camera fixture and golden output are copied from ParAnimator. The
keyed, hold, and override goldens use its existing loader and renderer.
The local source schema declares slash-delimited Id tuple strings, but
its loader requires numeric arrays. Golden generation converts only that
tuple representation before calling the source evaluator; the importer
accepts the schema-defined strings. No ParAnimator source was changed.

Open `id-3d-view-camera.json` for fifteen lanes. At frame 1, rotation y is
-45 degrees and perspective is 7. Open the RMS mapping and add the camera
for nineteen comparison lanes; click an output curve to inspect its recipe
and identity. Open `id-3d-view-keyed.json` to replace the comparison with
twenty-five lanes; perspective is 12 at frame 1. Opening
`invalid-id-3d-view.json` reports a singular camera and leaves the
displayed document, selection, and playhead unchanged. These files are under
`tests/test-timeline-paranimator/fixtures`.

Julibrot view tracks support mode, six-component geometry, eyes, and
four-component from-to signals. Each explicit member uses two full-range
keys with catalog types, aliases, and bounds. Mode is a held enum; numeric
members support linear and hold/step interpolation without integer
rounding. Component identities, layer sources, and complete recipes remain
inspectable. Invalid tracks contribute no partial lanes.

Julibrot camera3d uses centered, straight-on axis-aligned keyed inputs.
View-up has zero x and positive y; z may vary. Normalization follows
interpolation, and the source geometry's first five components stay fixed;
only its distance component comes from the camera. Continuous bounds
reject degenerate directions and unsafe normalization, including between
frames. Explicit geometry overrides retain unevaluated raw camera inputs.
Near-axis tolerance variants and extreme normalization ranges receive
explicit diagnostics pending the final parameter-evaluation audit.

The Julibrot camera fixture is copied from ParAnimator. Six
source-generated goldens cover camera, normalized view-up, keyed members,
hold/step, and overrides. Like Id tuples, explicit Julibrot tuple strings
are converted to arrays only for reference generation with the source
loader; the importer accepts the schema-defined strings without changing
ParAnimator.

Open `julibrot-view-normalized.json` for fifteen lanes and distance 18.5
at frame 1. Add the RMS mapping for nineteen comparison lanes; inspect
geometry component 5 and its owned camera recipe. Open
`julibrot-view-keyed.json` to replace the comparison with twelve lanes,
including held mode and fractional eye spacing. Opening
`invalid-julibrot-view.json` reports an unsupported camera and leaves the
displayed document, selection, and playhead unchanged. These fixtures are
under `tests/test-timeline-paranimator/fixtures`.

Color-map tracks support static files, gradients, and two keyed source
files with linear or hold/step interpolation. Import owns all 256 ordered
RGB entries; gradients accept RGB, HSV, HSL, and fixed CSS named colors.
Palette definitions remain the source of truth and are sampled at frame
boundaries for swatch display. Inspection retains the complete recipe,
source filenames, authored keys, and sampled RGB values, without inventing
scalar lanes. Brightness, contrast, gamma, hue-shift, and saturation
effects apply in source order, rounding and clipping RGB entries after
each effect.
Gamma uses the amount as its positive exponent; hue and saturation operate
in HSL. Each nested amount retains its two authored keys in a separate
inspectable lane. The destination key selects linear or hold/step
interpolation, with endpoint clamping outside partial key ranges.
Reverse supports whole maps or inclusive ranges; remap retains a
destination-indexed 256-entry source lookup, including repeated indices.
Ping-pong rounds its keyed offset halfway away from zero, folds signed
offsets through a cycle of twice the range span, and rotates toward higher
destination indices. Singleton ranges are unchanged. The separate offset
lane retains unrounded values and destination-key interpolation.
Mask-blend owns its config-relative input map and blends inclusive ranges
against the incoming palette once, even when ranges overlap. Pulse blends
a required inclusive range toward a concrete color. Both amounts are
bounded to [0, 1]. Sparkle bounds its amount to [0, 255], rounds halfway
away from zero, and clamps independently perturbed RGB channels. Its
nonnegative integer seed resets a local random engine for every sample;
queries and copied documents do not advance hidden random state.

The gradient fixture and golden map are copied from ParAnimator. Other
color-map goldens use its unmodified loader and renderer, including all
148 named colors and partial key ranges. Input map paths resolve relative
to the imported JSON; evaluation never reopens files or writes output maps.

Open `color-map-keyed.json` for palette and authored-key lanes. At frame 1,
entry 0 is RGB `0/128/128` and entry 255 is `128/0/128`. Add
`beat-keys/rms.beat-keys.json` for six comparison lanes and inspect the
palette's identity and recipe. Open `color-map-mixed.json` to replace the
comparison with one gradient lane. Opening `invalid-color-map.json`
reports an out-of-range color without replacing the document, selection,
or playhead. These fixtures are under
`tests/test-timeline-paranimator/fixtures`.

Open `color-map-effects-brightness.json` for palette and amount lanes. At
frame 1, the amount is `1.5` and every entry is RGB `150/180/255`. Add the
RMS fixture for six comparison lanes and inspect independent palette and
amount hit identities. Open `color-map-effects-adjustments.json` to replace
the comparison with five lanes and RGB `0/255/0`. Opening
`color-map-effects-invalid.json` reports nonpositive gamma without
replacing the document, selection, or playhead.

Brightness and adjustment fixtures are copied from ParAnimator with local
catalog paths. Its unchanged color-map evaluator generated 32 golden maps
covering all five adjustments, negative amounts, hue wrapping, partial
hold ranges, and effect order. Import rejects malformed nested signals and
unsafe numeric ranges transactionally, without exporting partial lanes.

Open `color-map-indexed-effects.json` for palette and ping-pong offset
lanes. At frame 1, the raw offset is `1` and entry 2 is RGB `2/253/2`.
Add the RMS fixture for six comparison lanes. Open
`color-map-indexed-variants.json` to replace them with thirteen lanes;
the first palette reverses all entries, with RGB `255/0/63` at entry 0.
Opening `color-map-indexed-invalid.json` reports an unsafe rounded offset
without replacing the document, selection, or playhead.

The indexed-effects fixture is copied from ParAnimator with a local
catalog and output name. Its indexed input reproduces the source test
generator. Forty maps from the unchanged source evaluator cover inclusive
ranges, remap duplicates, signed offsets, singleton ranges, integer
limits, partial hold ranges, and ordered composition with adjustments.

Open `color-map-masked-effects.json` for four palette and amount lanes.
At frame 1, entries 0 through 3 are RGB `255/0/0`, `1/254/1`,
`255/255/255`, and `3/3/3`. Add the RMS fixture for eight comparison lanes
and inspect independent palette and amount identities. Open
`color-map-masked-variants.json` to replace them with sixteen lanes.
Opening `color-map-masked-invalid.json` reports an out-of-range mask
amount without replacing the document, selection, or playhead.

The masked-effects fixture is copied from ParAnimator with a local
catalog. Thirty-eight maps from its unchanged evaluator cover owned
masks, overlapping ranges, pulse colors, seeded sparkle, amount limits,
partial hold ranges, and effect order. Sparkle uses the source's
`std::mt19937` and `std::uniform_int_distribution<int>`, drawing red,
green, then blue for each ascending palette index. The distribution's
mapping is STL-dependent: positive sparkle goldens verify MSVC, while
independent engine-reset and RGB-order tests verify the same source
semantics on every platform. Seeded palettes need not match across STLs.

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

`yes-no-pwm.json` and `gold-yes-no-pwm.par` are copied from ParAnimator's
integration tests. Open the PWM fixture and add
`beat-keys/rms.beat-keys.json` for six lanes. At frame 2 the PWM output is
`yes` and its mix is `0.666667`. Click the output span or mix lane to
inspect its identity, endpoints, window, and original recipe. Open
`pwm-variants.json` to replace the comparison with eight lanes. At frame 2
the inside output is `1` and the reversed boolean output is `no`.

`function-slot-pwm.json`, `gold-function-slot-pwm.par`, and `input/source.par`
are copied from ParAnimator's integration tests. Open the slot fixture and
add `beat-keys/rms.beat-keys.json` for six lanes. At frame 2 the function
output is `sin/log` and mix is `0.666667`; slot 0 retains the source `sin`.
Click the output span or mix lane to inspect the slot and source values.
Open `function-slot-variants.json` to replace the comparison with four
lanes. At frame 2 slot 0 emits `log/cos`, while slot 3 emits
`sin/cos/ident/log`. These are independent track outputs, not a merged PAR.

`camera2d-center-mag.json` and `gold-camera2d-center-mag.par` are copied
from ParAnimator's integration tests. Open the camera fixture for eleven
lanes and add `beat-keys/rms.beat-keys.json` for fifteen comparison lanes.
At frame 1, center is `-0.375/0.25`, magnification is `1.414214`, and
height is `2.121320`. Click an output curve to inspect its camera recipe
and source entry. Open `camera2d-keyed-variants.json` to replace the
comparison with twenty-two lanes. At frame 1, the first camera rotation
is 45 degrees, both view-up components are `0.707107`, and magnification
is `1.333333`; its source x-magnification factor remains `-2`.

`camera2d-corners.json` and `camera2d-eye-corners.json` are copied from
ParAnimator's integration tests, with source-generated golden PAR output.
Open the eye fixture for thirteen lanes and add
`beat-keys/rms.beat-keys.json` for seventeen comparison lanes. At frame 1,
the corners are `-2/2/1/-1/2/-1`. Click a corner curve to inspect its
identity and source recipe. Open `camera2d-corners-rotated-source.json`
to replace the comparison with eleven lanes; its six-value source view
supplies aspect 2 even with video mode F7. Invalid corners fixtures report
importer errors without replacing the displayed document.

`camera2d-skew-center-mag.json` and `camera2d-skew-corners.json` are copied
from ParAnimator's integration tests with their golden PAR output. Open
the corners fixture for twelve lanes and add
`beat-keys/rms.beat-keys.json` for sixteen comparison lanes. At frame 1,
skew is 5 degrees and top-left x is `-0.825023`. Click a corner curve to
inspect its identity and camera recipe. Open `camera2d-skew-eye.json` to
replace the comparison with sixteen lanes: its analytic eye and signed
geometric skew produce -20 degrees of skew at frame 2. The corners form
and geometric, hold/step, and reversed variants have source-generated
golden output. Opening `invalid-camera2d-skew-pole.json` reports an
off-grid tangent singularity without replacing the displayed document.

`camera2d-eye-center-mag.json` and its golden PAR output are copied from
ParAnimator's integration tests. Open the eye fixture for thirteen lanes
and add `beat-keys/rms.beat-keys.json` for seventeen comparison lanes.
At frame 1, eye is `-1/0`, derived view-up is `-1/0`, and rotation is -90
degrees. Click an eye curve to inspect its identity and circle recipe.
Open `camera2d-eye-variants.json` to replace the comparison with
twenty-eight lanes. The reversed ellipse rotates 90 degrees at frame 1
despite the authored vertical view-up. The keyed camera moves its look-at
and eye together, retaining zero rotation and a vertical derived direction.

`camera2d-straight-paths.json` composes a line look-at with keyed view-up
and height. Its golden PAR values were generated with ParAnimator.
Open it for eleven lanes and add `beat-keys/rms.beat-keys.json` for fifteen
comparison lanes. At frame 2, center is `2/1`, magnification is `1.333333`,
and rotation is 45 degrees. Click a look-at endpoint to inspect its key
identity and retained line recipe. Open `camera2d-straight-eye.json` to
replace the comparison with fifteen camera lanes. At frame 2, its fixed
eye at `0/3` and moving look-at at `1/0` give rotation `-18.434949`,
overriding the authored vertical view-up. Its golden output also comes
from ParAnimator. View-up and height paths remain pending; the current
source schema requires keys for those members.

`camera2d-look-ellipse.json` opens eleven camera lanes. Add
`beat-keys/rms.beat-keys.json` for fifteen comparison lanes. At frame 2,
center is `-2/0`, magnification is one, and rotation is zero. Click a
nested look-at curve to inspect its path identity and original recipe.
Open `camera2d-look-catmull-rom.json` to replace the comparison with eleven
lanes; its frame-2 center is `2/2.25`. The inspector retains the authored
control points. Six curved look-at fixtures cover the supported forms;
their golden PAR values were generated with ParAnimator.

Open `camera2d-offset-circle.json` for fifteen camera lanes and add
`beat-keys/rms.beat-keys.json` for nineteen comparison lanes. At frame 1,
eye is `2/1` and rotation is `63.434949` degrees. Clicking an eye curve
inspects its distinct identity and retained offset recipe. Open
`camera2d-offset-ellipse.json` to replace the comparison with fifteen
lanes; its reverse half-turn gives frame-1 rotation `65.625583` degrees.
`camera2d-offset-partial.json` covers a look-at on the supporting circle
outside the traveled arc. Golden PAR values for all three fixtures were
generated with ParAnimator.

Open `camera2d-eye-spiral-expanding.json` for fifteen camera lanes and add
`beat-keys/rms.beat-keys.json` for nineteen comparison lanes. At frame 1,
eye is `-0.5/-2` and rotation is `-90` degrees. Clicking an eye curve
inspects its path identity and original recipe. Open
`camera2d-eye-spiral-shrinking.json` to replace the comparison with fifteen
lanes; its offset look-at and reverse half-turn give frame-1 rotation
`141.623325` degrees. Offset and stationary fixtures cover a safe radius
crossing and constant-radius zero-turn motion. All four golden PAR files
were generated with ParAnimator; rotations are compared modulo 360 degrees
to allow equivalent orientations at either end of the angle range.

Open `camera2d-eye-lissajous-centered.json` for fifteen camera lanes and
add `beat-keys/rms.beat-keys.json` for nineteen comparison lanes. At frame
1, eye is `0/1` and rotation is zero. Click an eye curve to inspect its
path identity and original recipe. Open `camera2d-eye-lissajous-phase.json`
to replace the comparison with fifteen lanes; its x-only phase gives
frame-1 eye `-0.414214/-1` and rotation `-54.735610` degrees.
`camera2d-eye-lissajous-partial.json` covers a safe partial cycle with
look-at on the supporting circle but outside the traveled arc. All three
golden PAR files were generated with ParAnimator.

Open `camera2d-eye-lissajous-independent.json` for fifteen camera lanes
and add `beat-keys/rms.beat-keys.json` for nineteen comparison lanes.
At frame 1, eye is `-1.414214/0` and rotation is `-90` degrees. Click an
eye curve to inspect its independent frequencies, phase, and hit identity.
Open `camera2d-eye-bezier.json` to replace the comparison with fifteen
lanes; frame-2 eye is `1/0.25` and rotation is `75.963757` degrees.
`camera2d-eye-catmull-rom.json` covers a safe path whose hull overlaps
look-at; its frame-2 eye is `0/2.125` and rotation is zero.
`camera2d-eye-lissajous-slow-y.json` covers the opposite frequency
ordering. All four golden PAR files were generated with ParAnimator.

Numeric scalar tracks with two authored keys support `base`, `omit`,
`cycle`, and `ping-pong` extrapolation. Integer, double, and numeric
integer-or-enum tracks retain their keys, destination interpolation,
catalog policy, and complete recipes. Integer-or-enum endpoints must be
JSON integers, not numeric strings. Linear, hold, and step curves are
supported; unsupported forms receive indexed importer diagnostics.

`base` captures the original PAR source value outside the key range.
`omit` produces an absent value and a visible gap, not zero. `cycle`
includes both endpoint frames in its period and holds the last value
through the final frame interval. `ping-pong` reflects across the key
span without repeating endpoint frames. Continuous queries retain exact
tick periods and fractional values; integer output rounding is pending.

Open `maxiter-cycle.json`: frame 0 is `300` and frame 4 is `100`.
Add `beat-keys/rms.beat-keys.json` for five comparison lanes. Open
`maxiter-omit.json` to replace them with one lane; frames outside its
authored keys have no numeric value or connecting line. The four
`maxiter-*` fixtures and their golden PAR files come from ParAnimator.

Open `extrapolation-variants.json` for three lanes with held cycle,
double base, and numeric integer-or-enum ping-pong policies. Frame 0
shows `300`, `1.25`, and `200`; frame 4 shows `100`, `1.25`, and `200`.
Its golden PAR file was generated with ParAnimator. Select an
extrapolated segment, then open `extrapolation-invalid.json` to verify
that rejection preserves the document, selection, and playhead.

Other specialized track kinds and parameter-specific output
quantization remain unsupported.
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
by coordinate pairs. Swatches include rectangle bounds and RGB components.
Strings escape quotes, backslashes, control bytes,
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
