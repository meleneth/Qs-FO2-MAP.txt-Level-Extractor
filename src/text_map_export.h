#pragma once

#include "qmap_result.h"
#include "text_map_records.h"
#include "text_map_parser.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

struct TextMapTransform {
    std::string header;
    std::array<std::optional<std::string>, elevation_count> elevations;
    std::array<std::vector<std::string>, script_type_count> scripts;
    std::vector<std::string> objects;
};

Result<TextMapTransform> build_text_map_transform(
    const ParsedTextSource& left,
    const ParsedTextSource& right,
    const TextMapExportPlan& plan
);

Result<std::string> serialize_text_map_transform(const TextMapTransform& transform);

Result<std::string> export_text_map(
    const ParsedTextSource& left,
    const ParsedTextSource& right,
    const TextMapExportPlan& plan
);

} // namespace qmap
