//https://falloutmods.fandom.com/wiki/Merging_maps
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "map_txt_parser.h"
#include "text_map_parser.h"
#include "io_Platform.h"

char* find_str(uint8_t* map_txt, char* str, int len)
{
    //TODO: check if map.data (what's being passed as map_txt)
    //      has a "\0" terminator so string parsing works
    //      else use map.file_siz
    //TODO: just make this a normal find_string function
    char* str_start = nullptr;
    char* map_str   = (char*)map_txt;
    int map_len     = strlen(map_str);
    int str_len     = len;
    for (size_t i = 0; i < map_len; i++)
    {
        if (map_str[i] == str[0]) {
            if (len > 2) {
                if (map_str[i+2] != str[2]) {
                    continue;
                }
            }
            if (io_strncasecmp(&map_str[i], str, str_len) == 0) {
                str_start = &map_str[i];
                break;
            }
        }
    }
    return str_start;
}

char* find_level_marker(uint8_t* map_data, int level, int* marker_len)
{
    char buff[24];
    snprintf(buff, 24, "square_elev: %d\r\n\r\n", level);
    char* tmp = find_str(map_data, buff, strlen(buff));
    if (tmp) {
        *marker_len = strlen(buff);
        return tmp;
    }

    snprintf(buff, 24, "square_elev: %d\n\n", level);
    tmp = find_str(map_data, buff, strlen(buff));
    if (tmp) {
        *marker_len = strlen(buff);
        return tmp;
    }

    *marker_len = 0;
    return nullptr;
}
// TEMP_COMPAT: legacy GUI/export adapter. New code should call
// qmap::parse_text_map(std::string_view) and use ranges instead of map_lvls
// interior char pointers. Remove after GUI/export migrate to ParsedTextMap.
void parse_map_txt(uint8_t* map_data, map_lvls* map)
{
    if (!map_data) {
        return;
    }
    if (!map) {
        return;
    }

    map->data = map_data;
    auto parsed = qmap::parse_text_map(
        std::string_view{
            reinterpret_cast<const char*>(map_data),
            static_cast<std::size_t>(map->file_siz)
        }
    );
    if (!parsed) {
        printf("ERROR: %s\n", parsed.error().message.c_str());
        return;
    }

    auto* base = reinterpret_cast<char*>(map_data);
    map->header_size = static_cast<int>(parsed.value().header.size);
    for (int elevation = 0; elevation < qmap::elevation_count; ++elevation) {
        if (!parsed.value().elevations[elevation]) {
            map->level[elevation] = nullptr;
            map->lvl_sizes[elevation] = 0;
            continue;
        }

        const auto range = *parsed.value().elevations[elevation];
        map->level[elevation] = base + range.offset;
        map->lvl_sizes[elevation] = static_cast<int>(range.size);
    }
    map->scripts = base + parsed.value().scripts.offset;
    map->objects = base + parsed.value().objects.offset;
}

//assigns level string sizes
//some levels might not exist
//so we have to check each one in turn
// TEMP_COMPAT: parse_map_txt now computes these values through ParsedTextMap.
// Keep this as a no-op until legacy callers stop invoking it.
void map_level_sizes(map_lvls* map)
{
    (void)map;
}

