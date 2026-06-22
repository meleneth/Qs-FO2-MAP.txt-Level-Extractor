// https://falloutmods.fandom.com/wiki/MAP_File_Format
// https://fodev.net/files/fo2/map.html
#include "map_map_parser.h"
#include "B_Endian/B_Endian.h"
#include <math.h>
#include <memory.h>

enum map_flags {
  MAP_IS_SAVEGAME = 0x1, // map is a savegame map (.SAV).
  MAP_ELEV_0 = 0x2,      // map has an elevation at level 0.
  MAP_ELEV_1 = 0x4,      // map has an elevation at level 1.
  MAP_ELEV_2 = 0x8       // map has an elevation at level 2.
};
enum object_types {
  // The PID is a fundamental type used in the MAP file.
  // It is an identifier for describing objects.
  // It consists of a 4 byte integer of the form:
  //      0xaa00bbbb.
  // The byte aa is the type of the object,
  // while the 2 bytes bbbb are the id of the object.
  // The id is typically an index into a LST file.
  // Valid types include:
  OBJ_ITEM = 0x00,
  OBJ_CRITTER = 0x01,
  OBJ_SCENERY = 0x02,
  OBJ_WALL = 0x03,
  OBJ_TILE = 0x04,
  OBJ_MISC = 0x05,
  OBJ_INTRFACE = 0x06,
  OBJ_INVEN = 0x07,
  OBJ_HEAD = 0x08,
  OBJ_BG = 0x09
};

struct vars {
  int32_t mvars_siz = 0;
  int32_t *mvars = NULL;
  int32_t lvars_siz = 0;
  int32_t *lvars = NULL;
};
struct scenery {
  // scenery instance data
  int32_t flags;       // 0x54  instance flags
  int32_t door_flags;  // 0x58  open/shut flags, stairs/elevator/ladders have
                       // destination_built_tile
  int32_t destination; // 0x5C  destination map/level
};
struct critter {
  // PID_TYPE == 1 // critter
  int32_t flags;            // 0x54  instance flags
  int32_t damage_last_turn; // 0x58  Combat damage tracker        Only valid for
                            // .SAV
  int32_t
      combat_flags; // 0x5C  Combat maneuver flags/state  Only valid for .SAV
  int32_t
      action_points; // 0x60  Current combat action points Only valid for .SAV
  int32_t
      combat_result; // 0x64  Combat result flags/state    Only valid for .SAV
  int32_t ai_packet; // 0x68  Runtime AI packet
  int32_t team;      // 0x6C  Runtime team number
  int32_t last_hit_cid; // 0x70  Combat id of the critter that last hit this
                        // critter, or -1   Only valid for .SAV
  int32_t hit_points;   // 0x74  Current hit points
  int32_t radiation;    // 0x78  Current radiation level
  int32_t poison;       // 0x7C  Current poison level
};
struct exit_grid {
  // exit grid runtime data
  int32_t flags;     // 0x54  instance flags
  int32_t dest_map;  // 0x58  Destination map id.
  int32_t dest_tile; // 0x5C  Destination tile.
  int32_t dest_elev; // 0x60  Destination elevation.
  int32_t dest_dir;  // 0x64  Destination rotation.
};
struct ammo {
  // other object types
  int32_t flags;    // 0x54  instance flags
  int32_t ammo_amt; // 0x58  Current rounds in the stack
  int32_t ammo_pid; // 0x5C
};
struct object {
  int32_t obj_id; // 0x00  Unknown 0. I don't think this is part of the object,
                  // but some kind of separator.
  int32_t obj_tile;  // 0x04  A value of -1 means that the object is not on the
                     // grid (typically it is in an inventory)
  int32_t x;         // 0x08  Pixel x offset
  int32_t y;         // 0x0C  Pixel y offset
  int32_t sx;        // 0x10  cached screen x
  int32_t sy;        // 0x14  cached screen y
  int32_t frame;     // 0x18  current animation frame (for save?)
  int32_t rotation;  // 0x1C  object rotation
  int32_t obj_fid;   // PID?    // 0x20  art FID for object
  int32_t obj_flags; // 0x24  0x01000000 right hand, 0x02000000 left hand,
                     // 0x03000000 armor worn
  int32_t elevation; // 0x28
  int32_t obj_pid;   // 0x2C
  int32_t obj_cid;   // 0x30  combat id (save files?) -1 for normal objects
  int32_t light_radius;    // 0x34  in hexes
  int32_t light_intensity; // 0x38  (0..65536, interpreted as 0-100%)
  int32_t
      outline_color; // 0x3C  Outline color/state in saved maps during combat
  int32_t obj_sid;   // 0x40  runtime script id for this object
  int32_t obj_scr_index; // 0x44  Script index from scripts.lst, or -1.

