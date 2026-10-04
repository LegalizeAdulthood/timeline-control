// Copyright (c) 2026 Richard Thomson

#include <timelineParAnimator/TimelineJson.h>

#include <timeline/Interaction.h>
#include <timeline/Layout.h>
#include <timeline/Query.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string_view>
#include <variant>

using namespace timeline_par_animator;

namespace
{

std::filesystem::path fixture_path(const char *name)
{
    return std::filesystem::path("fixtures") / name;
}

const JsonImportOptions TEST_OPTIONS{
    24000,
    24,
    1,
};

JsonImportOptions tracker_options()
{
    JsonImportOptions options{};
    options.beat_keys_config_path = fixture_path("beat-keys/adapter.beat-keys.json");
    return options;
}

} // namespace

TEST(TimelineJson, importsMinimalParanimatorConfig)
{
    const std::filesystem::path source_path = fixture_path("maxiter.json");
    JsonImportResult result = import_timeline_json(source_path, TEST_OPTIONS);

    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.document.has_value());
    EXPECT_TRUE(result.diagnostics.empty());
    EXPECT_EQ("maxiter.json", result.document->metadata().title());
    EXPECT_EQ(source_path.string(), result.document->metadata().description());
    EXPECT_EQ(24000, result.document->timebase().ticks_per_second());
    ASSERT_TRUE(result.document->frame_grid().has_value());
    EXPECT_EQ(3, result.document->frame_grid()->frame_count());
    EXPECT_EQ(24, result.document->frame_grid()->frames_per_second_numerator());
    EXPECT_EQ(1, result.document->frame_grid()->frames_per_second_denominator());
    EXPECT_EQ(1, result.document->track_count());
    EXPECT_EQ(2, result.document->keyframe_count());
    ASSERT_EQ(1, result.document->lane_count());
    const timeline::Lane &lane = result.document->lanes().front();
    EXPECT_EQ("maxiter", lane.label());
    ASSERT_EQ(2, lane.item_count());
    EXPECT_DOUBLE_EQ(150.0, *lane.evaluate_keyframes(result.document->frame_grid()->frame_start(1)));
}

TEST(TimelineJson, reportsMultiTrackCounts)
{
    const JsonImportResult result = import_timeline_json(fixture_path("multi-track.json"), TEST_OPTIONS);

    ASSERT_TRUE(result.succeeded());
    EXPECT_EQ(3, result.document->frame_grid()->frame_count());
    EXPECT_EQ(2, result.document->track_count());
    EXPECT_EQ(4, result.document->keyframe_count());
}

TEST(TimelineJson, rejectsInvalidSchema)
{
    const JsonImportResult result = import_timeline_json(fixture_path("invalid-schema.json"), TEST_OPTIONS);

    EXPECT_FALSE(result.succeeded());
    EXPECT_FALSE(result.document.has_value());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_NE(std::string::npos, result.diagnostics.front().find("unexpected"));
}

TEST(TimelineJson, rejectsEmptySourcePath)
{
    const JsonImportResult result = import_timeline_json({}, TEST_OPTIONS);

    EXPECT_FALSE(result.succeeded());
    EXPECT_FALSE(result.document.has_value());
    ASSERT_EQ(1U, result.diagnostics.size());
    EXPECT_EQ("No JSON file was selected.", result.diagnostics.front());
}

