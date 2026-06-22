#pragma once
#include <cstdint>

inline constexpr int NAME_LENGTH = 16;

enum map_type {
    EMPTY          = 0,
    MAP_TXT        = 1,
    MAP_MAP        = 2
};

struct map_lvls
{
    int   map_type   = 0;
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
