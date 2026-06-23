#pragma once
#include "qmap_types.h"

#include <cstdint>
#include <string>
#include <vector>

struct map_lvls
{
    qmap::MapFileKind map_type = qmap::MapFileKind::empty;
    std::string file_path_storage;
    std::string map_name_storage;
    std::string parse_error;
    std::vector<uint8_t> owned_data;
    int   file_siz   = 0;     //should not be more than a couple MBs ever
    uint8_t* data    = nullptr;

    int header_size  = 0;
    int lvl_sizes[3] = {0};

    char* level[3] = {nullptr,nullptr,nullptr};    // pointers to "square_elev:" entries in data

    char* scripts = nullptr;
    char* objects = nullptr;
};
