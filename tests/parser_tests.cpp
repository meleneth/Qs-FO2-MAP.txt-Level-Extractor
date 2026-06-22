#include <catch2/catch_test_macros.hpp>

#include "map_map_parser.h"
#include "map_txt_parser.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::filesystem::path fixture_path(std::string_view name)
{
    return std::filesystem::path(TEST_MAPS_DIR) / std::filesystem::path(name);
}

std::vector<uint8_t> load_binary_fixture(std::string_view name)
{
    std::ifstream file(fixture_path(name), std::ios::binary);
    REQUIRE(file.is_open());

    return std::vector<uint8_t>(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
}

std::vector<uint8_t> load_text_fixture(std::string_view name)
{
    auto data = load_binary_fixture(name);
    data.push_back('\0');
    return data;
}

std::string header_filename(const map_header& header)
{
    const auto end = std::find(std::begin(header.filename), std::end(header.filename), '\0');
    return std::string(std::begin(header.filename), end);
}
int32_t read_be_i32(const std::vector<uint8_t>& data, int offset)
{
    REQUIRE(offset >= 0);
    REQUIRE(offset + static_cast<int>(sizeof(int32_t)) <= static_cast<int>(data.size()));

    return (static_cast<int32_t>(data[offset]) << 24)
        | (static_cast<int32_t>(data[offset + 1]) << 16)
        | (static_cast<int32_t>(data[offset + 2]) << 8)
        | static_cast<int32_t>(data[offset + 3]);
}

struct MapFixtureExpectation {
    const char* map_file;
    int file_size;
    const char* filename;
    int dude_start;
    int elev_start;
    int face_start;
    int lvar_cnt;
    int map_script_id;
    int map_flags;
    int light_level;
    int mvar_cnt;
    int map_id;
    bool levels[3];
};

struct TextFixtureExpectation {
    const char* txt_file;
    int file_size;
    int square_elev_offsets[3];
    int scripts_offset;
    int objects_offset;
};


struct ScriptFixtureExpectation {
    const char* map_file;
    int scripts_offset;
    int script_counts[SCRIPT_TYPE_COUNT];
    int objects_offset;
    int object_count;
    int first_elevation_count;
    int first_object_id;
};

struct ScriptRecordExpectation {
    const char* map_file;
    int type;
    int index;
    int32_t scr_id;
    int32_t scr_index;
    uint32_t scr_obj_id;
    int32_t spatial_tile;
    int32_t spatial_radius;
};

constexpr MapFixtureExpectation map_fixtures[] = {
    {"ARVILL2.map", 141332, "ARVILL2.map", 29105, 0, 0, 1, 0, 12, 1, 5, -1, {true, false, false}},
    {"BROKEN1.map", 416140, "BROKEN1.MAP", 18954, 0, 0, 2, 675, 8, 1, 31, 78, {true, true, false}},
    {"BROKEN2.map", 767096, "BROKEN2.MAP", 21154, 0, 0, 0, 899, 0, 1, 25, 79, {true, true, true}},
    {"Newr1.map",   515376, "NEWR1.MAP",   24305, 0, 0, 0, 353, 0, 1, 1, 54, {true, true, true}},
    {"Newr2.map",   654784, "NEWR2.MAP",   20100, 0, 0, 0, 354, 0, 1, 1, 55, {true, true, true}},
    {"test16.map", 126968, "TEST16.MAP", 20100, 0, 0, 0, 1538, 0, 1, 0, -1, {true, true, true}},
};


constexpr ScriptFixtureExpectation script_fixtures[] = {
    {"ARVILL2.map", 40260, {0, 0, 0, 0, 0}, 40280, 1141, 1141, 586},
    {"BROKEN1.map", 80368, {0, 0, 0, 58, 40}, 87696, 3533, 3303, 26},
    {"BROKEN2.map", 120336, {0, 5, 0, 51, 102}, 132920, 7102, 3011, 34},
    {"Newr1.map",   120240, {0, 3, 0, 75, 113}, 134876, 3910, 2841, 4032},
    {"Newr2.map",   120240, {0, 0, 0, 79, 136}, 134716, 5229, 3068, 588},
    {"test16.map",  120236, {0, 16, 0, 16, 16}, 123480, 32, 16, 122},
};


constexpr ScriptRecordExpectation script_record_fixtures[] = {
    // Values cross-check selected binary records against the mapper .txt dump.
    {"BROKEN2.map", SCRIPT_SPATIAL, 0, 16777216, 876, 3840206052u, 536891365, 5},
    {"BROKEN2.map", SCRIPT_SPATIAL, 4, 16777220, 1165, 29894656u, 1073770953, 2},
    {"BROKEN2.map", SCRIPT_OBJECTS, 0, 50331649, 511, 215u, 0, 0},
    {"BROKEN2.map", SCRIPT_OBJECTS, 50, 50331665, 1175, 46u, 0, 0},
    {"BROKEN2.map", SCRIPT_CRITTER, 0, 67108865, 19, 304u, 0, 0},
    {"BROKEN2.map", SCRIPT_CRITTER, 101, 67108966, 1134, 160u, 0, 0},
    {"test16.map", SCRIPT_SPATIAL, 15, 16777231, 1833, 185273296u, 21116, 20},
    {"test16.map", SCRIPT_OBJECTS, 15, 50331663, 826, 96u, 0, 0},
    {"test16.map", SCRIPT_CRITTER, 0, 67108866, 1856, 136u, 0, 0},
    {"test16.map", SCRIPT_CRITTER, 15, 67108880, 1890, 104u, 0, 0},
};
constexpr TextFixtureExpectation txt_fixtures[] = {
    {"ARVILL2.txt",  734150, {399, -1, -1},       270955, 271060},
    {"BROKEN1.txt", 2073374, {939, 272014, -1},   542038, 556703},
    {"BROKEN2.txt", 3750829, {773, 270933, 540957}, 811125, 835145},
    {"NEWR1.txt",   2603917, {299, 268914, 538676}, 808400, 837091},
    {"NEWR2.txt",   3260051, {299, 268743, 538362}, 808031, 840105},
    {"test16.txt",  836274, {282, 270306, 540330}, 810354, 818686},
};

int level_marker_size(const std::vector<uint8_t>& data, int marker_offset)
{
    constexpr int prefix_size = sizeof("square_elev: 0") - 1;
    REQUIRE(marker_offset >= 0);
    REQUIRE(marker_offset + prefix_size < static_cast<int>(data.size()));

    return data[marker_offset + prefix_size] == '\r'
        ? static_cast<int>(sizeof("square_elev: 0\r\n\r\n") - 1)
        : static_cast<int>(sizeof("square_elev: 0\n\n") - 1);
}

void check_script_record(const scripts_list scripts[SCRIPT_TYPE_COUNT], const ScriptRecordExpectation& expected)
{
    REQUIRE(expected.type >= 0);
    REQUIRE(expected.type < SCRIPT_TYPE_COUNT);
    REQUIRE(expected.index >= 0);
    REQUIRE(expected.index < scripts[expected.type].count);

    const script& actual = scripts[expected.type].scripts[expected.index];
    CHECK(actual.scr_id == expected.scr_id);
    CHECK(actual.scr_index == expected.scr_index);
    CHECK(actual.scr_obj_id == expected.scr_obj_id);
    CHECK(static_cast<int>(((uint32_t)actual.scr_id) >> 24) == expected.type);

    if (expected.type == SCRIPT_SPATIAL) {
        CHECK(actual.spatial_tile == expected.spatial_tile);
        CHECK(actual.spatial_radius == expected.spatial_radius);
    }
}
int pointer_offset(const map_lvls& map, const char* pointer)
{
    REQUIRE(pointer != nullptr);
    return static_cast<int>(pointer - reinterpret_cast<const char*>(map.data));
}

} // namespace