TEST(TimelineJson, importsTrackerTimelineWithAdjacentConfig)
{
    const JsonImportResult result =
        import_timeline_json(fixture_path("beat-keys/timeline-events.json"), tracker_options());

    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.document->source_summary().has_value());
    const timeline::SourceSummary &summary = *result.document->source_summary();
    EXPECT_EQ("par-beatdown.tracker-timeline", summary.schema());
    EXPECT_EQ(1, summary.schema_version());
    EXPECT_EQ(0, summary.feature_count());
    EXPECT_EQ(6, summary.event_count());
    EXPECT_EQ(0, *summary.first_frame());
    EXPECT_EQ(4, *summary.last_frame());
    EXPECT_EQ(0, summary.first_time()->ticks());
    EXPECT_EQ(16000, summary.last_time()->ticks());
    EXPECT_EQ(0, summary.frame_offset()->ticks());
    ASSERT_TRUE(result.document->frame_grid().has_value());
    EXPECT_EQ(5, result.document->frame_grid()->frame_count());
    EXPECT_EQ(30, result.document->frame_grid()->frames_per_second_numerator());
    EXPECT_EQ(1, result.document->frame_grid()->frames_per_second_denominator());
    ASSERT_EQ(1, result.document->lane_count());
    const timeline::Lane &lane = result.document->lanes().front();
    EXPECT_EQ("tracker-events", result.document->strings().lookup(lane.id()));
    EXPECT_EQ("Music events", lane.label());
    ASSERT_EQ(6, lane.item_count());
    EXPECT_EQ("note", std::get<timeline::Instant>(lane.items()[0]).kind());
    EXPECT_EQ(0, std::get<timeline::Instant>(lane.items()[0]).time().ticks());
    EXPECT_EQ("effect", std::get<timeline::Instant>(lane.items()[2]).kind());
    EXPECT_EQ(8000, std::get<timeline::Instant>(lane.items()[2]).time().ticks());
    EXPECT_EQ("row", std::get<timeline::Instant>(lane.items()[3]).kind());
    EXPECT_EQ(12000, std::get<timeline::Instant>(lane.items()[4]).time().ticks());
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TimelineJson, importsFullTrackerTimelineSummary)
{
    const JsonImportResult result = import_timeline_json(fixture_path("par-beatdown/gold-write-timeline-clock.json"));

    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.document->source_summary().has_value());
    const timeline::SourceSummary &summary = *result.document->source_summary();
    EXPECT_EQ(0, summary.feature_count());
    EXPECT_EQ(1891, summary.event_count());
    EXPECT_EQ(0, *summary.first_frame());
    EXPECT_EQ(4908, *summary.last_frame());
    EXPECT_EQ(0, summary.first_time()->ticks());
    EXPECT_EQ(19631430, summary.last_time()->ticks());
    ASSERT_TRUE(result.document->frame_grid().has_value());
    EXPECT_EQ(4909, result.document->frame_grid()->frame_count());
    EXPECT_EQ(30, result.document->frame_grid()->frames_per_second_numerator());
    EXPECT_EQ(1, result.document->frame_grid()->frames_per_second_denominator());
    ASSERT_EQ(1, result.document->lane_count());
    EXPECT_EQ(1891, result.document->lanes().front().item_count());
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TimelineJson, importsCompleteMusicTimeline)
{
    const JsonImportResult result = import_timeline_json(fixture_path("par-beatdown/song.music.json"));

    ASSERT_TRUE(result.succeeded());
    EXPECT_TRUE(result.diagnostics.empty());
    const timeline::Document &document = *result.document;
    EXPECT_EQ("song.music.json", document.metadata().title());
    EXPECT_EQ("data/my_neighbors_kid_is_an_internet_addict.xm (xm)", document.metadata().description());
    ASSERT_TRUE(document.source_summary());
    const timeline::SourceSummary &summary = *document.source_summary();
    EXPECT_EQ(36, summary.event_count());
    EXPECT_EQ(4, summary.feature_count());
    ASSERT_TRUE(summary.generation_summary());
    const timeline::GenerationSummary &generation = *summary.generation_summary();
    EXPECT_EQ("par-beatdown", generation.generator_name());
    EXPECT_EQ("0.1.0", generation.generator_version());
    ASSERT_EQ(1, timeline::size_cast(generation.source_references()));
    EXPECT_EQ("music", generation.source_references()[0].role());
    EXPECT_EQ("data/my_neighbors_kid_is_an_internet_addict.xm", generation.source_references()[0].location());
    ASSERT_TRUE(document.frame_grid());
    EXPECT_EQ(61, document.frame_grid()->frame_count());
    EXPECT_EQ(30, document.frame_grid()->frames_per_second_numerator());
    ASSERT_EQ(2, document.lane_count());
    EXPECT_EQ(36, document.lanes()[0].item_count());
    const auto &event = std::get<timeline::Instant>(document.lanes()[0].items()[0]);
    EXPECT_EQ(8520, event.time().ticks());
    EXPECT_EQ("12", event.attributes().at("row"));
    const auto &curve = std::get<timeline::Curve>(document.lanes()[1].items()[0]);
    EXPECT_EQ(4, curve.sample_count());
    const std::optional<timeline::FrameInspection> inspection = timeline::inspect_frame(document, 15);
    ASSERT_TRUE(inspection);
    ASSERT_EQ(2, timeline::size_cast(inspection->lanes));
    ASSERT_EQ(1, timeline::size_cast(inspection->lanes[1].items));
    EXPECT_DOUBLE_EQ(0.11958, *inspection->lanes[1].items[0].value);
}

