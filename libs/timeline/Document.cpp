#include <timeline/Document.h>

#include <utility>

namespace timeline
{

TimelineMetadata::TimelineMetadata(std::string title, std::string description) :
    m_title(std::move(title)),
    m_description(std::move(description))
{
}

TimelineDocument::TimelineDocument(Ticks ticks_per_second, TimelineMetadata metadata) :
    TimelineDocument(Timebase(ticks_per_second), std::move(metadata))
{
}

TimelineDocument::TimelineDocument(Timebase timebase, TimelineMetadata metadata) :
    m_timebase(timebase),
    m_metadata(std::move(metadata))
{
}

TimelineDocument::TimelineDocument(
    FrameGrid frame_grid, std::size_t track_count, std::size_t keyframe_count, TimelineMetadata metadata) :
    m_timebase(frame_grid.timebase()),
    m_metadata(std::move(metadata)),
    m_frame_grid(std::move(frame_grid)),
    m_track_count(track_count),
    m_keyframe_count(keyframe_count)
{
}

} // namespace timeline
