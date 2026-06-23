#include <catch2/catch_test_macros.hpp>

#include "map_txt_parser.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
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

struct TextFixtureExpectation {
    const char* txt_file;
    int file_size;
    int square_elev_offsets[3];
    int scripts_offset;
    int objects_offset;
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

TEST_CASE("text map parser locates fixture sections and level ranges", "[txt]")
{
    for (const auto& expected : txt_fixtures) {
        DYNAMIC_SECTION(expected.txt_file) {
            auto data = load_text_fixture(expected.txt_file);
            REQUIRE(static_cast<int>(data.size()) == expected.file_size + 1);

            map_lvls map;
            parse_map_txt(std::span<uint8_t>{data.data(), static_cast<std::size_t>(expected.file_size)}, &map);

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
    map.header_size = 123;
    map.level[0] = reinterpret_cast<char*>(data.data());
    map.lvl_sizes[0] = 456;
    map.scripts = reinterpret_cast<char*>(data.data());
    map.objects = reinterpret_cast<char*>(data.data());

    parse_map_txt(std::span<uint8_t>{data.data(), data.size() - 1}, &map);

    CHECK(map.data == data.data());
    CHECK(map.header_size == 0);
    for (int level = 0; level < 3; ++level) {
        CHECK(map.level[level] == nullptr);
        CHECK(map.lvl_sizes[level] == 0);
    }
    CHECK(map.scripts == nullptr);
    CHECK(map.objects == nullptr);
}