TEST(TimelineJson, retainsValidRecordsWithIndexedDiagnostics)
{
    const JsonImportResult result = import_timeline_json(fixture_path("par-beatdown/partial.music.json"));

    ASSERT_TRUE(result.succeeded());
    const timeline::Document &document = *result.document;
    EXPECT_EQ("Recovered song", document.metadata().title());
    EXPECT_EQ("fixture.xm (xm)", document.metadata().description());
    ASSERT_TRUE(document.frame_grid());
    EXPECT_EQ(61, document.frame_grid()->frame_count());
    ASSERT_EQ(2, document.lane_count());
    const timeline::Lane &events = document.lanes()[0];
    ASSERT_EQ(2, events.item_count());
    EXPECT_EQ("event-0", std::get<timeline::Instant>(events.items()[0]).id());
    const auto &event = std::get<timeline::Instant>(events.items()[1]);
    EXPECT_EQ("event-4", event.id());
    EXPECT_EQ(240000, event.time().ticks());
    EXPECT_EQ("125", event.attributes().at("parameter"));
    EXPECT_LT(event.time(), events.end());
    const auto &curve = std::get<timeline::Curve>(document.lanes()[1].items()[0]);
    EXPECT_EQ(3, curve.sample_count());
    EXPECT_DOUBLE_EQ(1.25, curve.samples()[0].value());
    EXPECT_DOUBLE_EQ(1.25, *curve.maximum());
    EXPECT_DOUBLE_EQ(0.2, curve.samples()[1].value());
    auto contains_diagnostic = [&](std::string_view text)
    {
        return std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
            [text](const std::string &message) { return message.find(text) != std::string::npos; });
    };
    for (const int index : {1, 2, 3, 5, 6, 7})
    {
        EXPECT_TRUE(contains_diagnostic("events[" + std::to_string(index) + "]"));
    }
    for (const int index : {1, 2, 4})
    {
        EXPECT_TRUE(contains_diagnostic("features[" + std::to_string(index) + "]"));
    }
    EXPECT_TRUE(contains_diagnostic("Warning: preserved warning"));
    EXPECT_TRUE(contains_diagnostic("Warning: another warning"));
    EXPECT_TRUE(contains_diagnostic("Log: preserved log"));
    EXPECT_TRUE(contains_diagnostic("warnings[1]"));
    EXPECT_TRUE(contains_diagnostic("unsupported"));
}

TEST(TimelineJson, rejectsInvalidTrackerArrayShape)
{
    const JsonImportResult result = import_timeline_json(fixture_path("par-beatdown/invalid-shape.json"));

    EXPECT_FALSE(result.succeeded());
    ASSERT_EQ(1, timeline::size_cast(result.diagnostics));
    EXPECT_NE(std::string::npos, result.diagnostics[0].find("events array"));
}

TEST(TimelineJson, malformedOptionalMetadataDoesNotDiscardEvents)
{
    const JsonImportResult result = import_timeline_json(fixture_path("par-beatdown/invalid-metadata.json"));

    ASSERT_TRUE(result.succeeded());
    EXPECT_EQ("invalid-metadata.json", result.document->metadata().title());
    EXPECT_EQ("fixture.xm (xm)", result.document->metadata().description());
    EXPECT_FALSE(result.document->source_summary()->generation_summary());
    EXPECT_EQ(1, result.document->lane_count());
    EXPECT_EQ(1, result.document->lanes()[0].item_count());
    EXPECT_EQ(3, timeline::size_cast(result.diagnostics));
}

