// Copyright (c) 2026 Richard Thomson

#include <timeline/DisplayList.h>

#include <stdexcept>

namespace timeline
{

std::string_view to_string(StyleRole value)
{
    switch (value)
    {
    case StyleRole::RULER:
        return "ruler";
    case StyleRole::RULER_LABEL:
        return "ruler_label";
    case StyleRole::LANE_BACKGROUND:
        return "lane_background";
    case StyleRole::LANE_LABEL:
        return "lane_label";
    case StyleRole::INSTANT_MARKER:
        return "instant_marker";
    case StyleRole::INTERVAL_SPAN:
        return "interval_span";
    case StyleRole::ENVELOPE_ATTACK:
        return "envelope_attack";
    case StyleRole::ENVELOPE_SUSTAIN:
        return "envelope_sustain";
    case StyleRole::ENVELOPE_DECAY:
        return "envelope_decay";
    case StyleRole::CURVE:
        return "curve";
    case StyleRole::KEYFRAME_SEGMENT:
        return "keyframe_segment";
    case StyleRole::KEYFRAME_MARKER:
        return "keyframe_marker";
    case StyleRole::SELECTED_LANE:
        return "selected_lane";
    case StyleRole::SELECTED_ITEM:
        return "selected_item";
    case StyleRole::SELECTED_RANGE:
        return "selected_range";
    case StyleRole::PLAYHEAD:
        return "playhead";
    case StyleRole::PALETTE:
        return "palette";
    }
    throw std::invalid_argument("unknown semantic rendering role");
}

} // namespace timeline