TEST_CASE("binary map parser reads fixture headers and level presence", "[map]")
{
    for (const auto& expected : map_fixtures) {
        DYNAMIC_SECTION(expected.map_file) {
            auto data = load_binary_fixture(expected.map_file);
            REQUIRE(static_cast<int>(data.size()) == expected.file_size);

            map_lvls map;
            map.file_siz = static_cast<int>(data.size());
            map.data = data.data();

            parse_map_map(&map);

            CHECK(map.header_size == static_cast<int>(sizeof(map_header)));
            CHECK(map.header.version == 20);
            CHECK(header_filename(map.header) == expected.filename);
            CHECK(map.header.dude_start == expected.dude_start);
            CHECK(map.header.elev_start == expected.elev_start);
            CHECK(map.header.face_start == expected.face_start);
            CHECK(map.header.lvar_cnt == expected.lvar_cnt);
            CHECK(map.header.map_script_id == expected.map_script_id);
            CHECK(map.header.map_flags == expected.map_flags);
            CHECK(map.header.light_level == expected.light_level);
            CHECK(map.header.mvar_cnt == expected.mvar_cnt);
            CHECK(map.header.map_id == expected.map_id);

            for (int level = 0; level < 3; ++level) {
                CHECK((map.level[level] != nullptr) == expected.levels[level]);
            }
        }
    }
}


