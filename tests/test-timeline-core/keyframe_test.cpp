// Copyright (c) 2026 Richard Thomson

#include <timeline/Keyframe.h>
#include <timeline/Lane.h>

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

using namespace timeline;

namespace
{

Time at(Ticks ticks)
{
    return Time::from_ticks(ticks);
}

} // namespace

TEST(Keyframe, stores_numeric_value_and_interpolation)
{
    const auto keyframe = Keyframe(
        "zoom-1", at(10), 1.5, KeyframeInterpolation::LINEAR, {{"operation", "replace"}, {"source", "music.rms"}});

    EXPECT_EQ("zoom-1", keyframe.id());
    EXPECT_EQ(10, keyframe.time().ticks());
    EXPECT_DOUBLE_EQ(1.5, keyframe.value());
    EXPECT_EQ(KeyframeInterpolation::LINEAR, keyframe.interpolation());
    EXPECT_EQ("replace", keyframe.attributes().at("operation"));
}

TEST(Keyframe, rejects_invalid_identity_and_value)
{
    EXPECT_THROW(Keyframe("", at(0), 1.0), std::invalid_argument);
    EXPECT_THROW(Keyframe("bad", at(0), std::numeric_limits<double>::infinity()), std::invalid_argument);
}

TEST(Lane, finds_neighboring_keyframes)
{
    auto lane = Lane("zoom", "camera.zoom", "keyframes", at(0), at(40));
    lane.add(Keyframe("zoom-20", at(20), 2.0));
    lane.add(Keyframe("zoom-0", at(0), 0.0));
    lane.add(Keyframe("zoom-30", at(30), 3.0));

    const auto neighbors = lane.neighboring_keyframes(at(10));

    ASSERT_TRUE(neighbors.before().has_value());
    ASSERT_TRUE(neighbors.after().has_value());
    EXPECT_EQ("zoom-0", neighbors.before()->get().id());
    EXPECT_EQ("zoom-20", neighbors.after()->get().id());
}

TEST(Lane, evaluates_hold_and_linear_keyframes)
{
    auto hold = Lane("hold", "Hold", "keyframes", at(0), at(30));
    hold.add(Keyframe("hold-0", at(0), 2.0, KeyframeInterpolation::HOLD, {}));
    hold.add(Keyframe("hold-20", at(20), 6.0));

    ASSERT_TRUE(hold.evaluate_keyframes(at(10)).has_value());
    EXPECT_DOUBLE_EQ(2.0, *hold.evaluate_keyframes(at(10)));
    EXPECT_DOUBLE_EQ(6.0, *hold.evaluate_keyframes(at(25)));

    auto linear = Lane("linear", "Linear", "keyframes", at(0), at(30));
    linear.add(Keyframe("linear-0", at(0), 2.0, KeyframeInterpolation::LINEAR, {}));
    linear.add(Keyframe("linear-20", at(20), 6.0));

    ASSERT_TRUE(linear.evaluate_keyframes(at(10)).has_value());
    EXPECT_DOUBLE_EQ(4.0, *linear.evaluate_keyframes(at(10)));
    EXPECT_DOUBLE_EQ(2.0, *linear.evaluate_keyframes(at(-10)));
}

TEST(Lane, rejects_duplicate_keyframe_times)
{
    auto lane = Lane("zoom", "camera.zoom", "keyframes", at(0), at(30));
    lane.add(Keyframe("zoom-0", at(0), 1.0));

    EXPECT_THROW(lane.add(Keyframe("zoom-again", at(0), 2.0)), std::invalid_argument);
}
