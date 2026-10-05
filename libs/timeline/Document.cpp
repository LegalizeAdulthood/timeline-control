// Copyright (c) 2026 Richard Thomson

#include <timeline/Document.h>

#include <algorithm>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
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

void validate_generation_strings(const GenerationSummary &summary, const StringTableBuilder &strings)
{
    if (!strings.contains(summary.generator_name()) || !strings.contains(summary.generator_version()))
    {
        throw std::invalid_argument(
            "timeline generation summary string ID is not present in the document string table");
    }
    for (const SourceReference &source : summary.source_references())
    {
        if (!strings.contains(source.role()) || !strings.contains(source.location()))
        {
            throw std::invalid_argument(
                "timeline source reference string ID is not present in the document string table");
        }
    }
    for (const NamedCount &count : summary.target_counts())
    {
        if (!strings.contains(count.name()))
        {
            throw std::invalid_argument("timeline named count string ID is not present in the document string table");
        }
    }
    for (const NamedCount &count : summary.source_counts())
    {
        if (!strings.contains(count.name()))
        {
            throw std::invalid_argument("timeline named count string ID is not present in the document string table");
        }
    }
}

void validate_document_strings(const Document &document, const StringTableBuilder &strings)
{
    if (!strings.contains(document.metadata().title()) || !strings.contains(document.metadata().description()))
    {
        throw std::invalid_argument("timeline metadata string ID is not present in the document string table");
    }
    if (!document.source_summary())
    {
        return;
    }
    if (!strings.contains(document.source_summary()->schema()))
    {
        throw std::invalid_argument("timeline source schema string ID is not present in the document string table");
    }
    if (document.source_summary()->generation_summary())
    {
        validate_generation_strings(*document.source_summary()->generation_summary(), strings);
    }
}

} // namespace

std::string to_string(const Document &document)
{
    std::ostringstream text;
    text.imbue(std::locale::classic());
    text << "Title: " << document.strings().lookup(document.metadata().title())
         << "\nValid: " << (document.is_valid() ? "yes" : "no")
         << "\nTicks per second: " << document.timebase().ticks_per_second();
    if (document.frame_grid())
    {
        const FrameGrid &grid = *document.frame_grid();
        text << "\nFrames: " << grid.frame_count() << "\nFrame rate: " << grid.frames_per_second_numerator() << '/'
             << grid.frames_per_second_denominator() << " fps";
    }
    if (document.source_summary())
    {
        const SourceSummary &source = *document.source_summary();
        text << "\nSchema: " << document.strings().lookup(source.schema()) << " v" << source.schema_version()
             << "\nFeatures: " << source.feature_count() << "\nEvents: " << source.event_count();
        if (source.generation_summary())
        {
            const GenerationSummary &generation = *source.generation_summary();
            text << "\nGenerator: " << document.strings().lookup(generation.generator_name()) << ' '
                 << document.strings().lookup(generation.generator_version());
            for (const SourceReference &input : generation.source_references())
            {
                text << "\nInput " << document.strings().lookup(input.role()) << ": "
                     << document.strings().lookup(input.location());
            }
            for (const NamedCount &count : generation.target_counts())
            {
                text << "\nTarget " << document.strings().lookup(count.name()) << ": " << count.count();
            }
            for (const NamedCount &count : generation.source_counts())
            {
                text << "\nSource " << document.strings().lookup(count.name()) << ": " << count.count();
            }
        }
        if (source.first_frame())
        {
            text << "\nFrame extent: " << *source.first_frame() << " to " << *source.last_frame();
        }
        if (source.first_time())
        {
            text << "\nTime extent: " << document.timebase().seconds(*source.first_time()) << " to "
                 << document.timebase().seconds(*source.last_time()) << " seconds";
        }
        if (source.frame_offset())
        {
            text << "\nFrame offset: " << document.timebase().seconds(*source.frame_offset()) << " seconds";
        }
    }
    text << "\nTracks: " << document.track_count() << "\nKeyframes: " << document.keyframe_count()
         << "\nLanes: " << document.lane_count()
         << "\nSource: " << document.strings().lookup(document.metadata().description());
    return text.str();
}

Metadata::Metadata(StringId title, StringId description) :
    m_title(title),
    m_description(description)
{
}

SourceReference::SourceReference(StringId role, StringId location) :
    m_role(role),
    m_location(location)
{
    if (m_role.empty() || m_location.empty())
    {
        throw std::invalid_argument("timeline source references require a role and location");
    }
}

NamedCount::NamedCount(StringId name, int count) :
    m_name(name),
    m_count(count)
{
    if (m_name.empty() || m_count < 0)
    {
        throw std::invalid_argument("timeline named counts require a name and nonnegative count");
    }
}

