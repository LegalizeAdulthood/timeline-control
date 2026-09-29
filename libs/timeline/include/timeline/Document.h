#pragma once

#include <timeline/Time.h>

#include <cstddef>
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

/// Root model for timeline content and document-level metadata.
///
/// A document owns the timeline timebase, preserves metadata, and exposes the
/// lane collection. An empty document is valid and contains zero lanes.
///
class TimelineDocument
{
public:
    explicit TimelineDocument(Ticks ticks_per_second, TimelineMetadata metadata = TimelineMetadata{});
    explicit TimelineDocument(Timebase timebase, TimelineMetadata metadata = TimelineMetadata{});

    const Timebase &timebase() const
    {
        return m_timebase;
    }
    const TimelineMetadata &metadata() const
    {
        return m_metadata;
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
};

} // namespace timeline