// returns a string of all objects located on the level passed in
char* parse_objects(map_lvls* map, int level)
{
    char* begin = nullptr;
    char* end   = nullptr;
    int objects_size = map->file_siz - (map->objects - (char*)map->data);
    char* objects_str = (char*)malloc(objects_size);
    char* objects_ptr = objects_str;
    for (size_t i = 0; i < objects_size; i++)
    {
        if ((map->objects[i] != '[') && (map->objects[i] != 'o')) {
            continue;
        }
        if (io_strncmp(&map->objects[i], "[OBJECT BEGIN]", sizeof("[OBJECT BEGIN]")-1) == 0) {
            begin = &map->objects[i];
            i += sizeof("[OBJECT BEGIN]");
            continue;
        }
        if (io_strncmp(&map->objects[i], "obj_elev: ", sizeof("obj_elev: ")-1) == 0) {
            char l = level + '0';
            char o = map->objects[i + sizeof("obj_elev: ")-1];
            if (o != l) {
                begin = nullptr;
                continue;
            }
        }
        if (io_strncmp(&map->objects[i], "[BEGIN INVEN ITEMS]", sizeof("[BEGIN INVEN ITEMS]")-1) == 0) {
            // Apparently a critter's inventory is stored inside their own object description
            // And those are all marked [OBJECT BEGIN], so it breaks the current parsing
            // So here we fast forward to the end of the inventory list before returning to parsing
            while (i < objects_size)
            {
                i++;
                if (map->objects[i] != '[') {
                    continue;
                }
                if (map->objects[i+1] != 'E') {
                    continue;
                }
                if (io_strncmp(&map->objects[i], "[END INVEN ITEMS]", sizeof("[END INVEN ITEMS]")-1) == 0) {
                    break;
                }
            }
        }
        if (io_strncmp(&map->objects[i], "[OBJECT END]", sizeof("[OBJECT END]")-1) == 0) {
            if (begin == nullptr) {
                continue;
            }
            end = &map->objects[i + sizeof("[OBJECT END]") + 3];
            int size = end - begin;
            memcpy(objects_ptr, begin, size);
            objects_ptr += size;
        }
    }

    objects_ptr[0] = '\0'; // nullptr terminate the string

    return objects_str;
}

// if not on this level return 0
// if on this level return length of script string
int script_spatial(char* script_txt, int remainder, int level)
{
    int built_tile_len = sizeof("built_tile: ")        -1;
    int radius_len     = sizeof("sp.radius: ")         -1;
    int scr_id_len     = sizeof("scr_id: ")            -1;
    int scr_num_len    = sizeof("scr_num: ")           -1;
    int objects_len    = sizeof(">>>>>>>>>>: OBJECTS") -1;

    for (size_t i = 0; i < remainder; i++) {
        if (script_txt[i] != 'b') {
            continue;
        }
        if (io_strncmp(&script_txt[i], "built_tile: ", built_tile_len) != 0) {
            continue;
        }

        int spatial_level = -1;
        int spatial_tile = atoi(&script_txt[i + built_tile_len]);
        //TODO: make this look like the script parsing switch in parse_scripts
        if ((spatial_tile >= 0) && (spatial_tile < 40000)) {
            spatial_level = 0;
        } else
        if (spatial_tile & 0x20000000) {
            spatial_level = 1;
        } else
        if (spatial_tile & 0x40000000) {
            spatial_level = 2;
        } else {
            printf("ERROR: something went wrong while parsing spatial scripts\n");
            return 0;
        }

        if (spatial_level != level) {
            return 0;
        }

        while (i++ < remainder) {
            if ((script_txt[i] != 's') && (script_txt[i] != '>')) {
                continue;
            }
            if (io_strncmp(&script_txt[i], "sp.radius: ", radius_len) == 0) {
                while (script_txt[i++] != '\n') {}
                return i+2;     //want to capture trailing "/r/n"
            }
            if ((io_strncmp(&script_txt[i], "scr_id: ",            scr_id_len ) == 0)
            ||  (io_strncmp(&script_txt[i], "scr_num: ",           scr_num_len) == 0)
            ||  (io_strncmp(&script_txt[i], ">>>>>>>>>>: OBJECTS", objects_len) == 0)) {
                return (i-1);
            }
        }
    }

    printf("ERROR reached end of script_txt before finding SPATIAL script ending\n");
    return 0;
}

//TODO: maybe parse the object obj_sid values into an int array?
//      then check the array instead of doing a string search here
//      (would definitely be more reliable)
//this currently checks if the passed in script id
// is in the passed in string of objects
bool check_object_level(char* id, char* objects)
{
    int id_len = 0;
    while ((id[id_len] >= '0') && id[id_len] <= '9') {
        id_len++;
    }

    char* found = find_str((uint8_t*)objects, id, id_len);
    if (found) {
        return true;
    }

    return false;
}

