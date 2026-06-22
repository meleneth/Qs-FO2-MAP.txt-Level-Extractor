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


constexpr MapFixtureExpectation map_fixtures[] = {
    {"ARVILL2.map", 141332, "ARVILL2.map", 29105, 0, 0, 1, 0, 12, 1, 5, -1, {true, false, false}},
    {"BROKEN1.map", 416140, "BROKEN1.MAP", 18954, 0, 0, 2, 675, 8, 1, 31, 78, {true, true, false}},
    {"BROKEN2.map", 767096, "BROKEN2.MAP", 21154, 0, 0, 0, 899, 0, 1, 25, 79, {true, true, true}},
    {"Newr1.map",   515376, "NEWR1.MAP",   24305, 0, 0, 0, 353, 0, 1, 1, 54, {true, true, true}},
    {"Newr2.map",   654784, "NEWR2.MAP",   20100, 0, 0, 0, 354, 0, 1, 1, 55, {true, true, true}},
    {"test16.map", 126968, "TEST16.MAP", 20100, 0, 0, 0, 1538, 0, 1, 0, -1, {true, true, true}},
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

TEST_CASE("binary map parser rejects incomplete headers", "[map]")
{
    std::vector<uint8_t> data(8, 0);

    map_lvls map;
    map.file_siz = static_cast<int>(data.size());
    map.data = data.data();

    parse_map_map(&map);

    CHECK(map.header_size == 0);
    CHECK(map.header.version == 0);
    for (int level = 0; level < 3; ++level) {
        CHECK(map.level[level] == nullptr);
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
