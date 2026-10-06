# Timeline Viewer I/O

`timeline-viewer-io` contains presentation-neutral file workflows shared
by the runnable timeline viewers. It depends on `timeline-paranimator` and
`timeline-core`, but has no GUI framework dependency.

## Timeline Loading

`load_timeline` accepts a JSON path, append intent, the current optional
document, and the current ParAnimator mappings. It discovers a sibling
`adapter.beat-keys.json`, inherits timing from an appended document,
imports the selected file, and composes compatible documents.

A successful `LoadResult` owns the candidate replacement document and
complete mapping set. The viewer moves that document into its control, so
the control remains the displayed model's owner. Failed results preserve
the caller's displayed state by omitting replacement data, and distinguish
import failure from composition failure.

## Snapshot Writing

`write_snapshot` writes already-rendered snapshot text in binary mode and
returns file diagnostics without toolkit strings or dialogs. Each viewer
remains responsible for deciding whether a snapshot is available and for
presenting the result.
