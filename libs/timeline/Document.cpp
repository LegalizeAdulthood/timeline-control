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

} // namespace timeline
