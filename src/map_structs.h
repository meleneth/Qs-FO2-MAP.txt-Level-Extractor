#pragma once
#include <cstdint>

inline constexpr int NAME_LENGTH = 16;

enum map_type {
    EMPTY          = 0,
    MAP_TXT        = 1,
    MAP_MAP        = 2
};

struct map_header
{
    uint32_t version;       // 0x0000 // FO1==19, FO2==20
    char filename[16];      // 0x0004
    int32_t dude_start;     // 0x0014 // 0-39999
    int32_t elev_start;     // 0x0018 // 0-2
    int32_t face_start;     // 0x001C // 0-5
    int32_t lvar_cnt;       // 0x0020 // local vars not map vars
    int32_t map_script_id;  // 0x0024 // -1==no map script, Text string is found in MSG file scrname.msg at index [id + 101].
    int32_t map_flags;      // 0x0028 // see flags enum
    int32_t light_level;    // 0x002C // Map darkness (according to mapper2, not sure if used).
    int32_t mvar_cnt;       // 0x0030 // Number of global variables stored in map. (maybe this means map_vars?)
    int32_t map_id;         // 0x0034 // Fallout 1: Map filename found in map.msg, Fallout 2: Map details found in data/maps.txt in section [Map id]
    uint32_t game_ticks;    // 0x0038 // Time since the epoch. Number of time ticks since the epoch. A time tick is equivalent to 0.1 seconds in game time. The epoch for Fallout 1 is "5 December 2161 00:00am", and for Fallout 2 "25 July 2241 00:00am". 
    int32_t unknown[44];    // 0x003C // QTODO: let's hope I don't have to figure this out
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

    struct map_header   header;
};
