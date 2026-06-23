#pragma once

#include "cli_file_io.h"
#include "cli_selection.h"
#include "cli_stats.h"
#include "binary_map_patch_planner.h"
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

struct ParseStatsOptions {
    std::filesystem::path input;
    std::filesystem::path proto_root;
};

struct ReplaceElevationOptions {
    std::filesystem::path source;
    std::filesystem::path destination;
    std::filesystem::path output;
    std::filesystem::path proto_root;
    int source_elevation = 0;
    int destination_elevation = 0;
    bool dry_run = false;
    bool force = false;
};

std::string format_replace_elevation_plan(
    const ReplaceElevationOptions& options,
    const BinaryReplaceElevationPlan& plan
);
std::string format_replace_elevation_write_success(
    const ReplaceElevationOptions& options,
    std::size_t byte_count
);
std::string format_replace_elevation_failure(
    std::string_view message,
    std::optional<std::size_t> offset = std::nullopt
);

int parse_stats(const std::filesystem::path& input);
int parse_stats(const ParseStatsOptions& options);
int extract_elevation(const ExtractOptions& options);
int split_elevations(const SplitOptions& options);
int combine_maps(const CombineOptions& options);
int replace_elevation(const ReplaceElevationOptions& options);

} // namespace qmap::cli
