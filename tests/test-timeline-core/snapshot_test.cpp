// Copyright (c) 2026 Richard Thomson

#include <timeline/Layout.h>
#include <timeline/Snapshot.h>

#include <gtest/gtest.h>

#include <locale>

using namespace timeline;

namespace
{

Time at(Ticks ticks)
{
    return Time::from_ticks(ticks);
}

std::string snapshot(const Document &document)
{
    const Layout layout(document, Viewport(400, 100, at(0), at(100)), LayoutMetrics(100, 20, 30, 4));
    return render_snapshot(layout.display_list());
}

const std::string RULER = "line RULER \"\" \"ruler\" 100 19 399 19\n"
                          "text RULER_LABEL \"\" \"ruler\" 4 4 \"Time\"\n";

/// Supplies digit grouping to detect accidental use of the global locale.
///
class GroupedNumbers : public std::numpunct<char>
{
protected:
    char do_thousands_sep() const override
    {
        return '_';
    }
    std::string do_grouping() const override
    {
        return "\3";
    }
};

} // namespace

TEST(Snapshot, rendersEmptyDisplayList)
{
    EXPECT_EQ("", render_snapshot(DisplayList{}));
}

TEST(Snapshot, rendersEmptyTimeline)
{
    EXPECT_EQ(RULER, snapshot(Document(100)));
}

TEST(Snapshot, rendersEventLane)
{
    Document document(100);
    Lane lane("events", "Events", "events", at(0), at(100));
    lane.add(Instant("beat", "beat", at(25)));
    document.add_lane(std::move(lane));

    EXPECT_EQ(RULER +
            "rectangle LANE_BACKGROUND \"events\" \"\" 100 20 300 30\n"
            "text LANE_LABEL \"events\" \"\" 4 24 \"Events\"\n"
            "marker INSTANT_MARKER \"events\" \"beat\" 174 24 2 22\n",
        snapshot(document));
}

TEST(Snapshot, rendersCurveLane)
{
    Document document(100);
    Lane lane("curve", "Curve", "curve", at(0), at(100));
    lane.add(Curve("signal", "signal", {{at(0), 0.0}, {at(40), 1.0}, {at(80), 0.0}}));
    document.add_lane(std::move(lane));

    EXPECT_EQ(RULER +
            "rectangle LANE_BACKGROUND \"curve\" \"\" 100 20 300 30\n"
            "text LANE_LABEL \"curve\" \"\" 4 24 \"Curve\"\n"
            "polyline CURVE \"curve\" \"signal\" 3 100 45 220 24 340 45\n",
        snapshot(document));
}

TEST(Snapshot, rendersKeyframeLane)
{
    Document document(100);
    Lane lane("keys", "Keys", "keyframes", at(0), at(100));
    lane.add(Keyframe("first", at(0), 0.0, KeyframeInterpolation::LINEAR, {}));
    lane.add(Keyframe("last", at(50), 1.0));
    document.add_lane(std::move(lane));

    EXPECT_EQ(RULER +
            "rectangle LANE_BACKGROUND \"keys\" \"\" 100 20 300 30\n"
            "text LANE_LABEL \"keys\" \"\" 4 24 \"Keys\"\n"
            "polyline KEYFRAME_SEGMENT \"keys\" \"first\" 2 100 45 250 24\n"
            "marker KEYFRAME_MARKER \"keys\" \"first\" 100 41 5 5\n"
            "marker KEYFRAME_MARKER \"keys\" \"last\" 248 24 5 5\n",
        snapshot(document));
}

TEST(Snapshot, preservesOrderGeometryAndEscapedStrings)
{
    DisplayList list;
    list.add(Text{-10, 20, "quotes\" slash\\ newline\n tab\t return\r", StyleRole::LANE_LABEL,
        DisplayId{"lane\n", std::string(1, '\x01') + std::string(1, '\xFF')}});
    list.add(Polyline{{}, StyleRole::CURVE, {}});
    list.add(Line{-1, -2, 3, 4, StyleRole::PLAYHEAD, {"lane", "item"}});
    list.add(Rectangle{1, 2, 3, 4, StyleRole::INTERVAL_SPAN, {}});
    list.add(Marker{5, 6, 7, 8, StyleRole::SELECTED_ITEM, {}});

    const std::string expected =
        "text LANE_LABEL \"lane\\n\" \"\\x01\\xFF\" -10 20 \"quotes\\\" slash\\\\ newline\\n tab\\t return\\r\"\n"
        "polyline CURVE \"\" \"\" 0\n"
        "line PLAYHEAD \"lane\" \"item\" -1 -2 3 4\n"
        "rectangle INTERVAL_SPAN \"\" \"\" 1 2 3 4\n"
        "marker SELECTED_ITEM \"\" \"\" 5 6 7 8\n";
    EXPECT_EQ(expected, render_snapshot(list));
    EXPECT_EQ(expected, render_snapshot(list));
}

TEST(Snapshot, ignoresGlobalNumericLocale)
{
    DisplayList list;
    list.add(Line{1000, 2000, 3000, 4000, StyleRole::RULER, {}});
    const std::locale original = std::locale::global(std::locale(std::locale::classic(), new GroupedNumbers));
    const std::string result = render_snapshot(list);
    std::locale::global(original);

    EXPECT_EQ("line RULER \"\" \"\" 1000 2000 3000 4000\n", result);
}
