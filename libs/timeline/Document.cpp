// Copyright (c) 2026 Richard Thomson

#include <timeline/Document.h>

#include <algorithm>
#include <limits>
#include <set>
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

DocumentBuilder::DocumentBuilder(Document document) :
    DocumentBuilder(document, document.strings())
{
}

DocumentBuilder::DocumentBuilder(Document document, const StringTable &strings) :
    m_document(std::move(document)),
    m_strings(strings)
{
}

void DocumentBuilder::add_lane(Lane lane)
{
    if (!m_strings.contains(lane.id()))
    {
        throw std::invalid_argument("timeline lane ID is not present in the document string table");
    }
    for (const Item &item : lane.items())
    {
        std::visit(
            [this](const auto &value)
            {
                if (!m_strings.contains(value.id()))
                {
                    throw std::invalid_argument("timeline item ID is not present in the document string table");
                }
            },
            item);
    }
    m_document.add_lane(std::move(lane));
}

void DocumentBuilder::add_lane(Lane lane, const StringTable &item_strings)
{
    for (Item &item : lane.m_items)
    {
        std::visit([this, &item_strings](auto &value) { value.m_id = intern(item_strings.lookup(value.id())); }, item);
    }
    add_lane(std::move(lane));
}

Document DocumentBuilder::build() &&
{
    m_document.m_strings = std::move(m_strings).build();
    return std::move(m_document);
}

Document combine_documents(const Document &document, const Document &addition)
{
    if (!document.frame_grid() || !addition.frame_grid() ||
        document.timebase().ticks_per_second() != addition.timebase().ticks_per_second() ||
        document.frame_grid()->frame_duration() != addition.frame_grid()->frame_duration())
    {
        throw std::invalid_argument("combined timelines require matching timebases and frame rates");
    }
    if (addition.track_count() > std::numeric_limits<int>::max() - document.track_count() ||
        addition.keyframe_count() > std::numeric_limits<int>::max() - document.keyframe_count())
    {
        throw std::overflow_error("combined timeline counts are too large");
    }
    const FrameGrid &original = *document.frame_grid();
    const Time start = std::min(original.offset(), addition.frame_grid()->offset());
    const Time end = std::max(original.end_time(), addition.frame_grid()->end_time());
    const Ticks duration = (end - start).ticks();
    const Ticks frame_ticks = original.frame_duration().ticks();
    const Ticks frame_count = duration / frame_ticks + (duration % frame_ticks != 0 ? 1 : 0);
    const FrameGrid grid(document.timebase(), frame_count, original.frames_per_second_numerator(),
        original.frames_per_second_denominator(), start);
    const Metadata metadata(document.metadata().title() + " + " + addition.metadata().title(),
        document.metadata().description() + "\n" + addition.metadata().description());
    const int track_count = document.track_count() + addition.track_count();
    const int keyframe_count = document.keyframe_count() + addition.keyframe_count();
    Document combined = document.source_summary()
        ? Document(grid, *document.source_summary(), track_count, keyframe_count, metadata)
        : Document(grid, track_count, keyframe_count, metadata);
    DocumentBuilder builder(std::move(combined), document.strings());
    std::set<std::string> lane_ids;
    for (const Lane &lane : document.lanes())
    {
        lane_ids.emplace(document.strings().lookup(lane.id()));
        builder.add_lane(lane);
    }
    int index = document.lane_count();
    for (const Lane &lane : addition.lanes())
    {
        std::string id;
        do
        {
            id = "added-" + std::to_string(index++) + "-" + std::string(addition.strings().lookup(lane.id()));
        } while (!lane_ids.emplace(id).second);
        Lane copy(builder.intern(id), lane.label(), lane.kind(), lane.start(), lane.end());
        for (const Item &item : lane.items())
        {
            std::visit([&copy](const auto &value) { copy.add(value); }, item);
        }
        if (document.strings().shares_storage_with(addition.strings()))
        {
            builder.add_lane(std::move(copy));
        }
        else
        {
            builder.add_lane(std::move(copy), addition.strings());
        }
    }
    return std::move(builder).build();
}

} // namespace timeline
