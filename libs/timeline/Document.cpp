#include <timeline/Document.h>

#include <stdexcept>
#include <utility>

namespace timeline
{

Metadata::Metadata(std::string title, std::string description) :
    m_title(std::move(title)),
    m_description(std::move(description))
{
}

SourceSummary::SourceSummary(std::string schema, std::size_t schema_version, std::size_t feature_count,
    std::size_t event_count, std::optional<Ticks> first_frame, std::optional<Ticks> last_frame,
    std::optional<Time> first_time, std::optional<Time> last_time, std::optional<Duration> frame_offset) :
    m_schema(std::move(schema)),
    m_schema_version(schema_version),
    m_feature_count(feature_count),
    m_event_count(event_count),
    m_first_frame(first_frame),
    m_last_frame(last_frame),
    m_first_time(first_time),
    m_last_time(last_time),
    m_frame_offset(frame_offset)
{
    if (m_schema.empty())
    {
        throw std::invalid_argument("timeline source schema cannot be empty");
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

Document::Document(Ticks ticks_per_second, Metadata metadata) :
    Document(Timebase(ticks_per_second), std::move(metadata))
{
}

Document::Document(Timebase timebase, Metadata metadata) :
    m_timebase(timebase),
    m_metadata(std::move(metadata))
{
}

Document::Document(Timebase timebase, SourceSummary source_summary, Metadata metadata) :
    m_timebase(timebase),
    m_metadata(std::move(metadata)),
    m_source_summary(std::move(source_summary))
{
}

Document::Document(FrameGrid frame_grid, std::size_t track_count, std::size_t keyframe_count, Metadata metadata) :
    m_timebase(frame_grid.timebase()),
    m_metadata(std::move(metadata)),
    m_frame_grid(std::move(frame_grid)),
    m_track_count(track_count),
    m_keyframe_count(keyframe_count)
{
}

Document::Document(FrameGrid frame_grid, SourceSummary source_summary, Metadata metadata) :
    m_timebase(frame_grid.timebase()),
    m_metadata(std::move(metadata)),
    m_frame_grid(std::move(frame_grid)),
    m_source_summary(std::move(source_summary))
{
}

} // namespace timeline
