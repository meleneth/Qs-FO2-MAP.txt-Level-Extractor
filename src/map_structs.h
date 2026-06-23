#pragma once
#include "qmap_types.h"

#include <cstdint>
#include <string>
#include <vector>

inline constexpr int NAME_LENGTH = 16;

struct map_lvls
{
    qmap::MapFileKind map_type = qmap::MapFileKind::empty;
    std::string file_path_storage;
    std::string map_name_storage;
    std::string parse_error;
    std::vector<uint8_t> owned_data;
    char* file_str   = nullptr;
    int   file_siz   = 0;     //should not be more than a couple MBs ever
    char* map_name   = nullptr;
    uint8_t* data    = nullptr;

    int header_size  = 0;
    int lvl_sizes[3] = {0};

    char label[3][NAME_LENGTH] = {"Level 1","Level 2","Level 3"};
    char* label_ptr[3] = {label[0],label[1],label[2]};
    char* level[3] = {nullptr,nullptr,nullptr};    // pointers to "square_elev:" entries in data

    char* scripts = nullptr;
    char* objects = nullptr;
};
