#include <timeline/Document.h>

#include <stdexcept>
#include <utility>

namespace timeline
{
namespace
{

void validate_document_counts(int track_count, int keyframe_count)
{
    if (track_count < 0 || keyframe_count < 0)
    {
        throw std::invalid_argument("timeline document counts cannot be negative");
    }
}

} // namespace

Metadata::Metadata(std::string title, std::string description) :
    m_title(std::move(title)),
    m_description(std::move(description))
{
}

SourceReference::SourceReference(std::string role, std::string location) :
    m_role(std::move(role)),
    m_location(std::move(location))
{
    if (m_role.empty() || m_location.empty())
    {
        throw std::invalid_argument("timeline source references require a role and location");
    }
}

NamedCount::NamedCount(std::string name, int count) :
    m_name(std::move(name)),
    m_count(count)
{
    if (m_name.empty() || m_count < 0)
    {
        throw std::invalid_argument("timeline named counts require a name and nonnegative count");
    }
}

GenerationSummary::GenerationSummary(std::string generator_name, std::string generator_version,
    std::vector<SourceReference> source_references, std::vector<NamedCount> target_counts,
    std::vector<NamedCount> source_counts) :
    m_generator_name(std::move(generator_name)),
    m_generator_version(std::move(generator_version)),
    m_source_references(std::move(source_references)),
    m_target_counts(std::move(target_counts)),
    m_source_counts(std::move(source_counts))
{
    if (m_generator_name.empty() || m_generator_version.empty())
    {
        throw std::invalid_argument("timeline generation summaries require a generator name and version");
    }
}

SourceSummary::SourceSummary(std::string schema, int schema_version, int feature_count, int event_count) :
    SourceSummary(std::move(schema), schema_version, feature_count, event_count, std::nullopt, std::nullopt,
        std::nullopt, std::nullopt, std::nullopt, std::nullopt)
{
}

SourceSummary::SourceSummary(std::string schema, int schema_version, int feature_count, int event_count,
    std::optional<Ticks> first_frame, std::optional<Ticks> last_frame, std::optional<Time> first_time,
    std::optional<Time> last_time, std::optional<Duration> frame_offset,
    std::optional<GenerationSummary> generation_summary) :
    m_schema(std::move(schema)),
    m_schema_version(schema_version),
    m_feature_count(feature_count),
    m_event_count(event_count),
    m_first_frame(first_frame),
    m_last_frame(last_frame),
    m_first_time(first_time),
    m_last_time(last_time),
    m_frame_offset(frame_offset),
    m_generation_summary(std::move(generation_summary))
{
    if (m_schema.empty())
    {
        throw std::invalid_argument("timeline source schema cannot be empty");
    }
    if (m_schema_version < 0 || m_feature_count < 0 || m_event_count < 0)
    {
        throw std::invalid_argument("timeline source summary counts cannot be negative");
    }
    if (m_first_frame.has_value() != m_last_frame.has_value())
    {
        throw std::invalid_argument("timeline source frame extent must have both endpoints");
    }
    if (m_first_frame && *m_last_frame < *m_first_frame)
    {
        throw std::invalid_argument("timeline source frame extent is reversed");
    }
    if (m_first_time.has_value() != m_last_time.has_value())
    {
        throw std::invalid_argument("timeline source time extent must have both endpoints");
    }
    if (m_first_time && *m_last_time < *m_first_time)
    {
        throw std::invalid_argument("timeline source time extent is reversed");
    }
}

Document::Document(Ticks ticks_per_second) :
    Document(ticks_per_second, Metadata{})
{
}

Document::Document(Ticks ticks_per_second, Metadata metadata) :
    Document(Timebase(ticks_per_second), std::move(metadata))
{
}

Document::Document(Timebase timebase) :
    Document(timebase, Metadata{})
{
}

Document::Document(Timebase timebase, Metadata metadata) :
    m_timebase(timebase),
    m_metadata(std::move(metadata))
{
}

Document::Document(Timebase timebase, SourceSummary source_summary) :
    Document(timebase, std::move(source_summary), Metadata{})
{
}

Document::Document(Timebase timebase, SourceSummary source_summary, Metadata metadata) :
    m_timebase(timebase),
    m_metadata(std::move(metadata)),
    m_source_summary(std::move(source_summary))
{
}

Document::Document(Timebase timebase, SourceSummary source_summary, int track_count, int keyframe_count) :
    Document(timebase, std::move(source_summary), track_count, keyframe_count, Metadata{})
{
}

Document::Document(
    Timebase timebase, SourceSummary source_summary, int track_count, int keyframe_count, Metadata metadata) :
    m_timebase(timebase),
    m_metadata(std::move(metadata)),
    m_source_summary(std::move(source_summary)),
    m_track_count(track_count),
    m_keyframe_count(keyframe_count)
{
    validate_document_counts(m_track_count, m_keyframe_count);
}

Document::Document(FrameGrid frame_grid, int track_count, int keyframe_count) :
    Document(std::move(frame_grid), track_count, keyframe_count, Metadata{})
{
}

Document::Document(FrameGrid frame_grid, int track_count, int keyframe_count, Metadata metadata) :
    m_timebase(frame_grid.timebase()),
    m_metadata(std::move(metadata)),
    m_frame_grid(std::move(frame_grid)),
    m_track_count(track_count),
    m_keyframe_count(keyframe_count)
{
    validate_document_counts(m_track_count, m_keyframe_count);
}

Document::Document(FrameGrid frame_grid, SourceSummary source_summary) :
    Document(std::move(frame_grid), std::move(source_summary), Metadata{})
{
}

Document::Document(FrameGrid frame_grid, SourceSummary source_summary, Metadata metadata) :
    m_timebase(frame_grid.timebase()),
    m_metadata(std::move(metadata)),
    m_frame_grid(std::move(frame_grid)),
    m_source_summary(std::move(source_summary))
{
}

Document::Document(FrameGrid frame_grid, SourceSummary source_summary, int track_count, int keyframe_count) :
    Document(std::move(frame_grid), std::move(source_summary), track_count, keyframe_count, Metadata{})
{
}

Document::Document(
    FrameGrid frame_grid, SourceSummary source_summary, int track_count, int keyframe_count, Metadata metadata) :
    m_timebase(frame_grid.timebase()),
    m_metadata(std::move(metadata)),
    m_frame_grid(std::move(frame_grid)),
    m_source_summary(std::move(source_summary)),
    m_track_count(track_count),
    m_keyframe_count(keyframe_count)
{
    validate_document_counts(m_track_count, m_keyframe_count);
}

std::optional<Time> Document::content_start() const
{
    if (m_frame_grid)
    {
        return m_frame_grid->offset();
    }
    if (m_lanes.empty())
    {
        return std::nullopt;
    }

    Time result = m_lanes.front().start();
    for (const Lane &lane : m_lanes)
    {
        if (lane.start() < result)
        {
            result = lane.start();
        }
    }
    return result;
}

std::optional<Time> Document::content_end() const
{
    if (m_frame_grid)
    {
        return m_frame_grid->end_time();
    }
    if (m_lanes.empty())
    {
        return std::nullopt;
    }

    Time result = m_lanes.front().end();
    for (const Lane &lane : m_lanes)
    {
        if (result < lane.end())
        {
            result = lane.end();
        }
    }
    return result;
}

void Document::add_lane(Lane lane)
{
    if (m_frame_grid && (lane.start() < m_frame_grid->offset() || m_frame_grid->end_time() < lane.end()))
    {
        throw std::out_of_range("timeline lane is outside its document frame grid");
    }
    m_lanes.push_back(std::move(lane));
}

} // namespace timeline
