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

Procedural paths, PWM, specialized track kinds, non-clamp extrapolation,
and parameter-specific output quantization remain outside the supported
realized-track subset. Unsupported tracks receive importer diagnostics.
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
