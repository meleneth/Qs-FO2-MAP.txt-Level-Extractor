#pragma once
#include "map_structs.h"
#include "text_map_export.h"

#include <span>

void parse_map_txt(std::span<uint8_t> map, map_lvls* lvls);
void export_map_txt(
    const qmap::TextMapExportPlan& plan,
    map_lvls* map_L,
    map_lvls* map_R,
    char* path
);
