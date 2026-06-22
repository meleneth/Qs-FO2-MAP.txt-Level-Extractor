#pragma once

#include "binary_map_parser.h"
#include "text_map_export.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace qmap::cli {

struct ExtractOptions {
    std::filesystem::path input;
    std::filesystem::path output;
    int elevation = 0;
    bool force = false;
};

struct SplitOptions {
    std::filesystem::path input;
    std::filesystem::path output_dir;
    bool force = false;
};

struct CombineOptions {
    std::filesystem::path left;
    std::filesystem::path right;
    std::filesystem::path output;
    int header = 0;
    std::vector<std::string> selection_specs;
    bool force = false;
};

std::string read_text_file(const std::filesystem::path& path);
std::vector<std::byte> read_binary_file(const std::filesystem::path& path);
std::string lowercase_extension(const std::filesystem::path& path);

std::string format_binary_map_stats(
    const BinaryMapHeader& header,
    const BinaryMapVariables& variables,
    const BinaryMapTiles& tiles,
    const BinaryMapScripts& scripts,
    const BinaryMapObjectCounts& objects,
    const std::optional<BinaryObjectPrefix>& first_object
);
TextMapExportPlan single_elevation_plan(int elevation);
std::filesystem::path split_output_path(
    const std::filesystem::path& output_dir,
    const std::filesystem::path& input,
    int elevation
);
MapSide parse_side(char side);
void apply_selection(TextMapExportPlan& plan, std::string_view spec);

int parse_stats(const std::filesystem::path& input);
int extract_elevation(const ExtractOptions& options);
int split_elevations(const SplitOptions& options);
int combine_maps(const CombineOptions& options);

} // namespace qmap::cli
