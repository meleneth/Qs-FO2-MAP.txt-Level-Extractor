#pragma once

#include "qmap_result.h"
#include "qmap_types.h"

#include <array>
#include <optional>
#include <string_view>

namespace qmap {

struct ParsedTextMap {
    Range header;
    std::array<std::optional<Range>, elevation_count> elevations;
    Range scripts;
    Range objects;

    std::optional<std::string_view> header_view(std::string_view text) const
    {
        return view_range(text, header);
    }

    std::optional<std::string_view> elevation_view(std::string_view text, int elevation) const
    {
        if (elevation < 0 || elevation >= elevation_count || !elevations[elevation]) {
            return std::nullopt;
        }

        return view_range(text, *elevations[elevation]);
    }

    std::optional<std::string_view> scripts_view(std::string_view text) const
    {
        return view_range(text, scripts);
    }

    std::optional<std::string_view> objects_view(std::string_view text) const
    {
        return view_range(text, objects);
    }
};

Result<ParsedTextMap> parse_text_map(std::string_view text);

} // namespace qmap