TEST(TimelineJson, importsTrackerRmsCurve)
{
    const JsonImportResult result =
        import_timeline_json(fixture_path("par-beatdown/gold-write-windowed-features.json"));

    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.document->frame_grid().has_value());
    EXPECT_EQ(46, result.document->frame_grid()->frame_count());
    ASSERT_EQ(1, result.document->lane_count());
    const timeline::Lane &lane = result.document->lanes().front();
    EXPECT_EQ("tracker-rms", result.document->strings().lookup(lane.id()));
    EXPECT_EQ("RMS", lane.label());
    ASSERT_EQ(1, lane.item_count());
    const auto &curve = std::get<timeline::Curve>(lane.items().front());
    ASSERT_EQ(4, curve.sample_count());
    EXPECT_EQ(60000, curve.samples()[1].time().ticks());
    EXPECT_DOUBLE_EQ(0.11958, curve.samples()[1].value());
    EXPECT_DOUBLE_EQ(0.0, *curve.minimum());
    EXPECT_DOUBLE_EQ(1.0, *curve.maximum());
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TimelineJson, mixedTrackerTimelineDrivesCoreDisplayList)
{
    const JsonImportResult result = import_timeline_json(fixture_path("par-beatdown/mixed-events-and-features.json"));

    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(2, result.document->lane_count());
    ASSERT_TRUE(result.document->frame_grid().has_value());
    const timeline::Layout layout(*result.document,
        timeline::Viewport(600, 120, timeline::Time::from_ticks(0), result.document->frame_grid()->end_time()),
        timeline::LayoutMetrics(100, 20, 30, 4));

    int marker_count = 0;
    int curve_count = 0;
    for (const timeline::Primitive &primitive : layout.display_list().primitives())
    {
        if (const auto marker = std::get_if<timeline::Marker>(&primitive))
        {
            EXPECT_EQ(timeline::StyleRole::INSTANT_MARKER, marker->style);
            EXPECT_EQ("tracker-events", result.document->strings().lookup(marker->id.lane_id));
            EXPECT_FALSE(marker->id.item_id.empty());
            const std::optional<timeline::HitResult> hit = layout.hit_test(timeline::Point{marker->x, marker->y}, 3);
            ASSERT_TRUE(hit);
            EXPECT_EQ(marker->style, hit->style);
            EXPECT_EQ(marker->id.item_id, hit->id.item_id);
            ++marker_count;
        }
        if (const auto curve = std::get_if<timeline::Polyline>(&primitive))
        {
            EXPECT_EQ(timeline::StyleRole::CURVE, curve->style);
            EXPECT_EQ("tracker-rms", result.document->strings().lookup(curve->id.lane_id));
            EXPECT_EQ("tracker-rms", curve->id.item_id);
            ASSERT_FALSE(curve->points.empty());
            const std::optional<timeline::HitResult> hit = layout.hit_test(curve->points.front(), 3);
            ASSERT_TRUE(hit);
            EXPECT_EQ(curve->style, hit->style);
            EXPECT_EQ(curve->id.item_id, hit->id.item_id);
            ++curve_count;
        }
    }
    EXPECT_EQ(2, marker_count);
    EXPECT_EQ(1, curve_count);
}

TEST(TimelineJson, importedItemSelectionSurvivesNavigation)
{
    const JsonImportResult result = import_timeline_json(fixture_path("par-beatdown/mixed-events-and-features.json"));
    ASSERT_TRUE(result.succeeded());
    const timeline::Document &document = *result.document;
    ASSERT_TRUE(document.frame_grid());
    const timeline::LayoutMetrics metrics(100, 20, 30, 4);
    timeline::Navigation navigation(*document.content_start(), *document.content_end(), document.lane_count());
    timeline::Interaction interaction(document);
    const timeline::Layout layout(document, navigation.viewport(600, 120), metrics);
    bool selected = false;
    for (const timeline::Primitive &primitive : layout.display_list().primitives())
    {
        if (const auto marker = std::get_if<timeline::Marker>(&primitive))
        {
            const std::optional<timeline::HitResult> hit = layout.hit_test(timeline::Point{marker->x, marker->y}, 3);
            ASSERT_TRUE(hit);
            interaction.select_hit(hit, false);
            selected = true;
            break;
        }
    }
    ASSERT_TRUE(selected);
    interaction.move_playhead_frame(0);
    navigation.zoom_by(2.0, *interaction.playhead());
    const timeline::Layout selected_layout(document, navigation.viewport(600, 120), metrics, interaction);
    bool highlighted = false;
    for (const timeline::Primitive &primitive : selected_layout.display_list().primitives())
    {
        if (const auto marker = std::get_if<timeline::Marker>(&primitive))
        {
            if (marker->style == timeline::StyleRole::SELECTED_ITEM)
            {
                highlighted = true;
                EXPECT_TRUE(interaction.is_selected(marker->id));
                const std::optional<timeline::HitResult> hit =
                    selected_layout.hit_test(timeline::Point{marker->x, marker->y}, 0);
                ASSERT_TRUE(hit);
                EXPECT_EQ(timeline::StyleRole::INSTANT_MARKER, hit->style);
            }
        }
    }
    EXPECT_TRUE(highlighted);
    EXPECT_EQ(0, *interaction.playhead_frame());
}

TEST(TimelineJson, preservesTrackerDiagnosticsOutsideCore)
{
    const JsonImportResult result = import_timeline_json(fixture_path("beat-keys/timeline-diagnostics.json"));

    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.document->source_summary().has_value());
    EXPECT_EQ(3U, result.diagnostics.size());
    EXPECT_EQ("Warning: tempo was approximated", result.diagnostics[0]);
    EXPECT_EQ("Unsupported: effect command 0x7f", result.diagnostics[1]);
    EXPECT_EQ("Log: loaded fixture", result.diagnostics[2]);
}

