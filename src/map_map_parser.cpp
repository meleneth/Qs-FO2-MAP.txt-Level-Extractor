// https://falloutmods.fandom.com/wiki/MAP_File_Format
// https://fodev.net/files/fo2/map.html
#include "map_map_parser.h"
#include "B_Endian/B_Endian.h"
#include <cstring>

enum map_flags {
  MAP_IS_SAVEGAME = 0x1, // map is a savegame map (.SAV).
  MAP_ELEV_0 = 0x2,      // map has an elevation at level 0.
  MAP_ELEV_1 = 0x4,      // map has an elevation at level 1.
  MAP_ELEV_2 = 0x8       // map has an elevation at level 2.
};

map_header parse_header(uint8_t *map_file) {
  map_header h;

  h.version = B_Endian::read_u32(&map_file[0]);
  memcpy(h.filename, &map_file[4], 16);
  h.dude_start = B_Endian::read_i32(&map_file[20]);
  h.elev_start = B_Endian::read_i32(&map_file[24]);
  h.face_start = B_Endian::read_i32(&map_file[28]);
  h.lvar_cnt = B_Endian::read_i32(&map_file[32]);
  h.map_script_id = B_Endian::read_i32(&map_file[36]);
  h.map_flags = B_Endian::read_i32(&map_file[40]);
  h.light_level = B_Endian::read_i32(&map_file[44]);
  h.mvar_cnt = B_Endian::read_i32(&map_file[48]);
  h.map_id = B_Endian::read_i32(&map_file[52]);
  h.game_ticks = B_Endian::read_u32(&map_file[56]);
  memcpy(h.unknown, &map_file[60], 4 * 44);

  return h;
}

// parse map.map file header
// QTODO: rename to something better
void parse_map_map(map_lvls *map) {
  if (map) {
    map->header = {};
    map->header_size = 0;
    for (int elevation = 0; elevation < 3; ++elevation) {
      map->level[elevation] = nullptr;
    }
  }

  if (!map || !map->data ||
      map->file_siz < static_cast<int>(sizeof(map_header))) {
    return;
  }

  map_header h = parse_header(map->data);
  map->header_size = sizeof(h);

  // when a level is marked that means there's NO level information (ffs why do
  // it that way?)
  if (!(h.map_flags & MAP_ELEV_0)) {
    // QTODO: replace "Level 0" etc. with proper markers (possibly ptrs to map
    // level data?) QTODO: yeah, this definitely needs a better marker
    map->level[0] = map->label[0];
  }
  if (!(h.map_flags & MAP_ELEV_1)) {
    map->level[1] = map->label[1];
  }
  if (!(h.map_flags & MAP_ELEV_2)) {
    map->level[2] = map->label[2];
  }

  map->header = h;
}