TEST_CASE("binary map script parser reads script counts and stops at objects", "[map][scripts]")
{
    for (const auto& expected : script_fixtures) {
        DYNAMIC_SECTION(expected.map_file) {
            auto data = load_binary_fixture(expected.map_file);

            map_lvls map;
            map.file_siz = static_cast<int>(data.size());
            map.data = data.data();
            parse_map_map(&map);

            int offset = expected.scripts_offset;
            scripts_list scripts[SCRIPT_TYPE_COUNT];
            parse_map_scripts(&map, &offset, scripts);

            CHECK(offset == expected.objects_offset);
            CHECK(read_be_i32(data, offset) == expected.object_count);

            int first_object_offset = offset + static_cast<int>(sizeof(int32_t));
            int first_elevation_count = 0;
            while (first_elevation_count == 0) {
                first_elevation_count = read_be_i32(data, first_object_offset);
                first_object_offset += static_cast<int>(sizeof(int32_t));
            }
            CHECK(first_elevation_count == expected.first_elevation_count);
            CHECK(read_be_i32(data, first_object_offset) == expected.first_object_id);

            for (int type = 0; type < SCRIPT_TYPE_COUNT; ++type) {
                CHECK(scripts[type].count == expected.script_counts[type]);
                CHECK((scripts[type].scripts != nullptr) == (expected.script_counts[type] > 0));
                for (int i = 0; i < scripts[type].count; ++i) {
                    CHECK(static_cast<int>(((uint32_t)scripts[type].scripts[i].scr_id) >> 24) == type);
                    CHECK(scripts[type].scripts[i].scr_next == -1);
                    CHECK(scripts[type].scripts[i].lvar_offset == -1);
                }
            }

            for (const auto& record : script_record_fixtures) {
                if (std::string_view(record.map_file) == expected.map_file) {
                    check_script_record(scripts, record);
                }
            }

            for (int type = 0; type < SCRIPT_TYPE_COUNT; ++type) {
                free(scripts[type].scripts);
            }
        }
    }
}

TEST_CASE("text map parser locates fixture sections and level ranges", "[txt]")
{
    for (const auto& expected : txt_fixtures) {
        DYNAMIC_SECTION(expected.txt_file) {
            auto data = load_text_fixture(expected.txt_file);
            REQUIRE(static_cast<int>(data.size()) == expected.file_size + 1);

            map_lvls map;
            map.file_siz = expected.file_size;

            parse_map_txt(data.data(), &map);
            map_level_sizes(&map);

            CHECK(pointer_offset(map, map.scripts) == expected.scripts_offset);
            CHECK(pointer_offset(map, map.objects) == expected.objects_offset);

            int previous_level = -1;
            for (int level = 0; level < 3; ++level) {
                const int marker_offset = expected.square_elev_offsets[level];
                if (marker_offset < 0) {
                    CHECK(map.level[level] == nullptr);
                    CHECK(map.lvl_sizes[level] == 0);
                    continue;
                }

                const int marker_size = level_marker_size(data, marker_offset);
                CHECK(pointer_offset(map, map.level[level]) == marker_offset + marker_size);
                if (previous_level < 0) {
                    CHECK(map.header_size == marker_offset);
                }

                int end_offset = expected.scripts_offset;
                for (int next = level + 1; next < 3; ++next) {
                    if (expected.square_elev_offsets[next] >= 0) {
                        end_offset = expected.square_elev_offsets[next];
                        break;
                    }
                }
                CHECK(map.lvl_sizes[level] == end_offset - (marker_offset + marker_size));
                previous_level = level;
            }
        }
    }
}

TEST_CASE("text map parser clears derived pointers before parse failure", "[txt]")
{
    std::vector<uint8_t> data{'h', 'e', 'a', 'd', 'e', 'r', '\n', '\0'};

    map_lvls map;
    map.file_siz = static_cast<int>(data.size() - 1);
    map.header_size = 123;
    map.level[0] = reinterpret_cast<char*>(data.data());
    map.lvl_sizes[0] = 456;
    map.scripts = reinterpret_cast<char*>(data.data());
    map.objects = reinterpret_cast<char*>(data.data());

    parse_map_txt(data.data(), &map);

    CHECK(map.data == data.data());
    CHECK(map.header_size == 0);
    for (int level = 0; level < 3; ++level) {
        CHECK(map.level[level] == nullptr);
        CHECK(map.lvl_sizes[level] == 0);
    }
    CHECK(map.scripts == nullptr);
    CHECK(map.objects == nullptr);
}

TEST_CASE("text map parser rejects negative legacy file sizes", "[txt]")
{
    std::vector<uint8_t> data{'h', '\0'};

    map_lvls map;
    map.file_siz = -1;
    map.header_size = 123;
    map.level[0] = reinterpret_cast<char*>(data.data());
    map.lvl_sizes[0] = 456;
    map.scripts = reinterpret_cast<char*>(data.data());
    map.objects = reinterpret_cast<char*>(data.data());

    parse_map_txt(data.data(), &map);

    CHECK(map.data == data.data());
    CHECK(map.header_size == 0);
    for (int level = 0; level < 3; ++level) {
        CHECK(map.level[level] == nullptr);
        CHECK(map.lvl_sizes[level] == 0);
    }
    CHECK(map.scripts == nullptr);
    CHECK(map.objects == nullptr);
}
