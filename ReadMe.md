[![CMake workflow](https://github.com/LegalizeAdulthood/timeline-control/actions/workflows/cmake.yml/badge.svg)](https://github.com/LegalizeAdulthood/timeline-control/actions/workflows/cmake.yml)

# Animation Timeline Control

This project applies
[hexagonal architecture](https://en.wikipedia.org/wiki/Hexagonal_architecture_(software)),
also known as ports and adapters, to a reusable animation timeline
control.
`timeline-core` sits inside the hexagon and owns timeline mechanism: the
document model, queries, navigation, interaction, layout, hit testing,
and display-list generation. Its APIs and display-list contract are the
ports through which the surrounding application uses the control. The
core depends on neither a UI framework nor an application file format.

The wxWidgets, Dear ImGui, and Qt libraries are presentation adapters.
They translate native input into core interaction, supply font metrics,
DPI-aware geometry, and semantic theme values, and render the resulting
display list with toolkit primitives. Timeline behavior therefore remains
shared and testable while each adapter retains native drawing and event
handling.

The JSON adapters bring application semantics across the other side of the
hexagon. They interpret authored animation tracks from
[ParAnimator](https://github.com/LegalizeAdulthood/paranimator) and music
analysis and beat-key mappings from
[par-beatdown](https://github.com/LegalizeAdulthood/par-beatdown), then
produce the toolkit-neutral core document. The viewer applications are
composition roots that connect these format adapters to a selected
presentation adapter; neither concern leaks into `timeline-core`.

# Documentation

- [Timeline Core Library](libs/timeline/ReadMe.md)
- [ParAnimator Timeline Adapter](libs/timelineParAnimator/ReadMe.md)
- [wxWidgets Timeline Adapter](libs/wxTimeline/ReadMe.md)
- [Dear ImGui Timeline Adapter](libs/imguiTimeline/ReadMe.md)
- [Qt Timeline Adapter](libs/qtTimeline/ReadMe.md)

# Obtaining the Source

Use git to clone this repository, then update the vcpkg submodule to
bootstrap the dependency process.

```
git clone https://github.com/LegalizeAdulthood/timeline-control
cd timeline-control
git submodule init
git submodule update --depth 1
```

# Building

The default workflow configures, builds, and tests the wxWidgets control
and viewer:

```text
cmake --workflow --preset default
```

| CMake Option Name | Default Option Value | Workflow Preset For Option |
| --- | --- | --- |
| `TIMELINE_CONTROL_WITH_WX` | `ON` | `default` |
| `TIMELINE_CONTROL_WITH_IMGUI` | `OFF` | `default-imgui` |
| `TIMELINE_CONTROL_WITH_QT` | `OFF` | `default-qt` |
| `TIMELINE_CONTROL_WITH_CAIRO` | `OFF` | `default-cairo` |

Hidden `*-on` and `*-off` configure presets enable or disable each option.
The Cairo option requires the wxWidgets option.

Each workflow performs configure, build, and test steps and writes its
output to a sibling directory. `default-cairo` enables both Cairo and
wxWidgets and builds the native `wx-timeline-viewer` alongside
`cairo-timeline-viewer`. On headless Linux, run
`xvfb-run -a cmake --workflow --preset default-cairo`.

# wxWidgets Timeline Viewer

Run `wx-timeline-viewer` from the build's `tools/timeline-viewer`
directory, with the configuration subdirectory on multi-configuration
generators.
File > Open loads supported ParAnimator and par-beatdown JSON through the
format adapters.
File > Add combines a second document using the displayed timeline's
timebase and frame rate, including companion `adapter.beat-keys.json`
mappings when present. Import and composition failures retain the
displayed document and interaction state; the viewer displays
importer-owned diagnostics.

The inspector shows document metadata, mapping recipes, hovered
identities, selection, playhead, ranges, frame samples, parameter outputs,
attributes, and palettes. View commands delegate zoom, fit, and
clear-selection operations to the control. File > Export Snapshot writes
the current core display list as text. With Cairo enabled,
`wx-timeline-viewer` remains the native wx executable and
`cairo-timeline-viewer` starts with antialiased rendering, so both can run
side by side. The Cairo viewer adds a View > Renderer menu for switching
renderers without changing the control state.

# ImGui Timeline Viewer

Run `imgui-timeline-viewer` from the build's `tools/imgui-timeline-viewer`
directory, under `Debug` or `Release` for multi-configuration generators.
An optional JSON path opens a document at startup.

File > Open uses a native JSON file dialog. File > Add comparison loads
music, beat-keys mappings, or animation using the current document's frame
rate. Companion `adapter.beat-keys.json` files are discovered beside the
selected input. Failed imports retain the displayed document and state;
diagnostics are shown by the viewer, never by the control.

The inspector shows metadata, mapping recipes, hover identities,
selection, playhead, ranges, and frame samples. View commands provide
zoom, fit, and clear selection. File > Export Snapshot writes the current
core display list as text. All viewers use the same adapters and core;
font metrics and theme colors remain toolkit-specific.

Viewer tests include a bounded SDL dummy-video/software-renderer smoke
check, requiring no desktop or GPU. CI runs all four public workflows.

# Qt Viewer

Run `qt-timeline-viewer` from the build's `tools/qt-timeline-viewer`
directory, with the configuration subdirectory on Windows. An optional
JSON path opens a document at startup. The build deploys the native Qt
platform plugin beside the viewer executable.
File > Open loads supported ParAnimator and ParBeatdown JSON through
the existing adapters, including discovered beat-keys companion mappings.
File > Add comparison loads animation, source music, or generated
beat-keys output using the displayed document's timebase and frame rate.
Companion mapping configurations retain their source timing semantics;
incompatible documents are rejected by core composition. File > Export
Snapshot writes the current core display list. Import and write
diagnostics belong to the viewer; failed operations retain the displayed
document and interaction state. Opening a replacement clears prior
mapping recipes.

The inspector shows document/source metadata, generation summaries,
mapping recipes, hovered identities, selection, frame samples, parameter
outputs, attributes, and palettes. View-menu commands delegate zoom, fit,
and clear-selection operations to the control.

[Utah C++ Programmers](https://meetup.com/utah-cpp-programmers)\
[Past Topics](https://utahcpp.wordpress.com/past-meeting-topics/)\
[Future Topics](https://utahcpp.wordpress.com/future-meeting-topics/)