int script_object(char* script_txt, int remainder, int level, char* objects)
{
    int scr_id_len  = sizeof("scr_id: ")            -1;

    // objects being passed in are only on this level,
    // so only scripts attached to those objects should be copied
    if (check_object_level(&script_txt[scr_id_len], objects) == false) {
        return 0;
    }

    for (size_t i = 0; i < remainder; i++) {
        if (script_txt[i] != 's') {
            continue;
        }

        if (io_strncmp(&script_txt[i], "scr_num_local_vars: ", sizeof("scr_num_local_vars: ")-1) == 0) {
            while (script_txt[i++] != '\n') {}
            return i;
        }

    }

    printf("ERROR reached end of script_txt before finding OBJECT script ending\n");
    return 0;
}

#define uint  unsigned int
struct parsed_scripts
{
    char* scr_num[5]   = {0};
    int scr_num_cnt[5] = {0};
};
struct spatial_script
{
    uint scr_id               = 0;
    uint scr_next             = 0;
    uint scr_flags            = 0;
    uint scr_script_idx       = 0;
    uint scr_oid              = 0;
    uint scr_local_var_offset = 0;
    uint scr_num_local_vars   = 0;

    uint built_tile           = 0;

    uint radius               = 0;
};



parsed_scripts parse_scripts(map_lvls* map, char* objects, int level)
{
    // >>>>>>>>>>: SCRIPTS <<<<<<<<<<


    // SCRS:
    // scr_num: 0      //  ==> s_system
    // scr_num: 0      //  ==> spatial scripts
    // scr_num: 0      //  ==> s_time
    // scr_num: 0      //  ==> all non-critter objects
    // scr_num: 0      //  ==> all critter objects
    //
    // typedef enum ScriptType {                // ==> from fallout2-re scripts.h
    //     SCRIPT_TYPE_SYSTEM,  // s_system     // ==> 0 ==> seems like the most likely match
    //     SCRIPT_TYPE_SPATIAL, // s_spatial    // ==> 1
    //     SCRIPT_TYPE_TIMED,   // s_time       // ==> 2 ==> not sure how to get system or timed scripts tho
    //     SCRIPT_TYPE_ITEM,    // s_item       // ==> 3     its unlikely that system/timed scripts are sorted by level tho
    //     SCRIPT_TYPE_CRITTER, // s_critter    // ==> 4     so it shouldn't be necessary to track them? I hope?
    //     SCRIPT_TYPE_COUNT,                   // ==> 5
    // } ScriptType;


    // [[SCRIPT]]           //  ==> immediately follows the scr_num: count for each script type
    //                      //  ==> occurs every 16 script entries in each scr_num type

    // scr_id: 50331652     // ==> matches obj_sid on the object its attached to 
    //                      // ==> is not the same as an id for the script,
    //                      //     but more like an id for this particular instance of the script
    //                      //     for that reason each object should have a unique obj_sid matching scr_id
    //                      //     even if the same script is reused for multiple objects
    //                      // ==> seems to be generated whenever a script is attached
    // scr_id: 67108864     // ==> 0x04000000  ==> indicates a critter object is attached?
    // scr_id: 50331648     // ==> 0x03000000  ==> all other object types?
    // scr_id: 16777216     // ==> 0x01000000  ==> spatial script types?
    //                      // ==> 0x00 and 0x02 appear to be unused?


    /* spatial scripts have these extra fields */
    // scr_id: 16777216                     // ==> 0x01000000  ==> spatial script types
    // ...
    // scr_udata.sp.built_tile: 39999       // ==> highest tile number for level 0, starts at 0
    // scr_udata.sp.radius: 0
    /*                                         */

    // scr_udata.sp.built_tile: 536870912   // ==> 0x020000000  ==> start tile for level 1
    // scr_udata.sp.built_tile: 1073741824  // ==> 0x040000000  ==> start tile for level 2


    // [OBJECT BEGIN]
    // ...
    // obj_sid: 4294967295        //  ==> 0xffffffff  ==> no assigned script
    // ...                                            ==> otherwise matches scr_id on script
    // [OBJECT END]                                   ==> should be unique per map



    //hooo boy, ok this guy is going to need a separate string for each script type
    //      (just in case the scr_num entries need to be properly tracked) (they do)
    //  also spatial script type needs to be separated by which level they're located on
    //      and then re-assigned to the new level for the new map
    //  also we need to make sure any scr_id/obj_sid pair don't overlap other pairs


    int scripts_size = map->objects - map->scripts;

    parsed_scripts scrs;

    for (size_t type = 0; type < 5; type++) {
        if ((type == SCRIPT_SYSTEM) || (type == SCRIPT_TIMED)) {
            scrs.scr_num[type] = nullptr;
            continue;
        }
        scrs.scr_num[type] = (char*)malloc(scripts_size);
    }
    char* scr_ptr[5] = {
        scrs.scr_num[0],
        scrs.scr_num[1],
        scrs.scr_num[2],
        scrs.scr_num[3],
        scrs.scr_num[4]
        };


    for (size_t i = 0; i < scripts_size; i++)
    {
        int script_len = 0;
        if (map->scripts[i] != 's') {
            // since "[[SCRIPT]]" is used every 16 entries for each scr_num count
            //  it's not useful to sort with
            // while sorting by "scr_num:" is possible by incrementing per type,
            // this simply checks each script by parsing the scr_id for type
            continue;
        }

        if (io_strncmp(&map->scripts[i], "scr_id: ", sizeof("scr_id: ")-1) == 0) {
            int scr_id = atoi(&map->scripts[i +sizeof("scr_id: ")-1]);
            switch ((scr_id >> 24) & 0x7)   //need only these 3 bits to check type
            {
            case (SCRIPT_SYSTEM): { //  (currently not handled)
                printf("ERROR Caught a SYSTEM script?\n");
                break;
                }
            case (SCRIPT_SPATIAL): {
                int spatial_len = script_spatial(&map->scripts[i], scripts_size - i, level);
                if (spatial_len > 0) {
                    strncpy(scr_ptr[SCRIPT_SPATIAL], &map->scripts[i], spatial_len);
                    scr_ptr[SCRIPT_SPATIAL]          += spatial_len;
                    scrs.scr_num_cnt[SCRIPT_SPATIAL] += 1;
                    i                                += spatial_len - 1;
                }
                break;
                }
            case (SCRIPT_TIMED): {  //  (currently not handled)
                printf("ERROR Caught a TIMER script?\n");
                break;
                }
            case (SCRIPT_OBJECTS): {
                int object_len = script_object(&map->scripts[i], scripts_size - i, level, objects);
                if (object_len > 0) {
                    strncpy(scr_ptr[SCRIPT_OBJECTS], &map->scripts[i], object_len);
                    snprintf(scr_ptr[SCRIPT_OBJECTS] + object_len, 3, "\r\n");
                    scr_ptr[SCRIPT_OBJECTS]          += object_len + 2;
                    scrs.scr_num_cnt[SCRIPT_OBJECTS] += 1;
                    i                                += object_len;
                }
                break;
                }
            case (SCRIPT_CRITTER): {
                int critter_len = script_object(&map->scripts[i], scripts_size - i, level, objects);
                if (critter_len > 0) {
                    strncpy(scr_ptr[SCRIPT_CRITTER], &map->scripts[i], critter_len);
                    snprintf(scr_ptr[SCRIPT_CRITTER] + critter_len, 3, "\r\n");
                    scr_ptr[SCRIPT_CRITTER]          += critter_len + 2;
                    scrs.scr_num_cnt[SCRIPT_CRITTER] += 1;
                    i                                += critter_len - 1;
                }
                break;
                }
            default:
                printf("ERROR script type not handled\n");
                break;
            }
        }
    }

    return scrs;
}

