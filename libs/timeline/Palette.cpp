// Copyright (c) 2026 Richard Thomson

#include <timeline/Palette.h>
#include <timeline/size_cast.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace timeline
{

RgbColor::RgbColor(int red, int green, int blue) :
    m_red(red),
    m_green(green),
    m_blue(blue)
{
    if (red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 || blue > 255)
    {
        throw std::invalid_argument("RGB channels must be in the range 0 through 255");
    }
}

PaletteCurve::PaletteCurve(StringId id, Time start, Time end, Evaluator evaluator) :
    PaletteCurve(id, "palette", start, end, std::move(evaluator), {})
{
}

PaletteCurve::PaletteCurve(
    StringId id, std::string kind, Time start, Time end, Evaluator evaluator, Attributes attributes) :
    m_id(id),
    m_kind(std::move(kind)),
    m_start(start),
    m_end(end),
    m_evaluate(std::move(evaluator)),
    m_attributes(std::move(attributes)),
    m_color_count(0)
{
    if (m_id.empty() || m_kind.empty() || end < start || !m_evaluate)
    {
        throw std::invalid_argument("palette curves require identity, an ordered range, and an evaluator");
    }
    m_color_count = size_cast(m_evaluate(m_start));
    if (m_color_count == 0)
    {
        throw std::invalid_argument("palette curves require at least one color");
    }
}

Palette PaletteCurve::sample(Time time) const
{
    Palette result = m_evaluate(std::clamp(time, m_start, m_end));
    if (size_cast(result) != m_color_count)
    {
        throw std::invalid_argument("palette evaluation must preserve its color count");
    }
    return result;
}

} // namespace timeline
