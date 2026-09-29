#pragma once

#include <timeline/Time.h>

#include <cstddef>
#include <optional>
#include <string>

namespace timeline
{

/// Human-facing information carried by a timeline document.
///
/// Metadata describes the document without affecting timeline coordinates,
/// validation, or lane contents.
///
class TimelineMetadata
{
public:
    TimelineMetadata() = default;
    TimelineMetadata(std::string title, std::string description);

    const std::string &title() const
    {
        return m_title;
    }
    const std::string &description() const
    {
        return m_description;
    }

private:
    std::string m_title;
    std::string m_description;
};

/// Shallow source-format facts retained before timeline lanes are materialized.
///
/// A source summary identifies the imported schema, counts authored records,
/// and preserves optional frame, time, and synchronization extents in exact
/// core units.
///
class TimelineSourceSummary
{
public:
    TimelineSourceSummary(std::string schema, std::size_t schema_version, std::size_t feature_count,
        std::size_t event_count, std::optional<Ticks> first_frame = std::nullopt,
        std::optional<Ticks> last_frame = std::nullopt, std::optional<TimelineTime> first_time = std::nullopt,
        std::optional<TimelineTime> last_time = std::nullopt,
        std::optional<TimelineDuration> frame_offset = std::nullopt);

    const std::string &schema() const
    {
        return m_schema;
    }
    std::size_t schema_version() const
    {
        return m_schema_version;
    }
    std::size_t feature_count() const
    {
        return m_feature_count;
    }
    std::size_t event_count() const
    {
        return m_event_count;
    }
    const std::optional<Ticks> &first_frame() const
    {
        return m_first_frame;
    }
    const std::optional<Ticks> &last_frame() const
    {
        return m_last_frame;
    }
    const std::optional<TimelineTime> &first_time() const
    {
        return m_first_time;
    }
    const std::optional<TimelineTime> &last_time() const
    {
        return m_last_time;
    }
    const std::optional<TimelineDuration> &frame_offset() const
    {
        return m_frame_offset;
    }

private:
    std::string m_schema;
    std::size_t m_schema_version;
    std::size_t m_feature_count;
    std::size_t m_event_count;
    std::optional<Ticks> m_first_frame;
    std::optional<Ticks> m_last_frame;
    std::optional<TimelineTime> m_first_time;
    std::optional<TimelineTime> m_last_time;
    std::optional<TimelineDuration> m_frame_offset;
};

/// Root model for timeline content and document-level metadata.
///
/// A document owns the timeline timebase, preserves metadata and optional
/// source-format facts, and exposes the lane collection. An empty document is
/// valid and contains zero lanes.
///
class TimelineDocument
{
public:
    explicit TimelineDocument(Ticks ticks_per_second, TimelineMetadata metadata = TimelineMetadata{});
    explicit TimelineDocument(Timebase timebase, TimelineMetadata metadata = TimelineMetadata{});
    TimelineDocument(
        Timebase timebase, TimelineSourceSummary source_summary, TimelineMetadata metadata = TimelineMetadata{});
    TimelineDocument(FrameGrid frame_grid, std::size_t track_count, std::size_t keyframe_count,
        TimelineMetadata metadata = TimelineMetadata{});
    TimelineDocument(
        FrameGrid frame_grid, TimelineSourceSummary source_summary, TimelineMetadata metadata = TimelineMetadata{});

    const Timebase &timebase() const
    {
        return m_timebase;
    }
    const TimelineMetadata &metadata() const
    {
        return m_metadata;
    }
    const std::optional<FrameGrid> &frame_grid() const
    {
        return m_frame_grid;
    }
    const std::optional<TimelineSourceSummary> &source_summary() const
    {
        return m_source_summary;
    }
    std::size_t track_count() const
    {
        return m_track_count;
    }
    std::size_t keyframe_count() const
    {
        return m_keyframe_count;
    }
    bool is_valid() const
    {
        return m_timebase.ticks_per_second() > 0;
    }
    bool lanes_empty() const
    {
        return lane_count() == 0;
    }
    std::size_t lane_count() const
    {
        return 0;
    }

private:
    Timebase m_timebase;
    TimelineMetadata m_metadata;
    std::optional<FrameGrid> m_frame_grid;
    std::optional<TimelineSourceSummary> m_source_summary;
    std::size_t m_track_count{0};
    std::size_t m_keyframe_count{0};
};

} // namespace timeline
