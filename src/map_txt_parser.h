#pragma once
#include "map_structs.h"

void parse_map_txt(uint8_t* map, map_lvls* lvls);
void export_map_txt(char** label_ptr_M, map_lvls* map_L, map_lvls* map_R, int header, char* path);
