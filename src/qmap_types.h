#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

namespace qmap {

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
        return position >= offset && position < end();
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
