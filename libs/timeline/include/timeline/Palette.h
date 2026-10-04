// Copyright (c) 2026 Richard Thomson

#pragma once

#include <timeline/Event.h>

#include <functional>
#include <vector>

namespace timeline
{

/// One opaque RGB color with integral channels in the range 0 through 255.
///
class RgbColor
{
public:
    RgbColor(int red, int green, int blue);
    int red() const
    {
        return m_red;
    }
    int green() const
    {
        return m_green;
    }
    int blue() const
    {
        return m_blue;
    }

private:
    int m_red;
    int m_green;
    int m_blue;
};

inline bool operator==(const RgbColor &left, const RgbColor &right)
{
    return left.red() == right.red() && left.green() == right.green() && left.blue() == right.blue();
}
inline bool operator!=(const RgbColor &left, const RgbColor &right)
{
    return !(left == right);
}

/// Ordered colors whose indexes retain their meaning during sampling.
///
using Palette = std::vector<RgbColor>;

/// Owned time-dependent palette definition, independent of files and toolkits.
///
/// Sampling clamps to the finite time range and preserves the palette size.
/// Definitions remain the source of truth; rendered swatches are disposable.
///
class PaletteCurve
{
public:
    /// Callable that owns the inputs required to evaluate one ordered palette.
    using Evaluator = std::function<Palette(Time)>;

    PaletteCurve(std::string id, Time start, Time end, Evaluator evaluator);
    PaletteCurve(std::string id, std::string kind, Time start, Time end, Evaluator evaluator, Attributes attributes);

    const std::string &id() const
    {
        return m_id;
    }
    const std::string &kind() const
    {
        return m_kind;
    }
    Time start() const
    {
        return m_start;
    }
    Time end() const
    {
        return m_end;
    }
    int color_count() const
    {
        return m_color_count;
    }
    const Attributes &attributes() const
    {
        return m_attributes;
    }
    Palette sample(Time time) const;

private:
    std::string m_id;
    std::string m_kind;
    Time m_start;
    Time m_end;
    Evaluator m_evaluate;
    Attributes m_attributes;
    int m_color_count;
};

} // namespace timeline
