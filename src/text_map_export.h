#pragma once

#include "qmap_result.h"
#include "text_map_parser.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace qmap {

enum class MapSide {
    left,
    right,
};

struct ParsedTextSource {
    std::string_view text;
    ParsedTextMap map;
};

struct ElevationSource {
    MapSide side = MapSide::left;
    int elevation = 0;
};

struct TextMapExportPlan {
    MapSide header_side = MapSide::left;
    std::array<std::optional<ElevationSource>, elevation_count> elevations;
};

Result<std::string> export_text_map(
    const ParsedTextSource& left,
    const ParsedTextSource& right,
    const TextMapExportPlan& plan
);

} // namespace qmap
