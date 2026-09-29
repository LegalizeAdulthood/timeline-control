#pragma once

#include <timeline/Lane.h>

#include <optional>
#include <string>
#include <vector>

namespace timeline
{

/// Human-facing information carried by a timeline document.
///
/// Metadata describes the document without affecting timeline coordinates,
/// validation, or lane contents.
///
class Metadata
{
public:
    Metadata() = default;
    Metadata(std::string title, std::string description);

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

/// Identifies one input used to produce generated timeline content.
///
/// A source reference retains the input's semantic role separately from its
/// source-format location.
///
class SourceReference
{
public:
    SourceReference(std::string role, std::string location);

    const std::string &role() const
    {
        return m_role;
    }
    const std::string &location() const
    {
        return m_location;
    }

private:
    std::string m_role;
    std::string m_location;
};

/// Associates a named generated-content group with its item count.
///
/// Named counts preserve source-defined grouping without materializing the
/// grouped timeline content.
///
class NamedCount
{
public:
    NamedCount(std::string name, int count);

    const std::string &name() const
    {
        return m_name;
    }
    int count() const
    {
        return m_count;
    }

private:
    std::string m_name;
    int m_count;
};

/// Provenance and grouping facts for generated timeline content.
///
/// A generation summary identifies the producer and its inputs, then reports
/// generated items grouped by output target and semantic source.
///
class GenerationSummary
{
public:
    GenerationSummary(std::string generator_name, std::string generator_version,
        std::vector<SourceReference> source_references, std::vector<NamedCount> target_counts,
        std::vector<NamedCount> source_counts);

    const std::string &generator_name() const
    {
        return m_generator_name;
    }
    const std::string &generator_version() const
    {
        return m_generator_version;
    }
    const std::vector<SourceReference> &source_references() const
    {
        return m_source_references;
    }
    const std::vector<NamedCount> &target_counts() const
    {
        return m_target_counts;
    }
    const std::vector<NamedCount> &source_counts() const
    {
        return m_source_counts;
    }

private:
    std::string m_generator_name;
    std::string m_generator_version;
    std::vector<SourceReference> m_source_references;
    std::vector<NamedCount> m_target_counts;
    std::vector<NamedCount> m_source_counts;
};

/// Shallow source-format facts retained before timeline lanes are materialized.
///
/// A source summary identifies the imported schema, counts authored records,
/// and preserves optional frame, time, and synchronization extents in exact
/// core units.
///
class SourceSummary
{
public:
    SourceSummary(std::string schema, int schema_version, int feature_count, int event_count,
        std::optional<Ticks> first_frame = std::nullopt, std::optional<Ticks> last_frame = std::nullopt,
        std::optional<Time> first_time = std::nullopt, std::optional<Time> last_time = std::nullopt,
        std::optional<Duration> frame_offset = std::nullopt,
        std::optional<GenerationSummary> generation_summary = std::nullopt);

    const std::string &schema() const
    {
        return m_schema;
    }
    int schema_version() const
    {
        return m_schema_version;
    }
    int feature_count() const
    {
        return m_feature_count;
    }
    int event_count() const
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
    const std::optional<Time> &first_time() const
    {
        return m_first_time;
    }
    const std::optional<Time> &last_time() const
    {
        return m_last_time;
    }
    const std::optional<Duration> &frame_offset() const
    {
        return m_frame_offset;
    }
    const std::optional<GenerationSummary> &generation_summary() const
    {
        return m_generation_summary;
    }

private:
    std::string m_schema;
    int m_schema_version;
    int m_feature_count;
    int m_event_count;
    std::optional<Ticks> m_first_frame;
    std::optional<Ticks> m_last_frame;
    std::optional<Time> m_first_time;
    std::optional<Time> m_last_time;
    std::optional<Duration> m_frame_offset;
    std::optional<GenerationSummary> m_generation_summary;
};

/// Root model for timeline content and document-level metadata.
///
/// A document owns the timeline timebase, preserves metadata and optional
/// source-format facts, and exposes the lane collection. An empty document is
/// valid and contains zero lanes.
///
class Document
{
public:
    explicit Document(Ticks ticks_per_second, Metadata metadata = Metadata{});
    explicit Document(Timebase timebase, Metadata metadata = Metadata{});
    Document(Timebase timebase, SourceSummary source_summary, Metadata metadata = Metadata{});
    Document(Timebase timebase, SourceSummary source_summary, int track_count, int keyframe_count,
        Metadata metadata = Metadata{});
    Document(FrameGrid frame_grid, int track_count, int keyframe_count, Metadata metadata = Metadata{});
    Document(FrameGrid frame_grid, SourceSummary source_summary, Metadata metadata = Metadata{});

    const Timebase &timebase() const
    {
        return m_timebase;
    }
    const Metadata &metadata() const
    {
        return m_metadata;
    }
    const std::optional<FrameGrid> &frame_grid() const
    {
        return m_frame_grid;
    }
    const std::optional<SourceSummary> &source_summary() const
    {
        return m_source_summary;
    }
    const std::vector<Lane> &lanes() const
    {
        return m_lanes;
    }
    std::optional<Time> content_start() const;
    std::optional<Time> content_end() const;
    void add_lane(Lane lane);
    int track_count() const
    {
        return m_track_count;
    }
    int keyframe_count() const
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
    int lane_count() const
    {
        return size_cast(m_lanes);
    }

private:
    Timebase m_timebase;
    Metadata m_metadata;
    std::optional<FrameGrid> m_frame_grid;
    std::optional<SourceSummary> m_source_summary;
    std::vector<Lane> m_lanes;
    int m_track_count{0};
    int m_keyframe_count{0};
};

} // namespace timeline
