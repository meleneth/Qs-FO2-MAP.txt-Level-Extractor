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
    ElevationIndex elevation = ElevationIndex{0};

    constexpr ElevationSource() = default;

    constexpr ElevationSource(MapSide source_side, ElevationIndex source_elevation)
        : side(source_side)
        , elevation(source_elevation)
    {
    }

    constexpr ElevationSource(MapSide source_side, int source_elevation)
        : ElevationSource(source_side, ElevationIndex{source_elevation})
    {
    }
};

struct TextMapExportPlan {
    std::optional<MapSide> header_side = MapSide::left;
    std::array<std::optional<ElevationSource>, elevation_count> elevations;
};

Result<std::string> export_text_map(
    const ParsedTextSource& left,
    const ParsedTextSource& right,
    const TextMapExportPlan& plan
);

} // namespace qmap
