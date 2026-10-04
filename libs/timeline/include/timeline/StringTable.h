// Copyright (c) 2026 Richard Thomson

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

namespace timeline
{

/// Compact identity for one immutable string in a document string table.
///
/// The zero value always identifies the empty string. IDs are meaningful only
/// with the string table that produced them.
///
class StringId
{
public:
    constexpr StringId() = default;
    explicit constexpr StringId(std::uint32_t value) :
        m_value(value)
    {
    }

    constexpr std::uint32_t value() const
    {
        return m_value;
    }
    constexpr bool empty() const
    {
        return m_value == 0;
    }

private:
    std::uint32_t m_value{0};
};

constexpr bool operator==(StringId lhs, StringId rhs)
{
    return lhs.value() == rhs.value();
}
constexpr bool operator!=(StringId lhs, StringId rhs)
{
    return !(lhs == rhs);
}
constexpr bool operator<(StringId lhs, StringId rhs)
{
    return lhs.value() < rhs.value();
}

class StringTableBuilder;

/// Immutable bidirectional table of document-owned strings and compact IDs.
///
/// Copies share the same storage. Returned views remain valid for the lifetime
/// of any table copy retaining that storage.
///
class StringTable
{
public:
    StringTable();

    std::string_view lookup(StringId id) const;
    std::optional<StringId> find(std::string_view value) const;
    int size() const;
    bool shares_storage_with(const StringTable &other) const
    {
        return m_storage == other.m_storage;
    }

private:
    struct Storage;

    explicit StringTable(std::shared_ptr<const Storage> storage);
    static std::shared_ptr<const Storage> empty_storage();

    std::shared_ptr<const Storage> m_storage;

    friend class StringTableBuilder;
};

/// Mutable construction boundary for an immutable document string table.
///
/// Interning deduplicates equal strings and preserves existing IDs when the
/// builder is initialized from another table. Building transfers the complete
/// table into shared immutable ownership.
///
class StringTableBuilder
{
public:
    StringTableBuilder();
    explicit StringTableBuilder(const StringTable &source);

    StringId intern(std::string_view value);
    std::string_view lookup(StringId id) const;
    bool contains(StringId id) const;
    StringTable build() &&;

private:
    void ensure_mutable_storage();

    StringTable m_source;
    std::shared_ptr<StringTable::Storage> m_storage;
};

} // namespace timeline