TEST(TimelineJson, importsRowPulseOverlaySummary)
{
    const JsonImportResult result = import_timeline_json(fixture_path("beat-keys/gold-write-row-pulses.json"));

    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.document->source_summary().has_value());
    const timeline::SourceSummary &source = *result.document->source_summary();
    EXPECT_EQ("par-beatdown.beat-keys-overlay", source.schema());
    EXPECT_EQ(0, *source.first_frame());
    EXPECT_EQ(4, *source.last_frame());
    ASSERT_TRUE(source.generation_summary().has_value());
    const timeline::GenerationSummary &generation = *source.generation_summary();
    EXPECT_EQ("beat-keys", generation.generator_name());
    EXPECT_EQ("0.1.0", generation.generator_version());
    ASSERT_EQ(3U, generation.source_references().size());
    EXPECT_EQ("base_animation", generation.source_references()[0].role());
    EXPECT_EQ("tests/beat-keys/base-animation.json", generation.source_references()[0].location());
    EXPECT_EQ("timeline", generation.source_references()[1].role());
    EXPECT_EQ("tests/beat-keys/timeline-events.json", generation.source_references()[1].location());
    EXPECT_EQ("adapter_config", generation.source_references()[2].role());
    EXPECT_EQ("tests/beat-keys/row-pulses.beat-keys.json", generation.source_references()[2].location());
    ASSERT_EQ(1U, generation.target_counts().size());
    EXPECT_EQ("row.flash", generation.target_counts()[0].name());
    EXPECT_EQ(4, generation.target_counts()[0].count());
    ASSERT_EQ(1U, generation.source_counts().size());
    EXPECT_EQ("music.row_pulse", generation.source_counts()[0].name());
    EXPECT_EQ(4, generation.source_counts()[0].count());
    EXPECT_EQ(1, result.document->track_count());
    EXPECT_EQ(4, result.document->keyframe_count());
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(TimelineJson, importsRmsOverlaySummary)
{
    const JsonImportResult result = import_timeline_json(fixture_path("beat-keys/gold-write-rms-keyframes.json"));

    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.document->source_summary().has_value());
    const timeline::SourceSummary &source = *result.document->source_summary();
    ASSERT_TRUE(source.generation_summary().has_value());
    const timeline::GenerationSummary &generation = *source.generation_summary();
    ASSERT_EQ(3U, generation.target_counts().size());
    EXPECT_EQ("camera.zoom", generation.target_counts()[0].name());
    EXPECT_EQ(3, generation.target_counts()[0].count());
    EXPECT_EQ("color.brightness", generation.target_counts()[1].name());
    EXPECT_EQ(3, generation.target_counts()[1].count());
    EXPECT_EQ("layer.opacity", generation.target_counts()[2].name());
    EXPECT_EQ(3, generation.target_counts()[2].count());
    ASSERT_EQ(1U, generation.source_counts().size());
    EXPECT_EQ("music.rms", generation.source_counts()[0].name());
    EXPECT_EQ(9, generation.source_counts()[0].count());
    EXPECT_EQ(3, result.document->track_count());
    EXPECT_EQ(9, result.document->keyframe_count());
    ASSERT_TRUE(result.document->frame_grid().has_value());
    EXPECT_EQ(5, result.document->frame_grid()->frame_count());
    ASSERT_EQ(3, result.document->lane_count());
    const timeline::Lane &zoom = result.document->lanes()[0];
    EXPECT_EQ("camera.zoom", result.document->strings().lookup(zoom.id()));
    EXPECT_EQ("camera.zoom", zoom.label());
    ASSERT_EQ(3, zoom.item_count());
    const auto &first = std::get<timeline::Keyframe>(zoom.items()[0]);
    EXPECT_EQ(0, first.time().ticks());
    EXPECT_DOUBLE_EQ(0.25, first.value());
    EXPECT_EQ(timeline::KeyframeInterpolation::HOLD, first.interpolation());
    EXPECT_EQ("replace", first.attributes().at("operation"));
    EXPECT_EQ("music.rms", first.attributes().at("source"));
    EXPECT_DOUBLE_EQ(0.25, *zoom.evaluate_keyframes(result.document->frame_grid()->frame_start(1)));
    EXPECT_TRUE(result.diagnostics.empty());
}
