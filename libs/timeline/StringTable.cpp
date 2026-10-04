// Copyright (c) 2026 Richard Thomson

#include <timeline/StringTable.h>

#include <timeline/size_cast.h>

#include <deque>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace timeline
{

struct StringTable::Storage
{
    std::deque<std::string> strings{""};
    std::unordered_map<std::string_view, StringId> ids{{strings.front(), StringId{}}};
};

std::shared_ptr<const StringTable::Storage> StringTable::empty_storage()
{
    static const std::shared_ptr<const Storage> storage = std::make_shared<Storage>();
    return storage;
}

StringTable::StringTable() :
    m_storage(empty_storage())
{
}

StringTable::StringTable(std::shared_ptr<const Storage> storage) :
    m_storage(std::move(storage))
{
}

std::string_view StringTable::lookup(StringId id) const
{
    if (id.value() >= m_storage->strings.size())
    {
        throw std::out_of_range("string ID is not present in this table");
    }
    return m_storage->strings[id.value()];
}

std::optional<StringId> StringTable::find(std::string_view value) const
{
    const std::unordered_map<std::string_view, StringId>::const_iterator found = m_storage->ids.find(value);
    return found == m_storage->ids.end() ? std::nullopt : std::optional<StringId>{found->second};
}

int StringTable::size() const
{
    return size_cast(m_storage->strings);
}

StringTableBuilder::StringTableBuilder() :
    m_source()
{
}

StringTableBuilder::StringTableBuilder(const StringTable &source) :
    m_source(source)
{
}

StringId StringTableBuilder::intern(std::string_view value)
{
    if (!m_storage)
    {
        const std::optional<StringId> found = m_source.find(value);
        if (found)
        {
            return *found;
        }
        ensure_mutable_storage();
    }
    const std::unordered_map<std::string_view, StringId>::const_iterator found = m_storage->ids.find(value);
    if (found != m_storage->ids.end())
    {
        return found->second;
    }
    if (m_storage->strings.size() >= std::numeric_limits<std::uint32_t>::max())
    {
        throw std::overflow_error("string table contains too many strings");
    }
    m_storage->strings.emplace_back(value);
    const StringId id{static_cast<std::uint32_t>(m_storage->strings.size() - 1)};
    m_storage->ids.emplace(m_storage->strings.back(), id);
    return id;
}

std::string_view StringTableBuilder::lookup(StringId id) const
{
    if (!m_storage)
    {
        return m_source.lookup(id);
    }
    if (!contains(id))
    {
        throw std::out_of_range("string ID is not present in this table builder");
    }
    return m_storage->strings[id.value()];
}

bool StringTableBuilder::contains(StringId id) const
{
    return m_storage ? id.value() < m_storage->strings.size() : id.value() < m_source.size();
}

StringTable StringTableBuilder::build() &&
{
    return m_storage ? StringTable(std::move(m_storage)) : std::move(m_source);
}

void StringTableBuilder::ensure_mutable_storage()
{
    m_storage = std::make_shared<StringTable::Storage>();
    for (int index = 1; index < m_source.size(); ++index)
    {
        m_storage->strings.emplace_back(m_source.lookup(StringId{static_cast<std::uint32_t>(index)}));
        m_storage->ids.emplace(m_storage->strings.back(), StringId{static_cast<std::uint32_t>(index)});
    }
}

} // namespace timeline
