// Copyright (c) 2026 Richard Thomson

#include <timeline/DisplayList.h>

#include <gtest/gtest.h>

using namespace timeline;

TEST(StyleRole, convertsEveryValueToString)
{
    EXPECT_EQ("ruler", to_string(StyleRole::RULER));
    EXPECT_EQ("ruler_label", to_string(StyleRole::RULER_LABEL));
    EXPECT_EQ("lane_background", to_string(StyleRole::LANE_BACKGROUND));
    EXPECT_EQ("lane_label", to_string(StyleRole::LANE_LABEL));
    EXPECT_EQ("instant_marker", to_string(StyleRole::INSTANT_MARKER));
    EXPECT_EQ("interval_span", to_string(StyleRole::INTERVAL_SPAN));
    EXPECT_EQ("envelope_attack", to_string(StyleRole::ENVELOPE_ATTACK));
    EXPECT_EQ("envelope_sustain", to_string(StyleRole::ENVELOPE_SUSTAIN));
    EXPECT_EQ("envelope_decay", to_string(StyleRole::ENVELOPE_DECAY));
    EXPECT_EQ("curve", to_string(StyleRole::CURVE));
    EXPECT_EQ("keyframe_segment", to_string(StyleRole::KEYFRAME_SEGMENT));
    EXPECT_EQ("keyframe_marker", to_string(StyleRole::KEYFRAME_MARKER));
    EXPECT_EQ("selected_lane", to_string(StyleRole::SELECTED_LANE));
    EXPECT_EQ("selected_item", to_string(StyleRole::SELECTED_ITEM));
    EXPECT_EQ("selected_range", to_string(StyleRole::SELECTED_RANGE));
    EXPECT_EQ("playhead", to_string(StyleRole::PLAYHEAD));
    EXPECT_EQ("palette", to_string(StyleRole::PALETTE));
}
