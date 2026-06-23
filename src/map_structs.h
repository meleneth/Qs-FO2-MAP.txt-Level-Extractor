#pragma once
#include "qmap_types.h"

#include <cstddef>
#include <cstdint>
#include <array>
#include <optional>
#include <string>
#include <vector>

struct map_lvls
{
    qmap::MapFileKind map_type = qmap::MapFileKind::empty;
    std::string file_path_storage;
    std::string map_name_storage;
    std::string parse_error;
    std::vector<uint8_t> owned_data;

    int header_size  = 0;
    std::array<std::optional<qmap::Range>, qmap::elevation_count> elevations = {};
    std::optional<qmap::Range> scripts;
    std::optional<qmap::Range> objects;
};