GenerationSummary::GenerationSummary(StringId generator_name, StringId generator_version,
    std::vector<SourceReference> source_references, std::vector<NamedCount> target_counts,
    std::vector<NamedCount> source_counts) :
    m_generator_name(generator_name),
    m_generator_version(generator_version),
    m_source_references(std::move(source_references)),
    m_target_counts(std::move(target_counts)),
    m_source_counts(std::move(source_counts))
{
    if (m_generator_name.empty() || m_generator_version.empty())
    {
        throw std::invalid_argument("timeline generation summaries require a generator name and version");
    }
}

SourceSummary::SourceSummary(StringId schema, int schema_version, int feature_count, int event_count) :
    SourceSummary(schema, schema_version, feature_count, event_count, std::nullopt, std::nullopt, std::nullopt,
        std::nullopt, std::nullopt, std::nullopt)
{
}

SourceSummary::SourceSummary(StringId schema, int schema_version, int feature_count, int event_count,
    std::optional<Ticks> first_frame, std::optional<Ticks> last_frame, std::optional<Time> first_time,
    std::optional<Time> last_time, std::optional<Duration> frame_offset,
    std::optional<GenerationSummary> generation_summary) :
    m_schema(schema),
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
    validate_document_strings(m_document, m_strings);
}

void DocumentBuilder::add_lane(Lane lane)
{
    if (!m_strings.contains(lane.id()) || !m_strings.contains(lane.label()) || !m_strings.contains(lane.kind()))
    {
        throw std::invalid_argument("timeline lane string ID is not present in the document string table");
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
                using Value = std::decay_t<decltype(value)>;
                if constexpr (!std::is_same_v<Value, Keyframe>)
                {
                    if (!m_strings.contains(value.kind()))
                    {
                        throw std::invalid_argument("timeline item kind is not present in the document string table");
                    }
                }
                if constexpr (!std::is_same_v<Value, Keyframe> && !std::is_same_v<Value, PaletteCurve>)
                {
                    if (!m_strings.contains(value.label()))
                    {
                        throw std::invalid_argument("timeline item label is not present in the document string table");
                    }
                }
                for (const Attribute &attribute : value.attributes().values())
                {
                    if (!m_strings.contains(attribute.key()) || !m_strings.contains(attribute.value()))
                    {
                        throw std::invalid_argument(
                            "timeline item attribute is not present in the document string table");
                    }
                }
            },
            item);
    }
    m_document.add_lane(std::move(lane));
}

void DocumentBuilder::append(const Lane &lane, const StringTable &strings, StringId id)
{
    const auto remap_attributes = [this, &strings](const Attributes &attributes)
    {
        std::vector<Attribute> remapped;
        for (const Attribute &attribute : attributes.values())
        {
            remapped.emplace_back(intern(strings.lookup(attribute.key())), intern(strings.lookup(attribute.value())));
        }
        return Attributes(std::move(remapped));
    };
    std::vector<Item> items;
    for (const Item &item : lane.items())
    {
        items.push_back(std::visit(
            [this, &strings, &remap_attributes](const auto &value) -> Item
            {
                using Value = std::decay_t<decltype(value)>;
                const StringId item_id = intern(strings.lookup(value.id()));
                if constexpr (std::is_same_v<Value, Keyframe>)
                {
                    return value.with_id(item_id).with_attributes(remap_attributes(value.attributes()));
                }
                else if constexpr (std::is_same_v<Value, PaletteCurve>)
                {
                    return value.with_id(item_id)
                        .with_kind(intern(strings.lookup(value.kind())))
                        .with_attributes(remap_attributes(value.attributes()));
                }
                else
                {
                    return value.with_id(item_id)
                        .with_strings(intern(strings.lookup(value.kind())), intern(strings.lookup(value.label())))
                        .with_attributes(remap_attributes(value.attributes()));
                }
            },
            item));
    }
    add_lane(lane.with_id(id).with_strings(
        intern(strings.lookup(lane.label())), intern(strings.lookup(lane.kind())), std::move(items)));
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
    StringTableBuilder strings(document.strings());
    const std::string title = std::string(document.strings().lookup(document.metadata().title())) + " + " +
        std::string(addition.strings().lookup(addition.metadata().title()));
    const std::string description = std::string(document.strings().lookup(document.metadata().description())) + "\n" +
        std::string(addition.strings().lookup(addition.metadata().description()));
    const Metadata metadata(strings.intern(title), strings.intern(description));
    const int track_count = document.track_count() + addition.track_count();
    const int keyframe_count = document.keyframe_count() + addition.keyframe_count();
    Document combined = document.source_summary()
        ? Document(grid, *document.source_summary(), track_count, keyframe_count, metadata)
        : Document(grid, track_count, keyframe_count, metadata);
    DocumentBuilder builder(std::move(combined), std::move(strings).build());
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
        builder.append(lane, addition.strings(), builder.intern(id));
    }
    return std::move(builder).build();
}

} // namespace timeline
