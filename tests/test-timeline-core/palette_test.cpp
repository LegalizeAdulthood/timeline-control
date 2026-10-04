// Copyright (c) 2026 Richard Thomson

#include <timeline/Interaction.h>
#include <timeline/Layout.h>
#include <timeline/Palette.h>
#include <timeline/Query.h>
#include <timeline/Snapshot.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>

using namespace timeline;

TEST(Palette, validatesRgbComponentsAndOwnedDefinitions)
{
    EXPECT_THROW(RgbColor(-1, 0, 0), std::invalid_argument);
    EXPECT_THROW(RgbColor(0, 256, 0), std::invalid_argument);
    EXPECT_THROW(RgbColor(0, 0, 256), std::invalid_argument);
    const Palette colors{RgbColor(255, 0, 0), RgbColor(0, 0, 255)};
    const PaletteCurve curve(StringId{1}, Time{}, Time::from_ticks(20), [colors](Time) { return colors; });
    EXPECT_EQ(colors, curve.sample(Time::from_ticks(-1)));
    EXPECT_EQ(colors, curve.sample(Time::from_ticks(21)));
    EXPECT_EQ(2, curve.color_count());
    EXPECT_THROW(PaletteCurve(StringId{}, Time{}, Time::from_ticks(20), [colors](Time) { return colors; }),
        std::invalid_argument);
    EXPECT_THROW(
        PaletteCurve(StringId{1}, Time{}, Time::from_ticks(20), [](Time) { return Palette{}; }), std::invalid_argument);
    EXPECT_THROW(
        PaletteCurve(StringId{1}, Time{}, Time::from_ticks(20), PaletteCurve::Evaluator{}), std::invalid_argument);
    const PaletteCurve changing(StringId{1}, Time{}, Time::from_ticks(20),
        [colors](Time time) { return time.ticks() == 0 ? colors : Palette{}; });
    EXPECT_THROW(changing.sample(Time::from_ticks(10)), std::invalid_argument);
}

TEST(Palette, queriesSamplesWithoutScalarSubstitutionAndSurvivesCopying)
{
    const Document document = []
    {
        DocumentBuilder builder(Document(FrameGrid(Timebase(30), 3, 30, 1), 0, 0));
        Lane lane(builder.intern("colors"), "Colors", "palette", Time{}, Time::from_ticks(3));
        lane.add(PaletteCurve(
            builder.intern("palette"), "color-map", Time{}, Time::from_ticks(3),
            [](Time time) { return Palette{RgbColor(static_cast<int>(time.ticks()) * 20, 0, 255)}; },
            Attributes{{"recipe", "owned"}}));
        builder.add_lane(std::move(lane));
        return std::move(builder).build();
    }();
    const FrameInspection inspected = *inspect_frame(document, 1);
    ASSERT_EQ(1, inspected.lanes.size());
    ASSERT_EQ(1, inspected.lanes[0].items.size());
    const InspectionItem &item = inspected.lanes[0].items[0];
    EXPECT_EQ(InspectionItemType::PALETTE, item.type);
    EXPECT_EQ(InspectionItemRole::SAMPLED, item.role);
    EXPECT_FALSE(item.value);
    ASSERT_TRUE(item.palette);
    EXPECT_EQ(RgbColor(20, 0, 255), item.palette->front());
    EXPECT_EQ("owned", item.attributes.at("recipe"));
    const RangeInspection range = inspect_range(document, Time{}, Time::from_ticks(3));
    EXPECT_FALSE(range.lanes[0].items[0].palette);
}

TEST(Palette, laysOutSwatchesWithRgbValuesAndStableHits)
{
    DocumentBuilder builder(Document(FrameGrid(Timebase(30), 3, 30, 1), 0, 0));
    const StringId colors_id = builder.intern("colors");
    Lane lane(colors_id, "Colors", "palette", Time{}, Time::from_ticks(3));
    const Palette colors{RgbColor(255, 0, 0), RgbColor(0, 255, 0), RgbColor(0, 0, 255), RgbColor(255, 255, 255)};
    const StringId palette_id = builder.intern("palette");
    lane.add(PaletteCurve(palette_id, Time{}, Time::from_ticks(3), [colors](Time) { return colors; }));
    builder.add_lane(std::move(lane));
    const Document document = std::move(builder).build();
    const Viewport viewport(260, 80, Time{}, Time::from_ticks(3));
    const LayoutMetrics metrics(20, 20, 40, 4);
    const Layout layout(document, viewport, metrics);
    int swatches = 0;
    for (const Primitive &primitive : layout.display_list().primitives())
    {
        if (std::holds_alternative<Swatch>(primitive))
        {
            const Swatch &swatch = std::get<Swatch>(primitive);
            EXPECT_EQ(colors[swatches % 4], swatch.color);
            EXPECT_GT(swatch.width, 0);
            EXPECT_GT(swatch.height, 0);
            EXPECT_GE(swatch.x, 20);
            EXPECT_LE(swatch.x + swatch.width, 260);
            const std::optional<HitResult> result = layout.hit_test(Point{swatch.x, swatch.y}, 0);
            ASSERT_TRUE(result);
            const HitResult &hit = *result;
            EXPECT_EQ(colors_id, hit.id.lane_id);
            EXPECT_EQ(palette_id, hit.id.item_id);
            EXPECT_EQ(StyleRole::PALETTE, hit.style);
            ++swatches;
        }
    }
    EXPECT_EQ(12, swatches);
    EXPECT_NE(std::string::npos, render_snapshot(layout.display_list()).find("swatch PALETTE"));
    Interaction interaction(document);
    interaction.select_hit(HitResult{StyleRole::PALETTE, DisplayId{colors_id, palette_id}}, false);
    const Layout selected(document, viewport, metrics, interaction);
    for (const Primitive &primitive : selected.display_list().primitives())
    {
        if (std::holds_alternative<Swatch>(primitive))
        {
            EXPECT_EQ(StyleRole::SELECTED_ITEM, std::get<Swatch>(primitive).style);
            EXPECT_NE(colors.end(), std::find(colors.begin(), colors.end(), std::get<Swatch>(primitive).color));
        }
    }
    const Layout narrow(document, Viewport(29, 80, Time{}, Time::from_ticks(3)), metrics);
    EXPECT_TRUE(std::any_of(narrow.display_list().primitives().begin(), narrow.display_list().primitives().end(),
        [](const Primitive &primitive) { return std::holds_alternative<Swatch>(primitive); }));
    const Layout outside(document, Viewport(260, 80, Time::from_ticks(-10), Time::from_ticks(-1)), metrics);
    EXPECT_FALSE(std::any_of(outside.display_list().primitives().begin(), outside.display_list().primitives().end(),
        [](const Primitive &primitive) { return std::holds_alternative<Swatch>(primitive); }));
    DocumentBuilder continuous_builder(Document(Timebase(30)), document.strings());
    continuous_builder.add_lane(document.lanes()[0]);
    const Document continuous = std::move(continuous_builder).build();
    const Layout without_grid(continuous, viewport, metrics);
    EXPECT_TRUE(
        std::any_of(without_grid.display_list().primitives().begin(), without_grid.display_list().primitives().end(),
            [](const Primitive &primitive) { return std::holds_alternative<Swatch>(primitive); }));
    EXPECT_NE(std::string::npos, render_snapshot(layout.display_list()).find("255 0 0"));
}
