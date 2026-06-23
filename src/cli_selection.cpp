#include "cli_selection.h"

#include <charconv>
#include <stdexcept>

namespace qmap::cli {
namespace {

int parse_selection_elevation(std::string_view value)
{
    if (value.empty()) {
        throw std::runtime_error("selection elevations must be 0, 1, or 2");
    }

    int parsed = 0;
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end) {
        throw std::runtime_error("selection elevations must be 0, 1, or 2");
    }
    if (!is_valid_elevation(parsed)) {
        throw std::runtime_error("selection elevations must be 0, 1, or 2");
    }

    return parsed;
}

} // namespace

TextMapExportPlan single_elevation_plan(int elevation)
{
    if (!is_valid_elevation(elevation)) {
        throw std::runtime_error("elevation must be 0, 1, or 2");
    }

    TextMapExportPlan plan;
    plan.header_side = MapSide::left;
    plan.elevations[elevation] = ElevationSource{MapSide::left, *elevation_index_from_int(elevation)};
    return plan;
}

std::filesystem::path split_output_path(
    const std::filesystem::path& output_dir,
    const std::filesystem::path& input,
    int elevation
)
{
    auto stem = input.stem().string();
    stem += "_elev";
    stem += std::to_string(elevation);
    stem += ".txt";
    return output_dir / stem;
}

MapSide parse_side(char side)
{
    if (side == 'L' || side == 'l') {
        return MapSide::left;
    }
    if (side == 'R' || side == 'r') {
        return MapSide::right;
    }
    throw std::runtime_error("selection side must be L or R");
}

void apply_selection(TextMapExportPlan& plan, std::string_view spec)
{
    // Format: destination=side:source, for example 2=R:0.
    const auto equals = spec.find('=');
    const auto colon = spec.find(':');
    if (equals == std::string_view::npos || colon == std::string_view::npos || colon != equals + 2) {
        throw std::runtime_error("selection must use DEST=SIDE:SOURCE, for example 2=R:0");
    }

    const auto destination = parse_selection_elevation(spec.substr(0, equals));
    const auto source = parse_selection_elevation(spec.substr(colon + 1));

    plan.elevations[destination] = ElevationSource{parse_side(spec[equals + 1]), source};
}

} // namespace qmap::cli