  int32_t
      inventory_cnt; // 0x48  Number of inventory entries owned by this object
  int32_t inventory_size; // 0x4C  Allocated inventory capacity in the saved
                          // object data
  object *inv_ptr; // 0x50  Serialized pointer placeholder from the original
                   // engine (savegames?)
  // inventory inv;            // 0x50  Serialized pointer placeholder from the
  // original engine
};

struct objects_list {
  int32_t count_total;
  int32_t count_elev[3];
  object *objects;
};

struct tiles {
  uint32_t elev[3][10000];
  // uint32_t elev_0[10000];
  // uint32_t elev_1[10000];
  // uint32_t elev_2[10000];
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

vars parse_map_vars(map_lvls *map, int *offset) {
  uint8_t *data = map->data;
  int mvars_cnt = map->header.mvar_cnt;
  int mvars_siz = mvars_cnt * sizeof(int32_t);
  int lvars_cnt = map->header.lvar_cnt;
  int lvars_siz = lvars_cnt * sizeof(int32_t);

  vars v;
  if (mvars_cnt > 0) {
    v.mvars_siz = mvars_siz;
    v.mvars = (int32_t *)malloc(mvars_siz);
  }
  if (lvars_cnt > 0) {
    v.lvars_siz = lvars_siz;
    v.lvars = (int32_t *)malloc(lvars_siz);
  }

  memcpy(v.mvars, &map->data[*offset], mvars_siz);
  *offset += mvars_siz;
  memcpy(v.lvars, &map->data[*offset], lvars_siz);
  *offset += lvars_siz;

  return v;
}

tiles parse_map_tiles(map_lvls *map, int *offset) {
  int copy_size = 10000 * sizeof(uint32_t);
  // int offset = (map->header.mvar_cnt + map->header.lvar_cnt) *
  // sizeof(int32_t);
  tiles t;

  for (int i = 0; i < 3; i++) {
    if (map->level[i]) {
      memcpy(t.elev[i], &map->data[*offset], copy_size);
      *offset += copy_size;
    } else {
      // QTODO: this needs a better check than -1 since the array is now
      // uint32_t instead of int32_t
      t.elev[i][0] = -1;
    }
  }

  // if (map->level[1]) {
  //     memcpy(t.elev_1, &map->data[236 + *offset_], copy_size);
  //     *offset_ += copy_size;
  // } else {
  //     //QTODO: this needs a better check than -1 since the array is now
  //     uint32_t instead of int32_t t.elev_1[0] = -1;
  // }
  // if (map->level[2]) {
  //     memcpy(t.elev_2, &map->data[236 + *offset_], copy_size);
  //     *offset_ += copy_size;
  // } else {
  //     //QTODO: this needs a better check than -1 since the array is now
  //     uint32_t instead of int32_t t.elev_2[0] = -1;
  // }

  return t;
}

int32_t read_adv(map_lvls *map, int *offset) {
  if ((*offset + sizeof(int32_t)) > map->file_siz) {
    return 0;
  }

  uint8_t *data = map->data;
  int32_t out = B_Endian::read_i32(&data[*offset]);
  *offset += sizeof(int32_t);

  return out;
}

namespace {

int script_record_word_count(int type) {
  int words = 16;
  if (type == SCRIPT_SPATIAL) {
    words += 2;
  }
  if (type == SCRIPT_TIMED) {
    words += 1;
  }
  return words;
}

int script_type_high_byte(int32_t pid) {
  return ((uint32_t)pid) >> 24;
}

bool is_known_script_type(int type) {
  return (type >= SCRIPT_SYSTEM) && (type <= SCRIPT_CRITTER);
}

int script_type_from_pid(int32_t pid) {
  int type = script_type_high_byte(pid);
  if (is_known_script_type(type)) {
    return type;
  }
  return SCRIPT_SYSTEM;
}

void skip_serialized_script_padding_record(map_lvls *map, int *offset,
                                           int section_type, int slot_index) {
  int start_offset = *offset;
  int32_t scr_id = B_Endian::read_i32(&map->data[*offset]);
  int32_t scr_next = B_Endian::read_i32(&map->data[*offset + sizeof(int32_t)]);
  int raw_type = script_type_high_byte(scr_id);
  int inferred_type = script_type_from_pid(scr_id);
  int words = script_record_word_count(inferred_type);

  if ((raw_type != section_type) && is_known_script_type(raw_type)) {
    printf("SUSPECT: script padding slot uses different valid layout; "
           "section_type=%d slot=%d offset=%d scr_id=0x%08X "
           "scr_next=%d raw_type=%d words=%d\n",
           section_type, slot_index, start_offset, (uint32_t)scr_id, scr_next,
           raw_type, words);
  } else if (!is_known_script_type(raw_type)) {
    printf("SUSPECT: script padding slot has unknown id high byte; "
           "section_type=%d slot=%d offset=%d scr_id=0x%08X "
           "scr_next=%d raw_type=%d using_words=%d\n",
           section_type, slot_index, start_offset, (uint32_t)scr_id, scr_next,
           raw_type, words);
  }

  *offset += words * sizeof(int32_t);
}

bool skip_script_padding_and_footer(map_lvls *map, int *offset, int block_count,
                                    int type) {
  for (int i = block_count; i < 16; ++i) {
    skip_serialized_script_padding_record(map, offset, type, i);
  }

  int footer_count = read_adv(map, offset);
  int footer_next = read_adv(map, offset);
  if ((footer_count == block_count) && (footer_next == 0)) {
    return true;
  }

  printf(
      "ERROR: script footer mismatch; type=%d expected_count=%d got_count=%d got_next=%d\n",
      type, block_count, footer_count, footer_next);
  return false;
}

void parse_script_record(map_lvls *map, int *offset, int type, script *out) {
  out->scr_id = read_adv(map, offset);
  out->scr_next = read_adv(map, offset);

  if (type == SCRIPT_SPATIAL) {
    out->spatial_tile = read_adv(map, offset);
    out->spatial_radius = read_adv(map, offset);
  }
  if (type == SCRIPT_TIMED) {
    out->time = read_adv(map, offset);
  }

  out->scr_flags = read_adv(map, offset);
  out->scr_index = read_adv(map, offset);
  out->program_ptr = read_adv(map, offset);
  out->scr_obj_id = (uint32_t)read_adv(map, offset);
  out->lvar_offset = read_adv(map, offset);
  out->lvar_cnt = read_adv(map, offset);
  out->last_used_val = read_adv(map, offset);
  out->current_action = read_adv(map, offset);
  out->fixed_param = read_adv(map, offset);
  out->action_id = read_adv(map, offset);
  out->override_flags = read_adv(map, offset);
  out->unknown_1 = read_adv(map, offset);
  out->how_much = read_adv(map, offset);
  out->unknown_2 = read_adv(map, offset);
}

} // namespace

void parse_map_scripts(map_lvls *map, int *offset,
                       scripts_list scripts[SCRIPT_TYPE_COUNT]) {
  for (int type = SCRIPT_SYSTEM; type < SCRIPT_TYPE_COUNT; ++type) {
    scripts[type].count = read_adv(map, offset);
    if (scripts[type].count <= 0) {
      scripts[type].scripts = nullptr;
      continue;
    }

    scripts[type].scripts =
        (script *)calloc(scripts[type].count, sizeof(script));

    int remaining = scripts[type].count;
    int parsed = 0;

    while (remaining > 0) {
      const int block_count = (remaining > 16) ? 16 : remaining;
      for (int i = 0; i < block_count; ++i) {
        parse_script_record(map, offset, type,
                            &scripts[type].scripts[parsed++]);
      }
      remaining -= block_count;

      if (remaining > 0) {
        read_adv(map, offset); // number of records in this full 16-record block
        read_adv(
            map,
            offset); // serialized next-block pointer from the original engine
      } else if (block_count < 16) {
        if (!skip_script_padding_and_footer(map, offset, block_count, type)) {
          return;
        }
      } else {
        read_adv(map, offset); // final full-block record count
        read_adv(map, offset); // serialized next-block pointer
      }
    }
  }
}

objects_list parse_map_objects(map_lvls *map, int *offset) {
  uint8_t *data_ptr = &map->data[*offset];
  objects_list ol;

  ol.count_total = B_Endian::read_i32(&data_ptr[0x00]);

  ol.count_elev[0] = B_Endian::read_i32(&data_ptr[0x04]);
  ol.count_elev[1] = B_Endian::read_i32(&data_ptr[0x08]);
  ol.count_elev[2] = B_Endian::read_i32(&data_ptr[0x0C]);

  ol.objects = (object *)malloc(sizeof(object) * ol.count_total);
  object *obj = ol.objects;

  for (int elev = 0; elev < 3; elev++) {
    for (int i = 0; i < ol.count_elev[elev]; i++) {
      obj[i].obj_id = B_Endian::read_i32(&data_ptr[0x00]);
      obj[i].obj_tile = B_Endian::read_i32(&data_ptr[0x04]);
      obj[i].x = B_Endian::read_i32(&data_ptr[0x08]);
      obj[i].y = B_Endian::read_i32(&data_ptr[0x0C]);
      obj[i].sx = B_Endian::read_i32(&data_ptr[0x10]);
      obj[i].sy = B_Endian::read_i32(&data_ptr[0x14]);
      obj[i].frame = B_Endian::read_i32(&data_ptr[0x18]);
      obj[i].rotation = B_Endian::read_i32(&data_ptr[0x1C]);
      obj[i].obj_fid = B_Endian::read_i32(&data_ptr[0x20]);
      obj[i].obj_flags = B_Endian::read_i32(&data_ptr[0x24]);
      obj[i].elevation = B_Endian::read_i32(&data_ptr[0x28]);
      obj[i].obj_pid = B_Endian::read_i32(&data_ptr[0x2C]);
      obj[i].obj_cid = B_Endian::read_i32(&data_ptr[0x30]);
      obj[i].light_radius = B_Endian::read_i32(&data_ptr[0x34]);
      obj[i].light_intensity = B_Endian::read_i32(&data_ptr[0x38]);
      obj[i].outline_color = B_Endian::read_i32(&data_ptr[0x3C]);
      obj[i].obj_sid = B_Endian::read_i32(&data_ptr[0x40]);
      obj[i].obj_scr_index = B_Endian::read_i32(&data_ptr[0x44]);

      obj[i].inventory_cnt = B_Endian::read_i32(&data_ptr[0x48]);
      obj[i].inventory_size = B_Endian::read_i32(&data_ptr[0x4C]);

      if (obj[i].inventory_cnt > 0) {
        // obj[i].inv.inv_ptr = (object*)malloc(obj[i].inventory_size);
      }
      // QTODO: parse inventory

      switch ((obj[i].obj_pid >> 24) & 0x7) {
      case OBJ_ITEM:
        // parse item
        break;
      case OBJ_CRITTER:
        // parse critter
        break;
      case OBJ_SCENERY:
        // parse scenery
        break;
      case OBJ_WALL:
        // parse wall
        break;
      case OBJ_TILE:
        // parse tile
        break;
      case OBJ_MISC:
        // parse misc
        break;
      case OBJ_INTRFACE:
        // parse interface
        break;
      case OBJ_INVEN:
        // parse inventory
        break;
      case OBJ_HEAD:
        // parse head?
        break;
      case OBJ_BG:
        // parse background
        break;

      default:
        printf("ERROR: Object type not recognized.");
        break;
      }
    }
  }

  return ol;
}

// BakerStaunch
// --Yeah, so your object struct could just be a pointer to
// where in that data the object is (or possibly better an offset)
//  & a length and then whatever fields you might want to manipulate
// --Then when you read an object, you pull out the pid type and
// flags or whatever you need to determine the length,
// and create the object struct from those
// --One thing that might make deserialization a little simpler
// is having a struct which is just the file data pointer,
// a size and a cursor of where you're up to in the file
// (and possibly flags if you want to do some extra error handling),
// then your B_Endian::read* functions can take that and adjust the
// cursor position (and verify they won't try to read more data than
//  what is in the file)

void export_map_map(char **label_ptr_M, map_lvls *map_L, map_lvls *map_R,
                    int header, char *path_buff) {
  map_header *head = (header == 0) ? &map_L->header : &map_R->header;
  int H_size = (header == 0) ? map_L->header_size : map_R->header_size;
  int offset = sizeof(map_header);

  vars v_L = parse_map_vars(map_L, &offset);
  // vars v_R = parse_map_vars(map_R, &offset);

  tiles t_L = parse_map_tiles(map_L, &offset);
  // tiles t_R = parse_map_tiles(map_R, &offset);

  scripts_list s_L[SCRIPT_TYPE_COUNT];
  parse_map_scripts(map_L, &offset, s_L);
  // scripts_list s_R = parse_map_scripts(map_R, &offset);

  objects_list o_L = parse_map_objects(map_L, &offset);
  // objects_list o_R = parse_map_objects(map_R, &offset);
}
