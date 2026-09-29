#pragma once

namespace timeline
{

template <typename Container>
int size_cast(const Container &c)
{
    return static_cast<int>(c.size());
}

} // namespace timeline
