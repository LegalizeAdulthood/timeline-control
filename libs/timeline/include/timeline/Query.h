// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/Document.h>

#include <optional>
#include <string>
#include <vector>

namespace timeline
{

/// Generic timeline item category returned by read-only queries.
enum class InspectionItemType
{
    INSTANT,
    INTERVAL,
    ENVELOPE,
    CURVE,
    KEYFRAME
};

/// Relationship between a returned item and the inspected time or range.
enum class InspectionItemRole
{
    ACTIVE,
    SAMPLED,
    BEFORE,
    AFTER,
    EXACT
};

/// Stable item identity and optional numeric value returned by a query.
///
/// Numeric values contain strengths for event-like items, samples for curves,
/// and authored values for keyframes.
///
struct InspectionItem
{
    std::string id;
    std::string kind;
    InspectionItemType type;
    InspectionItemRole role;
    std::optional<double> value;
    Attributes attributes;
};

/// Lane summary and matching items returned by a timeline query.
///
/// Every queried lane is represented, including lanes with no matching items.
///
struct LaneInspection
{
    std::string id;
    std::string label;
    std::string kind;
    int item_count;
    std::vector<InspectionItem> items;
    /// Evaluated numeric keyframe signal at a frame query's exact time.
    std::optional<double> value;
};

/// Read-only snapshot of one frame across every document lane.
///
/// The inspection time is the exact start of the selected frame.
///
struct FrameInspection
{
    Ticks frame;
    Time time;
    std::vector<LaneInspection> lanes;
};

/// Read-only snapshot of one inclusive time range across every document lane.
///
/// Matching items retain their stable IDs and are grouped by lane.
///
struct RangeInspection
{
    Time start;
    Time end;
    std::vector<LaneInspection> lanes;
};

/// Inspects one frame, or returns no value when the document has no matching frame.
std::optional<FrameInspection> inspect_frame(const Document &document, Ticks frame);

/// Inspects all document lanes over an inclusive time range.
RangeInspection inspect_range(const Document &document, Time start, Time end);

} // namespace timeline