void check_char_scripts(parsed_scripts* scripts, char** objects, script_type type)
{
    char* script_ptr = nullptr;
    uint scr_id = 0;
    uint max_id = 0;


    for (size_t el_out = 0; el_out < 3; el_out++)
    {
        for (size_t i = 0; i < strlen(scripts[el_out].scr_num[type]); i++)
        {
            bool match_found = false;
            if (scripts[el_out].scr_num[type][i] != 's') {
                continue;
            }
            if (io_strncmp(&scripts[el_out].scr_num[type][i], "scr_id: ", sizeof("scr_id: ")-1) != 0) {
                continue;
            }



            script_ptr = &scripts[el_out].scr_num[type][i];
            scr_id = atoi(&script_ptr[sizeof("scr_id: "-1)]);

            for (size_t el_in = 0; el_in < 3; el_in++)
            {
                for (; i < strlen(script_ptr); i++)
                {
                    if (scripts[el_in].scr_num[type][i] != 's') {
                        continue;
                    }
                    if (io_strncmp(&scripts[el_in].scr_num[type][i], "scr_id: ", sizeof("scr_id: ")-1) != 0) {
                        continue;
                    }

                    uint tmp = atoi(&scripts[el_in].scr_num[type][i + sizeof("scr_id: ")-1]);
                    if (max_id < tmp) {
                        max_id = tmp;
                    }
                    if (scr_id == tmp) {
                        match_found = true;
                    }
                }
            }

            if (match_found) {
                max_id++;

                char tmp[32];
                snprintf(tmp, 32, "%u", scr_id);

                char* obj_sid =  find_str((uint8_t*)objects[el_out], tmp, strlen(tmp));

                snprintf(tmp, 32, "%u", max_id);
                memcpy(obj_sid, tmp, strlen(tmp));
                memcpy(&script_ptr[sizeof("scr_id: "-1)], tmp, strlen(tmp));
            } else {
                printf("ERROR Can't find object that scr_id %u is attached to.\n", scr_id);
            }
        }
    }

}

