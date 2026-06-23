#pragma once

#include "text_map_export.h"

#include <filesystem>
#include <string_view>

namespace qmap::cli {

TextMapExportPlan single_elevation_plan(int elevation);
std::filesystem::path split_output_path(
    const std::filesystem::path& output_dir,
    const std::filesystem::path& input,
    int elevation
);
MapSide parse_side(char side);
void apply_selection(TextMapExportPlan& plan, std::string_view spec);

} // namespace qmap::cli
