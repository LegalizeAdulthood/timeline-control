// Copyright (c) 2026 Richard Thomson

#include <timeline/Query.h>
#include <timelineParAnimator/TimelineJson.h>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

using namespace timeline_par_animator;
using Json = nlohmann::json;

namespace
{

std::array<double, 3> hint_components(const Json &key)
{
    std::string text = key.at("value").get<std::string>();
    std::replace(text.begin(), text.end(), '/', ' ');
    std::istringstream input(text);
    std::array<double, 3> result{};
    for (double &component : result)
    {
        input >> component;
    }
    return result;
}

} // namespace

TEST(CameraNormalization, samplesSourceArithmeticAfterInterpolationWithOwnedBounds)
{
    for (const std::string &family : {"id-3d", "julibrot"})
    {
        for (const std::string &variant : {"subnormal", "range", "range-hold", "tiny-component"})
        {
            const std::string path = "fixtures/" + family + "-view-" + variant + ".json";
            SCOPED_TRACE(path);
            const timeline::Document document = [&path]
            {
                JsonImportOptions options;
                options.frames_per_second_numerator = 30000;
                options.frames_per_second_denominator = 1001;
                const JsonImportResult imported = import_timeline_json(path, options);
                if (!imported.succeeded() || !imported.diagnostics.empty())
                {
                    throw std::runtime_error("Camera normalization import failed");
                }
                return *imported.document;
            }();
            std::ifstream input(path);
            const Json recipe = Json::parse(input);
            const Json &keys = recipe.at("tracks")[0].at("camera3d").at("view-up").at("keys");
            const std::array<double, 3> first = hint_components(keys[0]);
            const std::array<double, 3> last = hint_components(keys[1]);
            const timeline::FrameGrid &grid = *document.frame_grid();
            for (int step = 0; step <= 100; ++step)
            {
                const timeline::Time time = grid.offset() +
                    timeline::Duration::from_ticks((grid.frame_start(2) - grid.offset()).ticks() * step / 100);
                const double fraction = variant == "range-hold" ? 0
                                                                : static_cast<double>((time - grid.offset()).ticks()) /
                        static_cast<double>((grid.frame_start(2) - grid.offset()).ticks());
                std::array<double, 3> raw{};
                for (int axis = 0; axis < 3; ++axis)
                {
                    raw[axis] = step == 100 ? last[axis] : first[axis] + fraction * (last[axis] - first[axis]);
                }
                const double length = std::sqrt(raw[0] * raw[0] + raw[1] * raw[1] + raw[2] * raw[2]);
                for (int axis = 0; axis < 3; ++axis)
                {
                    const timeline::Curve &curve = std::get<timeline::Curve>(document.lanes()[12 + axis].items()[0]);
                    const double normalized = raw[axis] / length;
                    const double expected = std::abs(normalized) < 1e-12 ? 0 : normalized;
                    EXPECT_NEAR(expected, curve.sample(time), 1e-14);
                    EXPECT_LE(*curve.minimum(), curve.sample(time));
                    EXPECT_GE(*curve.maximum(), curve.sample(time));
                    EXPECT_TRUE(curve.samples().empty());
                    EXPECT_EQ("animation-0-view-up[" + std::to_string(axis) + "]", document.lanes()[12 + axis].id());
                    EXPECT_EQ("true", curve.attributes().at("normalize"));
                    EXPECT_EQ(keys.dump(), Json::parse(curve.attributes().at("signal")).at("keys").dump());
                }
            }
            if (variant == "subnormal" || variant == "range-hold")
            {
                EXPECT_GT(std::get<timeline::Curve>(document.lanes()[13].items()[0]).sample(grid.offset()), 1);
            }
            const timeline::FrameInspection frame = *timeline::inspect_frame(document, 1);
            EXPECT_DOUBLE_EQ(std::get<timeline::Curve>(document.lanes()[13].items()[0]).sample(grid.frame_start(1)),
                *frame.lanes[13].items[0].value);
        }
    }
}

TEST(CameraNormalization, rejectsSourceSingularitiesWithoutPartialTracks)
{
    for (const std::string &family : {"id-3d", "julibrot"})
    {
        SCOPED_TRACE(family);
        const JsonImportResult imported =
            import_timeline_json("fixtures/invalid-" + family + "-view-normalization.json");
        EXPECT_FALSE(imported.succeeded());
        EXPECT_FALSE(imported.document.has_value());
        ASSERT_EQ(7, timeline::size_cast(imported.diagnostics));
        for (int index = 0; index < 6; ++index)
        {
            EXPECT_NE(std::string::npos, imported.diagnostics[index].find("animation-" + std::to_string(index) + ":"));
            EXPECT_NE(std::string::npos, imported.diagnostics[index].find("view-up"));
        }
        EXPECT_NE(std::string::npos, imported.diagnostics.back().find("no supported animation tracks"));
    }
}