void export_map_txt(char** label_ptr_M, map_lvls* map_L, map_lvls* map_R, int header, char* path)
{
    if (header == -1) {
        return;
    }

    uint8_t* head   = (header == 0) ? map_L->data : map_R->data;
    int   H_size    = (header == 0) ? map_L->header_size : map_R->header_size;
    char* out_lvl[3]      = {0};
    int   out_lvl_size[3] = {0};

    for (size_t i = 0; i < 3; i++) {
        if (io_strncmp(label_ptr_M[i], "empty", NAME_LENGTH) == 0) {
            continue;
        }
        for (size_t j = 0; j < 3; j++) {
            if (io_strncmp(label_ptr_M[i], map_L->label_ptr[j], NAME_LENGTH) == 0) {
                out_lvl[i] = map_L->level[j];
                out_lvl_size[i] = map_L->lvl_sizes[j];
            } else
            if (io_strncmp(label_ptr_M[i], map_R->label_ptr[j], NAME_LENGTH) == 0) {
                out_lvl[i] = map_R->level[j];
                out_lvl_size[i] = map_R->lvl_sizes[j];
            }
        }
    }

    int out_size = map_L->file_siz + map_R->file_siz;
    uint8_t* out_map = (uint8_t*)malloc(out_size);
    char* out_ptr = (char*)out_map;
    //header
    int remainder = snprintf(out_ptr, H_size+1, "%s", (char*)head);
    out_ptr += H_size;
    //map tiles
    for (size_t i = 0; i < 3; i++) {
        if (out_lvl[i]) {
            snprintf(
                out_ptr, out_lvl_size[i] + sizeof("square_elev: i\r\n\r\n"),
                "square_elev: %i\r\n\r\n%s",
                i, out_lvl[i]
            );
            out_ptr += out_lvl_size[i] + sizeof("square_elev: i\r\n\r\n") -1; //"-1" to over-write the '\0' character
        }
    }

    // >>>>>>>>>>: OBJECTS <<<<<<<<<<

    // copy object strings from source maps into char* allocated memory
    // only objects on copied levels are copied over
    char* objects[3] = {0};
    parsed_scripts scripts[3] = {0};
    for (size_t dst_lvl = 0; dst_lvl < 3; dst_lvl++) {
        if (label_ptr_M[dst_lvl][0] == '\0') {
            continue;
        }
        if (io_strncmp(label_ptr_M[dst_lvl], "empty", NAME_LENGTH) == 0) {
            continue;
        }
        for (size_t src_lvl = 0; src_lvl < 3; src_lvl++) {
            if (io_strncmp(label_ptr_M[dst_lvl], map_L->label_ptr[src_lvl], NAME_LENGTH) == 0) {
                objects[dst_lvl] = parse_objects(map_L, src_lvl);
                // need to know which objects on source level when parsing object scripts
                scripts[dst_lvl] = parse_scripts(map_L, objects[dst_lvl], src_lvl);
            } else
            if (io_strncmp(label_ptr_M[dst_lvl], map_R->label_ptr[src_lvl], NAME_LENGTH) == 0) {
                objects[dst_lvl] = parse_objects(map_R, src_lvl);
                // need to know which objects on source level when parsing object scripts
                scripts[dst_lvl] = parse_scripts(map_R, objects[dst_lvl], src_lvl);
            }
        }
    }


    //>>>>>>>>>>: SCRIPTS <<<<<<<<<<
    spatial_script* spatials[3] = {0};
    int spatial_cnt_0 = scripts[0].scr_num_cnt[SCRIPT_SPATIAL];
    int spatial_cnt_1 = scripts[1].scr_num_cnt[SCRIPT_SPATIAL];
    int spatial_cnt_2 = scripts[2].scr_num_cnt[SCRIPT_SPATIAL];
    spatials[0] = (spatial_script*)malloc(spatial_cnt_0 * sizeof(spatial_script));
    spatials[1] = (spatial_script*)malloc(spatial_cnt_1 * sizeof(spatial_script));
    spatials[2] = (spatial_script*)malloc(spatial_cnt_2 * sizeof(spatial_script));
    // assign new levels to spatial scripts
    for (size_t elevation = 0; elevation < 3; elevation++) {
        if (!scripts[elevation].scr_num_cnt[SCRIPT_SPATIAL]) {
            continue;
        }


        char* scr_ptr = scripts[elevation].scr_num[SCRIPT_SPATIAL];
        int len = strlen(scr_ptr);
        int j = 0;
        for (size_t i = 0; i < len; i++) {
            if (scr_ptr[i] != 's') {
                continue;
            }

            // scr_id: 16777216
            if (io_strncmp(&scr_ptr[i], "scr_id: ", sizeof("scr_id")) == 0) {
                i += sizeof("scr_id");
                spatials[elevation][j].scr_id = atoi(&scr_ptr[i]);
                while (scr_ptr[i++] != '\n');   //go to next line
            }
            // scr_next: 4294967295
            if (io_strncmp(&scr_ptr[i], "scr_next: ", sizeof("scr_next:")) == 0) {
                i += sizeof("scr_next:");
                spatials[elevation][j].scr_next = atoi(&scr_ptr[i]);
                while (scr_ptr[i++] != '\n');   //go to next line
            }
            // scr_flags: 0
            if (io_strncmp(&scr_ptr[i], "scr_flags: ", sizeof("scr_flags:")) == 0) {
                i += sizeof("scr_flags:");
                spatials[elevation][j].scr_flags = atoi(&scr_ptr[i]);
                while (scr_ptr[i++] != '\n');   //go to next line
            }
            // scr_script_idx: 221
            if (io_strncmp(&scr_ptr[i], "scr_script_idx: ", sizeof("scr_script_idx:")) == 0) {
                i += sizeof("scr_script_idx:");
                spatials[elevation][j].scr_script_idx = atoi(&scr_ptr[i]);
                while (scr_ptr[i++] != '\n');   //go to next line
            }
            // scr_oid: 1919251315
            if (io_strncmp(&scr_ptr[i], "scr_oid: ", sizeof("scr_oid:")) == 0) {
                i += sizeof("scr_oid:");
                spatials[elevation][j].scr_oid = atoi(&scr_ptr[i]);
                while (scr_ptr[i++] != '\n');   //go to next line
            }
            // scr_local_var_offset: 4294967295
            if (io_strncmp(&scr_ptr[i], "scr_local_var_offset: ", sizeof("scr_local_var_offset:")) == 0) {
                i += sizeof("scr_local_var_offset:");
                spatials[elevation][j].scr_local_var_offset = atoi(&scr_ptr[i]);
                while (scr_ptr[i++] != '\n');   //go to next line
            }
            // scr_num_local_vars: 0
            if (io_strncmp(&scr_ptr[i], "scr_num_local_vars: ", sizeof("scr_num_local_vars:")) == 0) {
                i += sizeof("scr_num_local_vars:");
                spatials[elevation][j].scr_num_local_vars = atoi(&scr_ptr[i]);
                while (scr_ptr[i++] != '\n');   //go to next line
            }
            if (scr_ptr[i] == '\r') {
                while (scr_ptr[i++] != '\n');
            }
            // scr_udata.sp.built_tile: 21925
            if (io_strncmp(&scr_ptr[i], "scr_udata.sp.built_tile: ", sizeof("scr_udata.sp.built_tile:")) == 0) {
                i += sizeof("scr_udata.sp.built_tile:");
                spatials[elevation][j].built_tile = atoi(&scr_ptr[i]);
                while (scr_ptr[i++] != '\n');   //go to next line
            }
            if (scr_ptr[i] == '\r') {
                while (scr_ptr[i++] != '\n');
            }
            // scr_udata.sp.radius: 0
            if (io_strncmp(&scr_ptr[i], "scr_udata.sp.radius: ", sizeof("scr_udata.sp.radius:")) == 0) {
                i += sizeof("scr_udata.sp.radius:");
                spatials[elevation][j].radius = atoi(&scr_ptr[i]);
                while (scr_ptr[i++] != '\n');   //go to next line
            }

            //actually assign the new level here
            uint tile = spatials[elevation][j].built_tile;
            //        20004FE5
            tile &= 0x0fffffff;
            if (elevation != 0) {
                tile |= (0x1 << (28 + elevation));
            }
            spatials[elevation][j].built_tile = tile;

            j++;
        }
    }



    //>>>>>>>>>>: OBJECTS <<<<<<<<<<

    // assign new elevations (matching new map) to copied objects
    for (size_t elevation = 0; elevation < 3; elevation++)
    {
        if (!objects[elevation]) {
            continue;
        }

        char* obj_ptr = objects[elevation];
        int len = strlen(obj_ptr);
        int obj_elev_len = sizeof("obj_elev: ")-1;  //-1 to account for the auto-appended '\0' character
        for (size_t j = 0; j < len; j++) {
            if (obj_ptr[j] != 'o') {
                continue;
            }
            if (io_strncmp(&obj_ptr[j], "obj_elev: ", obj_elev_len) == 0) {
                j += obj_elev_len;
                obj_ptr[j] = elevation + '0';
            }
        }
    }


    //time to sort the scr_id/obj_sid entries so they don't overlap
    for (size_t type = 0; type < 5; type++) {

        if (type != 1) {
            continue;
        }

        uint highest_id  = 0;
        bool reassign_id = false;

        //loop through all 3 levels
        for (size_t el_out = 0; el_out < 3; el_out++)
        {
            //loop through all the scripts on this level
            int script_cnt_out = scripts[el_out].scr_num_cnt[type];
            for (size_t i = 0; i < script_cnt_out; i++)
            {
                //check outside script against all other scripts in all 3 inside levels
                if (type == SCRIPT_OBJECTS) {                   //Object scripts only
                    check_char_scripts(scripts, objects, SCRIPT_OBJECTS);
                }
                if (type == SCRIPT_CRITTER) {                   //Critter scripts only
                    check_char_scripts(scripts, objects, SCRIPT_CRITTER);
                }
                if (type == SCRIPT_SPATIAL) {                   //Spatial Scripts only
                    uint scr_id = spatials[el_out][i].scr_id;
                    for (size_t el_in = el_out; el_in < 3; el_in++)
                    {
                        //loop through all inside scripts on this level and actually do the check
                        int script_cnt_in = scripts[el_in].scr_num_cnt[type];
                        //if inside same elevation as first script, start at next script up
                        //else just start at 0
                        size_t j = (el_out == el_in) ? i+1 : 0;
                        for (; j < script_cnt_in; j++)
                        {
                            uint check = spatials[el_in][j].scr_id;
                            if (scr_id == check) {
                                reassign_id = true;
                            }
                            if (highest_id < spatials[el_in][j].scr_id) {
                                highest_id = spatials[el_in][j].scr_id;
                            }
                        }
                    }
                }

                if (reassign_id) {
                    highest_id += 1;
                    if (type == SCRIPT_SPATIAL) {
                        spatials[el_out][i].scr_id = highest_id;
                    }
                }
            }
        }
    }


    int amt = 
    snprintf(
        out_ptr, out_size - (out_ptr - (char*)out_map),
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\r\n\r\n\r\n"
        "SCRS:\r\n"
    );
    out_ptr += amt;


    //print out scripts
    for (size_t i = 0; i < 5; i++) {
        int total = scripts[0].scr_num_cnt[i] + scripts[1].scr_num_cnt[i] + scripts[2].scr_num_cnt[i];
        int abc = 
        snprintf(
            out_ptr, out_size - (out_ptr - (char*)out_map),
            "scr_num: %i\r\n", total
        );
        out_ptr += strlen(out_ptr);

        for (size_t elevation = 0; elevation < 3; elevation++) {
            if (scripts[elevation].scr_num_cnt[i]) {    //TODO: remove this if() check
                if (i == SCRIPT_SPATIAL) {
                    for (size_t cnt = 0; cnt < scripts[elevation].scr_num_cnt[i]; cnt++)
                    {
                        snprintf(
                            out_ptr, out_size - (out_ptr - (char*)out_map),
                            "\r\n"
                            "scr_id: %u\r\n"
                            "scr_next: %u\r\n"
                            "scr_flags: %u\r\n"
                            "scr_script_idx: %u\r\n"
                            "scr_oid: %u\r\n"
                            "scr_local_var_offset: %u\r\n"
                            "scr_num_local_vars: %u\r\n\r\n"
                            "scr_udata.sp.built_tile: %u\r\n\r\n"
                            "scr_udata.sp.radius: %u\r\n"
                            ,
                            spatials[elevation][cnt].scr_id,
                            spatials[elevation][cnt].scr_next,
                            spatials[elevation][cnt].scr_flags,
                            spatials[elevation][cnt].scr_script_idx,
                            spatials[elevation][cnt].scr_oid,
                            spatials[elevation][cnt].scr_local_var_offset,
                            spatials[elevation][cnt].scr_num_local_vars,
                            spatials[elevation][cnt].built_tile,
                            spatials[elevation][cnt].radius
                        );
                        out_ptr += strlen(out_ptr);
                    }
                } else {
                    int remainder = out_size - (out_ptr - (char*)out_map);
                    // scripts[elevation].
                    snprintf(
                        out_ptr, remainder,
                        "\r\n%s", scripts[elevation].scr_num[i]
                    );
                    out_ptr += strlen(out_ptr);
                }

                snprintf(out_ptr, 3, "\r\n");
                out_ptr += strlen(out_ptr);
            }
        }
    }



    int size = 
    snprintf(
        out_ptr, out_size - (out_ptr - (char*)out_map),
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\r\n\r\n"
        "[[OBJECTS BEGIN]]\r\n"
    );
    out_ptr += size;

    //print out objects
    for (size_t elevation = 0; elevation < 3; elevation++) {
        if (objects[elevation]) {
            snprintf(
                out_ptr, out_size - (out_ptr - (char*)out_map),
                "\r\n%s", objects[elevation]);
        }
        out_ptr += strlen(out_ptr);
    }
    snprintf(
        out_ptr, out_size - (out_ptr - (char*)out_map),
        "[[OBJECTS END]]\r\n"
    );


    //TODO: save out to file
    io_save_txt_file(path, (char*)out_map);
    // printf("+=+=+=+=+=+\n");
    // printf("%s\r\n", out_map);

    free(out_map);
    for (size_t elevation = 0; elevation < 3; elevation++) {
        free(objects[elevation]);
        free(spatials[elevation]);
        for (size_t i = 0; i < 5; i++) {
            free(scripts[elevation].scr_num[i]);
        }
    }
}
