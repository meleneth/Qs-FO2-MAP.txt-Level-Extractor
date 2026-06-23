#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

namespace qmap {

constexpr int elevation_count = 3;

struct ElevationIndex {
    int value = 0;

    constexpr explicit ElevationIndex(int elevation = 0)
        : value(elevation)
    {
    }

};

constexpr bool operator==(ElevationIndex left, ElevationIndex right)
{
    return left.value == right.value;
}

constexpr bool operator!=(ElevationIndex left, ElevationIndex right)
{
    return !(left == right);
}

constexpr bool is_valid_elevation(int elevation)
{
    return elevation >= 0 && elevation < elevation_count;
}

constexpr bool is_valid_elevation(ElevationIndex elevation)
{
    return is_valid_elevation(elevation.value);
}

inline std::optional<ElevationIndex> elevation_index_from_int(int elevation)
{
    if (!is_valid_elevation(elevation)) {
        return std::nullopt;
    }

    return ElevationIndex{elevation};
}

enum class MapFileKind {
    empty,
    text,
    binary,
};

struct Range {
    std::size_t offset = 0;
    std::size_t size = 0;

    constexpr std::size_t end() const
    {
        return offset + size;
    }

    constexpr bool empty() const
    {
        return size == 0;
    }

    constexpr bool contains(std::size_t position) const
    {
        return position >= offset && position - offset < size;
    }
};

inline std::optional<std::string_view> view_range(std::string_view text, Range range)
{
    if (range.offset > text.size() || range.size > text.size() - range.offset) {
        return std::nullopt;
    }

    return text.substr(range.offset, range.size);
}

} // namespace qmap
