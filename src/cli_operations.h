#pragma once

#include "cli_file_io.h"
#include "cli_selection.h"
#include "cli_stats.h"
#include "text_map_export.h"

#include <filesystem>
#include <string>
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

int parse_stats(const std::filesystem::path& input);
int extract_elevation(const ExtractOptions& options);
int split_elevations(const SplitOptions& options);
int combine_maps(const CombineOptions& options);

} // namespace qmap::cli
