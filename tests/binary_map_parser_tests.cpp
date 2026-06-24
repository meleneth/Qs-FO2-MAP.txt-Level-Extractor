#include <catch2/catch_test_macros.hpp>

#include "binary_map_parser.h"
#include "prototype_metadata.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string_view>
#include <vector>

namespace {

std::filesystem::path fixture_path(std::string_view name)
{
    return std::filesystem::path(TEST_MAPS_DIR) / std::filesystem::path(name);
}

std::filesystem::path local_proto_root()
{
    return std::filesystem::path(TEST_MAPS_DIR).parent_path()
        / ".local_fallout2_data"
        / "proto";
}

std::vector<std::byte> load_binary_fixture(std::string_view name)
{
    std::ifstream file(fixture_path(name), std::ios::binary);
    REQUIRE(file.is_open());

    std::vector<char> chars(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );
    std::vector<std::byte> bytes;
    bytes.reserve(chars.size());
    for (const auto value : chars) {
        bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(value)));
    }
    return bytes;
}

std::size_t count_records_including_inventory(const std::vector<qmap::BinaryObjectRecord>& records)
{
    std::size_t count = 0;
    for (const auto& record : records) {
        ++count;
        count += count_records_including_inventory(record.inventory);
    }
    return count;
}

struct FixtureObjectSummary {
    std::size_t top_level_records = 0;
    std::size_t records_with_inventory = 0;
    std::size_t inventory_records = 0;
    std::size_t inventory_parent_records = 0;
    std::size_t max_inventory_depth = 0;
    std::size_t item_tails = 0;
    std::size_t weapon_tails = 0;
    std::size_t ammo_tails = 0;
    std::size_t misc_item_tails = 0;
    std::size_t key_tails = 0;
    std::size_t scenery_tails = 0;
    std::size_t door_tails = 0;
    std::size_t stairs_tails = 0;
    std::size_t elevator_tails = 0;
    std::size_t ladder_tails = 0;
    std::size_t misc_exit_grid_tails = 0;
    std::size_t critter_tails = 0;
};

void summarize_object_records(
    const std::vector<qmap::BinaryObjectRecord>& records,
    std::span<const std::byte> bytes,
    const qmap::PrototypeDatabase& prototypes,
    int map_version,
    std::size_t depth,
    FixtureObjectSummary& summary
)
{
    for (const auto& record : records) {
        ++summary.records_with_inventory;
        summary.max_inventory_depth = std::max(summary.max_inventory_depth, depth);
        if (depth > 0) {
            ++summary.inventory_records;
        }
        if (!record.inventory.empty()) {
            ++summary.inventory_parent_records;
        }

        if (const auto prototype = prototypes.find(record.prefix.pid)) {
            if (record.object_type == qmap::BinaryObjectType::item && !record.tail.empty()) {
                auto tail = qmap::parse_binary_item_tail(
                    bytes,
                    record.tail,
                    *prototype
                );
                if (tail) {
                    ++summary.item_tails;
                    switch (prototype->subtype) {
                    case qmap::item_weapon:
                        ++summary.weapon_tails;
                        break;
                    case qmap::item_ammo:
                        ++summary.ammo_tails;
                        break;
                    case qmap::item_misc:
                        ++summary.misc_item_tails;
                        break;
                    case qmap::item_key:
                        ++summary.key_tails;
                        break;
                    default:
                        break;
                    }
                }
            } else if (record.object_type == qmap::BinaryObjectType::scenery && !record.tail.empty()) {
                auto tail = qmap::parse_binary_scenery_subtype_tail(
                    bytes,
                    record.tail,
                    *prototype,
                    map_version
                );
                if (tail) {
                    ++summary.scenery_tails;
                    switch (prototype->subtype) {
                    case qmap::scenery_door:
                        ++summary.door_tails;
                        break;
                    case qmap::scenery_stairs:
                        ++summary.stairs_tails;
                        break;
                    case qmap::scenery_elevator:
                        ++summary.elevator_tails;
                        break;
                    case qmap::scenery_ladder_up:
                    case qmap::scenery_ladder_down:
                        ++summary.ladder_tails;
                        break;
                    default:
                        break;
                    }
                }
            } else if (record.object_type == qmap::BinaryObjectType::misc && record.tail.size == 4 * sizeof(std::int32_t)) {
                auto tail = qmap::parse_binary_misc_tail(bytes, record.tail);
                if (tail) {
                    ++summary.misc_exit_grid_tails;
                }
            }
        }
        if (record.object_type == qmap::BinaryObjectType::critter && !record.tail.empty()) {
            auto tail = qmap::parse_binary_critter_tail(bytes, record.tail);
            if (tail) {
                ++summary.critter_tails;
            }
        }

        summarize_object_records(record.inventory, bytes, prototypes, map_version, depth + 1, summary);
    }
}

FixtureObjectSummary summarize_fixture_objects(
    std::span<const std::byte> bytes,
    const qmap::BinaryMapHeader& header,
    const qmap::BinaryMapObjectRecords& objects,
    const qmap::PrototypeDatabase& prototypes
)
{
    FixtureObjectSummary summary;
    summary.top_level_records = objects.records.size();
    summarize_object_records(objects.records, bytes, prototypes, static_cast<int>(header.version), 0, summary);
    return summary;
}

std::byte b(unsigned int value)
{
    return static_cast<std::byte>(value);
}

void append_u32(std::vector<std::byte>& bytes, std::uint32_t value)
{
    bytes.push_back(b((value >> 24) & 0xFFu));
    bytes.push_back(b((value >> 16) & 0xFFu));
    bytes.push_back(b((value >> 8) & 0xFFu));
    bytes.push_back(b(value & 0xFFu));
}

void append_i32(std::vector<std::byte>& bytes, std::int32_t value)
{
    append_u32(bytes, static_cast<std::uint32_t>(value));
}

void append_filename(std::vector<std::byte>& bytes, std::string_view filename)
{
    for (std::size_t index = 0; index < qmap::binary_map_filename_size; ++index) {
        const auto ch = index < filename.size() ? static_cast<unsigned char>(filename[index]) : 0;
        bytes.push_back(b(ch));
    }
}

std::vector<std::byte> example_header()
{
    std::vector<std::byte> bytes;
    bytes.reserve(qmap::binary_map_header_size);
    append_u32(bytes, 20);
    append_filename(bytes, "TEST.MAP");
    append_i32(bytes, 20100);
    append_i32(bytes, 1);
    append_i32(bytes, 3);
    append_i32(bytes, 4);
    append_i32(bytes, 1538);
    append_i32(bytes, 0x4);
    append_i32(bytes, 12);
    append_i32(bytes, 7);
    append_i32(bytes, 99);
    append_u32(bytes, 123456u);
    for (int index = 0; index < static_cast<int>(qmap::binary_map_unknown_header_words); ++index) {
        append_i32(bytes, index);
    }
    return bytes;
}

std::vector<std::byte> example_map_with_variables()
{
    auto bytes = example_header();
    append_i32(bytes, 10);
    append_i32(bytes, -20);
    append_i32(bytes, 21);
    append_i32(bytes, 22);
    append_i32(bytes, 23);
    append_i32(bytes, 24);
    append_i32(bytes, 70);
    append_i32(bytes, 30);
    append_i32(bytes, 40);
    append_i32(bytes, -50);
    append_i32(bytes, 60);
    return bytes;
}

std::vector<std::byte> example_map_with_tiles()
{
    auto bytes = example_map_with_variables();
    bytes.insert(bytes.end(), 10000 * 4, b(0xAA));
    bytes.insert(bytes.end(), 10000 * 4, b(0xBB));
    return bytes;
}

void append_script_record(
    std::vector<std::byte>& bytes,
    qmap::BinaryScriptType type,
    std::int32_t scr_id,
    std::int32_t scr_index
)
{
    append_i32(bytes, scr_id);
    append_i32(bytes, -1);
    if (type == qmap::BinaryScriptType::spatial) {
        append_i32(bytes, 536870912);
        append_i32(bytes, 5);
    }
    if (type == qmap::BinaryScriptType::timed) {
        append_i32(bytes, 1234);
    }
    append_i32(bytes, 0);
    append_i32(bytes, scr_index);
    append_i32(bytes, 0);
    append_u32(bytes, 215);
    append_i32(bytes, -1);
    append_i32(bytes, 0);
    append_i32(bytes, 0);
    append_i32(bytes, 0);
    append_i32(bytes, 0);
    append_i32(bytes, 0);
    append_i32(bytes, 0);
    append_i32(bytes, 0);
    append_i32(bytes, 0);
    append_i32(bytes, 0);
}

int script_record_word_count(qmap::BinaryScriptType type)
{
    int words = 16;
    if (type == qmap::BinaryScriptType::spatial) {
        words += 2;
    }
    if (type == qmap::BinaryScriptType::timed) {
        words += 1;
    }
    return words;
}

void append_script_padding(std::vector<std::byte>& bytes, qmap::BinaryScriptType type, int parsed_count)
{
    const auto words = script_record_word_count(type);
    const auto script_id = static_cast<std::int32_t>(static_cast<int>(type) << 24);
    for (int slot = parsed_count; slot < 16; ++slot) {
        append_i32(bytes, script_id);
        append_i32(bytes, -1);
        for (int index = 2; index < words; ++index) {
            append_i32(bytes, 0);
        }
    }
}

void append_script_padding_record_as(
    std::vector<std::byte>& bytes,
    qmap::BinaryScriptType stored_type
)
{
    const auto words = script_record_word_count(stored_type);
    append_i32(bytes, static_cast<std::int32_t>(static_cast<int>(stored_type) << 24));
    append_i32(bytes, -1);
    for (int index = 2; index < words; ++index) {
        append_i32(bytes, 0);
    }
}

void append_unknown_script_padding_record(std::vector<std::byte>& bytes)
{
    append_i32(bytes, static_cast<std::int32_t>(0xCCCCCCCCu));
    append_i32(bytes, -1);
    for (int index = 2; index < 16; ++index) {
        append_i32(bytes, 0);
    }
}

void append_script_footer(std::vector<std::byte>& bytes, int count, int next)
{
    append_i32(bytes, count);
    append_i32(bytes, next);
}

std::vector<std::byte> example_map_with_scripts()
{
    auto bytes = example_map_with_tiles();

    append_i32(bytes, 0);

    append_i32(bytes, 1);
    append_script_record(bytes, qmap::BinaryScriptType::spatial, 0x01000000, 876);
    append_script_padding(bytes, qmap::BinaryScriptType::spatial, 1);
    append_script_footer(bytes, 1, 0);

    append_i32(bytes, 0);

    append_i32(bytes, 17);
    for (int index = 0; index < 16; ++index) {
        append_script_record(bytes, qmap::BinaryScriptType::object, 0x03000001 + index, 500 + index);
    }
    append_script_footer(bytes, 16, 1234);
    append_script_record(bytes, qmap::BinaryScriptType::object, 0x03000020, 900);
    append_script_padding(bytes, qmap::BinaryScriptType::object, 1);
    append_script_footer(bytes, 1, 0);

    append_i32(bytes, 0);
    return bytes;
}

void append_object_prefix(
    std::vector<std::byte>& bytes,
    std::int32_t obj_id,
    std::int32_t elevation,
    std::int32_t pid,
    std::int32_t script_id,
    std::int32_t inventory_count = 0,
    std::int32_t inventory_size = 0,
    std::int32_t tile = 12345
)
{
    append_i32(bytes, obj_id);
    append_i32(bytes, tile);
    append_i32(bytes, 1);
    append_i32(bytes, 2);
    append_i32(bytes, 3);
    append_i32(bytes, 4);
    append_i32(bytes, 5);
    append_i32(bytes, 1);
    append_i32(bytes, 0x02000001);
    append_i32(bytes, 0);
    append_i32(bytes, elevation);
    append_i32(bytes, pid);
    append_i32(bytes, -1);
    append_i32(bytes, 6);
    append_i32(bytes, 7);
    append_i32(bytes, 0);
    append_i32(bytes, script_id);
    append_i32(bytes, 800);
    append_i32(bytes, inventory_count);
    append_i32(bytes, inventory_size);
    append_i32(bytes, 901);
    append_i32(bytes, 902);
}

void append_i32_repeated(std::vector<std::byte>& bytes, int count, std::int32_t start_value)
{
    for (int index = 0; index < count; ++index) {
        append_i32(bytes, start_value + index);
    }
}

std::vector<std::byte> example_map_with_object_prefixes()
{
    auto bytes = example_map_with_scripts();
    append_i32(bytes, 2);
    append_i32(bytes, 1);
    append_i32(bytes, 0);
    append_i32(bytes, 1);
    append_object_prefix(bytes, 100, 0, 0x02000001, 50331649);
    append_object_prefix(bytes, 200, 2, 0x01000002, 67108865);
    return bytes;
}

std::vector<std::byte> example_map_with_object_records()
{
    auto bytes = example_map_with_scripts();
    append_i32(bytes, 2);
    append_i32(bytes, 1);
    append_object_prefix(bytes, 100, 0, 0x05000010, 50331649);
    append_i32_repeated(bytes, 4, 9000);
    append_i32(bytes, 0);
    append_i32(bytes, 1);
    append_object_prefix(bytes, 200, 2, 0x01000002, 67108865);
    append_i32_repeated(bytes, 10, 9100);
    return bytes;
}

std::vector<std::byte> example_map_with_inventory_object()
{
    auto bytes = example_map_with_scripts();
    append_i32(bytes, 1);
    append_i32(bytes, 1);
    append_object_prefix(bytes, 100, 0, 0x03000001, 50331649, 1, 4);
    append_i32(bytes, 3);
    append_object_prefix(bytes, 101, -1, 0x00000002, -1, 0, 0, -1);
    append_i32(bytes, 0);
    append_i32(bytes, 0);
    return bytes;
}

} // namespace

TEST_CASE("parse_binary_map_header reads typed header fields", "[map][binary]")
{
    const auto bytes = example_header();

    const auto parsed = qmap::parse_binary_map_header(bytes);

    REQUIRE(parsed);
    CHECK(parsed.value().version == 20);
    CHECK(parsed.value().filename_string() == "TEST.MAP");
    CHECK(parsed.value().dude_start == 20100);
    CHECK(parsed.value().elev_start == 1);
    CHECK(parsed.value().face_start == 3);
    CHECK(parsed.value().lvar_count == 4);
    CHECK(parsed.value().map_script_id == 1538);
    CHECK(parsed.value().map_flags == 0x4);
    CHECK(parsed.value().light_level == 12);
    CHECK(parsed.value().mvar_count == 7);
    CHECK(parsed.value().map_id == 99);
    CHECK(parsed.value().game_ticks == 123456u);
    CHECK(parsed.value().unknown[0] == 0);
    CHECK(parsed.value().unknown[43] == 43);
    CHECK(parsed.value().has_elevation(0));
    CHECK_FALSE(parsed.value().has_elevation(1));
    CHECK(parsed.value().has_elevation(2));
    CHECK_FALSE(parsed.value().has_elevation(-1));
    CHECK_FALSE(parsed.value().has_elevation(3));
}

TEST_CASE("parse_binary_map_header rejects short input", "[map][binary]")
{
    const auto bytes = std::array{b(0), b(0), b(0), b(20), b('T')};

    const auto parsed = qmap::parse_binary_map_header(bytes);

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "unexpected end of input");
    CHECK(parsed.error().offset == 4);
}

TEST_CASE("parse_binary_map_variables reads map and local variable blocks", "[map][binary]")
{
    const auto bytes = example_map_with_variables();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);

    const auto parsed = qmap::parse_binary_map_variables(bytes, header.value());

    REQUIRE(parsed);
    REQUIRE(parsed.value().map_vars.size() == 7);
    CHECK(parsed.value().map_vars[0] == 10);
    CHECK(parsed.value().map_vars[1] == -20);
    CHECK(parsed.value().map_vars[6] == 70);
    REQUIRE(parsed.value().local_vars.size() == 4);
    CHECK(parsed.value().local_vars[0] == 30);
    CHECK(parsed.value().local_vars[1] == 40);
    CHECK(parsed.value().local_vars[2] == -50);
    CHECK(parsed.value().local_vars[3] == 60);
}

TEST_CASE("parse_binary_map_variables rejects truncated variable blocks", "[map][binary]")
{
    auto bytes = example_map_with_variables();
    bytes.pop_back();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);

    const auto parsed = qmap::parse_binary_map_variables(bytes, header.value());

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "unexpected end of input");
    CHECK(parsed.error().offset == qmap::binary_map_header_size);
}

TEST_CASE("parse_binary_map_variables rejects negative counts", "[map][binary]")
{
    const auto bytes = example_header();
    qmap::BinaryMapHeader header;
    header.mvar_count = -1;

    const auto parsed = qmap::parse_binary_map_variables(bytes, header);

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "negative map variable count");
}

TEST_CASE("parse_binary_map_tiles exposes present elevation byte ranges", "[map][binary]")
{
    const auto bytes = example_map_with_tiles();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);

    const auto parsed = qmap::parse_binary_map_tiles(bytes, header.value());

    REQUIRE(parsed);
    REQUIRE(parsed.value().elevations[0].size() == 40000);
    CHECK(parsed.value().elevations[0].front() == b(0xAA));
    CHECK(parsed.value().elevations[1].empty());
    REQUIRE(parsed.value().elevations[2].size() == 40000);
    CHECK(parsed.value().elevations[2].front() == b(0xBB));
}

TEST_CASE("parse_binary_map_tiles rejects truncated tile blocks", "[map][binary]")
{
    auto bytes = example_map_with_tiles();
    bytes.resize(bytes.size() - 1);
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);

    const auto parsed = qmap::parse_binary_map_tiles(bytes, header.value());

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "unexpected end of input");
}

TEST_CASE("parse_binary_map_scripts reads script records and block footers", "[map][binary]")
{
    const auto bytes = example_map_with_scripts();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);

    const auto parsed = qmap::parse_binary_map_scripts(bytes, header.value());

    REQUIRE(parsed);
    REQUIRE(parsed.value().by_type[0].empty());

    const auto& spatial = parsed.value().by_type[1];
    REQUIRE(spatial.size() == 1);
    CHECK(parsed.value().count_offsets[1] + sizeof(std::int32_t) == spatial[0].raw.offset);
    CHECK(spatial[0].offsets.scr_id == spatial[0].raw.offset);
    REQUIRE(spatial[0].offsets.spatial_tile);
    CHECK(*spatial[0].offsets.spatial_tile == spatial[0].raw.offset + 2 * sizeof(std::int32_t));
    CHECK(spatial[0].offsets.scr_obj_id == spatial[0].raw.offset + 7 * sizeof(std::int32_t));
    CHECK(spatial[0].offsets.lvar_offset == spatial[0].raw.offset + 8 * sizeof(std::int32_t));
    CHECK(spatial[0].offsets.lvar_count == spatial[0].raw.offset + 9 * sizeof(std::int32_t));
    CHECK(spatial[0].type == qmap::BinaryScriptType::spatial);
    CHECK(spatial[0].scr_id == 0x01000000);
    CHECK(spatial[0].scr_next == -1);
    CHECK(spatial[0].spatial_tile == 536870912);
    CHECK(spatial[0].spatial_radius == 5);
    CHECK(spatial[0].scr_index == 876);
    CHECK(spatial[0].scr_obj_id == 215u);
    CHECK(spatial[0].raw.size == 18 * sizeof(std::int32_t));

    const auto& objects = parsed.value().by_type[3];
    REQUIRE(objects.size() == 17);
    CHECK(parsed.value().count_offsets[3] + sizeof(std::int32_t) == objects[0].raw.offset);
    CHECK(objects[0].offsets.scr_id == objects[0].raw.offset);
    CHECK_FALSE(objects[0].offsets.spatial_tile);
    CHECK(objects[0].offsets.scr_obj_id == objects[0].raw.offset + 5 * sizeof(std::int32_t));
    CHECK(objects[0].offsets.lvar_offset == objects[0].raw.offset + 6 * sizeof(std::int32_t));
    CHECK(objects[0].offsets.lvar_count == objects[0].raw.offset + 7 * sizeof(std::int32_t));
    CHECK(objects[0].scr_id == 0x03000001);
    CHECK(objects[15].scr_id == 0x03000010);
    CHECK(objects[16].scr_id == 0x03000020);
    CHECK(objects[16].scr_index == 900);
    CHECK(objects[16].raw.size == 16 * sizeof(std::int32_t));
    CHECK(parsed.value().end_offset == bytes.size());
}

TEST_CASE("parse_binary_map_scripts rejects footer count mismatches", "[map][binary]")
{
    auto bytes = example_map_with_tiles();
    append_i32(bytes, 0);
    append_i32(bytes, 1);
    append_script_record(bytes, qmap::BinaryScriptType::spatial, 0x01000000, 876);
    append_script_padding(bytes, qmap::BinaryScriptType::spatial, 1);
    append_script_footer(bytes, 2, 0);

    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);

    const auto parsed = qmap::parse_binary_map_scripts(bytes, header.value());

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "script block footer count mismatch");
}

TEST_CASE("parse_binary_map_scripts tolerates padding records with different valid layouts", "[map][binary]")
{
    auto bytes = example_map_with_tiles();
    append_i32(bytes, 0);
    append_i32(bytes, 1);
    append_script_record(bytes, qmap::BinaryScriptType::spatial, 0x01000000, 876);
    for (int slot = 1; slot < 16; ++slot) {
        append_script_padding_record_as(bytes, qmap::BinaryScriptType::object);
    }
    append_script_footer(bytes, 1, 0);
    append_i32(bytes, 0);
    append_i32(bytes, 0);
    append_i32(bytes, 0);

    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);

    const auto parsed = qmap::parse_binary_map_scripts(bytes, header.value());

    REQUIRE(parsed);
    REQUIRE(parsed.value().by_type[1].size() == 1);
    CHECK(parsed.value().by_type[1][0].scr_id == 0x01000000);
    CHECK(parsed.value().end_offset == bytes.size());
}

TEST_CASE("parse_binary_map_scripts treats unknown padding layouts as system-sized", "[map][binary]")
{
    auto bytes = example_map_with_tiles();
    append_i32(bytes, 0);
    append_i32(bytes, 1);
    append_script_record(bytes, qmap::BinaryScriptType::spatial, 0x01000000, 876);
    for (int slot = 1; slot < 16; ++slot) {
        append_unknown_script_padding_record(bytes);
    }
    append_script_footer(bytes, 1, 0);
    append_i32(bytes, 0);
    append_i32(bytes, 0);
    append_i32(bytes, 0);

    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);

    const auto parsed = qmap::parse_binary_map_scripts(bytes, header.value());

    REQUIRE(parsed);
    REQUIRE(parsed.value().by_type[1].size() == 1);
    CHECK(parsed.value().end_offset == bytes.size());
}

TEST_CASE("parse_binary_map_scripts rejects truncated records", "[map][binary]")
{
    auto bytes = example_map_with_scripts();
    bytes.resize(bytes.size() - 3);
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);

    const auto parsed = qmap::parse_binary_map_scripts(bytes, header.value());

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "unexpected end of input");
}

TEST_CASE("parse_binary_map_object_prefixes reads counts and fixed object fields", "[map][binary]")
{
    const auto bytes = example_map_with_object_prefixes();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);

    const auto parsed = qmap::parse_binary_map_object_prefixes(bytes, scripts.value().end_offset);

    REQUIRE(parsed);
    CHECK(parsed.value().total_count == 2);
    CHECK(parsed.value().elevation_counts[0] == 1);
    CHECK(parsed.value().elevation_counts[1] == 0);
    CHECK(parsed.value().elevation_counts[2] == 1);
    REQUIRE(parsed.value().records.size() == 2);
    CHECK(parsed.value().records[0].obj_id == 100);
    CHECK(parsed.value().records[0].offsets.obj_id == parsed.value().records[0].raw.offset);
    CHECK(parsed.value().records[0].offsets.tile == parsed.value().records[0].raw.offset + sizeof(std::int32_t));
    CHECK(parsed.value().records[0].offsets.elevation == parsed.value().records[0].raw.offset + 10 * sizeof(std::int32_t));
    CHECK(parsed.value().records[0].offsets.pid == parsed.value().records[0].raw.offset + 11 * sizeof(std::int32_t));
    CHECK(parsed.value().records[0].offsets.script_id == parsed.value().records[0].raw.offset + 16 * sizeof(std::int32_t));
    CHECK(parsed.value().records[0].offsets.inventory_count == parsed.value().records[0].raw.offset + 18 * sizeof(std::int32_t));
    CHECK(parsed.value().records[0].offsets.inventory_size == parsed.value().records[0].raw.offset + 19 * sizeof(std::int32_t));
    CHECK(parsed.value().records[0].elevation == 0);
    CHECK(parsed.value().records[0].pid == 0x02000001);
    CHECK(parsed.value().records[0].pid_type() == 2);
    CHECK(qmap::binary_object_type_from_pid(parsed.value().records[0].pid) == qmap::BinaryObjectType::scenery);
    CHECK(parsed.value().records[0].script_id == 50331649);
    CHECK(parsed.value().records[0].unknown_10 == 901);
    CHECK(parsed.value().records[0].unknown_11 == 902);
    CHECK(parsed.value().records[0].raw.size == 22 * sizeof(std::int32_t));
    CHECK(parsed.value().records[1].obj_id == 200);
    CHECK(parsed.value().records[1].elevation == 2);
    CHECK(parsed.value().records[1].pid == 0x01000002);
    CHECK(parsed.value().records[1].pid_type() == 1);
    CHECK(qmap::binary_object_type_from_pid(parsed.value().records[1].pid) == qmap::BinaryObjectType::critter);
    CHECK(parsed.value().records[1].script_id == 67108865);
    CHECK(parsed.value().end_offset == bytes.size());
}

TEST_CASE("parse_binary_map_object_counts follows present elevation counts", "[map][binary]")
{
    auto bytes = example_map_with_scripts();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);
    append_i32(bytes, 5);
    append_i32(bytes, 2);
    append_object_prefix(bytes, 100, 0, 0x02000001, 50331649);

    const auto parsed = qmap::parse_binary_map_object_counts(bytes, scripts.value().end_offset, header.value());

    REQUIRE(parsed);
    CHECK(parsed.value().total_count == 5);
    CHECK(parsed.value().first_counted_elevation == 0);
    CHECK(parsed.value().elevation_counts[0] == 2);
    CHECK(parsed.value().elevation_counts[1] == 0);
    CHECK(parsed.value().elevation_counts[2] == 0);
    CHECK(parsed.value().data_offset == scripts.value().end_offset + 2 * sizeof(std::int32_t));
}

TEST_CASE("parse_binary_map_object_counts rejects excessive first elevation count", "[map][binary]")
{
    auto bytes = example_map_with_scripts();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);
    append_i32(bytes, 1);
    append_i32(bytes, 2);

    const auto parsed = qmap::parse_binary_map_object_counts(bytes, scripts.value().end_offset, header.value());

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "object count mismatch");
}

TEST_CASE("parse_first_binary_object_block_header finds the first present elevation block", "[map][binary]")
{
    auto bytes = example_map_with_scripts();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);
    append_i32(bytes, 5);
    append_i32(bytes, 0);
    append_i32(bytes, 3);
    append_object_prefix(bytes, 200, 2, 0x01000002, 67108865);

    qmap::BinaryMapHeader only_upper = header.value();
    only_upper.map_flags = 0x2;
    const auto parsed = qmap::parse_first_binary_object_block_header(
        bytes,
        scripts.value().end_offset,
        only_upper
    );

    REQUIRE(parsed);
    CHECK(parsed.value().total_count == 5);
    CHECK(parsed.value().elevation == 1);
    CHECK(parsed.value().block_count == 0);
    CHECK(parsed.value().objects_offset == scripts.value().end_offset + 2 * sizeof(std::int32_t));
}

TEST_CASE("parse_first_binary_object_block_header rejects excessive first block count", "[map][binary]")
{
    auto bytes = example_map_with_scripts();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);
    append_i32(bytes, 1);
    append_i32(bytes, 2);

    const auto parsed = qmap::parse_first_binary_object_block_header(
        bytes,
        scripts.value().end_offset,
        header.value()
    );

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "object count mismatch");
}

TEST_CASE("parse_first_binary_object_prefix reads the first prefix in the first object block", "[map][binary]")
{
    auto bytes = example_map_with_scripts();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);
    append_i32(bytes, 2);
    append_i32(bytes, 1);
    append_object_prefix(bytes, 100, 0, 0x02000001, 50331649);
    append_i32(bytes, 1);
    append_object_prefix(bytes, 200, 2, 0x01000002, 67108865);

    const auto parsed = qmap::parse_first_binary_object_prefix(bytes, scripts.value().end_offset, header.value());

    REQUIRE(parsed);
    REQUIRE(parsed.value().has_value());
    CHECK(parsed.value()->obj_id == 100);
    CHECK(parsed.value()->pid == 0x02000001);
    CHECK(parsed.value()->elevation == 0);
    CHECK(parsed.value()->raw.size == 22 * sizeof(std::int32_t));
}

TEST_CASE("parse_first_binary_object_prefix skips empty object blocks", "[map][binary]")
{
    auto bytes = example_map_with_scripts();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);
    append_i32(bytes, 1);
    append_i32(bytes, 0);
    append_i32(bytes, 1);
    append_object_prefix(bytes, 200, 2, 0x01000002, 67108865);

    const auto parsed = qmap::parse_first_binary_object_prefix(bytes, scripts.value().end_offset, header.value());

    REQUIRE(parsed);
    REQUIRE(parsed.value().has_value());
    CHECK(parsed.value()->obj_id == 200);
    CHECK(parsed.value()->pid == 0x01000002);
    CHECK(parsed.value()->elevation == 2);
}

TEST_CASE("parse_first_binary_object_prefix rejects excessive first block count", "[map][binary]")
{
    auto bytes = example_map_with_scripts();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);
    append_i32(bytes, 1);
    append_i32(bytes, 2);
    append_object_prefix(bytes, 100, 0, 0x02000001, 50331649);

    const auto parsed = qmap::parse_first_binary_object_prefix(bytes, scripts.value().end_offset, header.value());

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "object count mismatch");
}

TEST_CASE("parse_first_binary_object_prefix returns empty when all present blocks are empty", "[map][binary]")
{
    auto bytes = example_map_with_scripts();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);
    append_i32(bytes, 0);
    append_i32(bytes, 0);
    append_i32(bytes, 0);

    const auto parsed = qmap::parse_first_binary_object_prefix(bytes, scripts.value().end_offset, header.value());

    REQUIRE(parsed);
    CHECK_FALSE(parsed.value().has_value());
}

TEST_CASE("binary_object_type_from_pid rejects unknown object type bytes", "[map][binary]")
{
    CHECK_FALSE(qmap::binary_object_type_from_pid(0x0A000001).has_value());
}

TEST_CASE("modeled_binary_object_tail_size reports current fixed object tails", "[map][binary]")
{
    CHECK(qmap::modeled_binary_object_tail_size(qmap::BinaryObjectType::item) == 0);
    CHECK(qmap::modeled_binary_object_tail_size(qmap::BinaryObjectType::critter) == 40);
    CHECK(qmap::modeled_binary_object_tail_size(qmap::BinaryObjectType::scenery) == 0);
    CHECK(qmap::modeled_binary_object_tail_size(qmap::BinaryObjectType::wall) == 0);
    CHECK(qmap::modeled_binary_object_tail_size(qmap::BinaryObjectType::tile) == 0);
    CHECK(qmap::modeled_binary_object_tail_size(qmap::BinaryObjectType::misc) == 0);
    CHECK(qmap::modeled_binary_object_tail_size(qmap::BinaryObjectType::interface_object) == 0);
    CHECK(qmap::modeled_binary_object_tail_size(qmap::BinaryObjectType::inventory) == 0);
    CHECK(qmap::modeled_binary_object_tail_size(qmap::BinaryObjectType::head) == 0);
    CHECK(qmap::modeled_binary_object_tail_size(qmap::BinaryObjectType::background) == 0);
}

TEST_CASE("parse_binary_map_object_prefixes rejects count mismatches", "[map][binary]")
{
    auto bytes = example_map_with_scripts();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);
    append_i32(bytes, 2);
    append_i32(bytes, 1);
    append_i32(bytes, 0);
    append_i32(bytes, 0);

    const auto parsed = qmap::parse_binary_map_object_prefixes(bytes, scripts.value().end_offset);

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "object count mismatch");
}

TEST_CASE("parse_binary_map_object_prefixes rejects count sums without overflowing", "[map][binary]")
{
    std::vector<std::byte> bytes;
    append_i32(bytes, 1);
    append_i32(bytes, 2147483647);
    append_i32(bytes, 2147483647);
    append_i32(bytes, 0);

    const auto parsed = qmap::parse_binary_map_object_prefixes(bytes, 0);

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "object count mismatch");
}

TEST_CASE("parse_binary_map_object_prefixes rejects excessive first count before reading more counts", "[map][binary]")
{
    std::vector<std::byte> bytes;
    append_i32(bytes, 1);
    append_i32(bytes, 2);

    const auto parsed = qmap::parse_binary_map_object_prefixes(bytes, 0);

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "object count mismatch");
}

TEST_CASE("parse_binary_map_object_prefixes rejects truncated prefixes", "[map][binary]")
{
    auto bytes = example_map_with_object_prefixes();
    bytes.pop_back();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);

    const auto parsed = qmap::parse_binary_map_object_prefixes(bytes, scripts.value().end_offset);

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "unexpected end of input");
}

TEST_CASE("parse_binary_map_object_records rejects excessive elevation counts before parsing objects", "[map][binary]")
{
    std::vector<std::byte> bytes;
    append_i32(bytes, 1);
    append_i32(bytes, 2);

    qmap::BinaryMapHeader header;
    header.map_flags = 0x6;

    const auto parsed = qmap::parse_binary_map_object_records(bytes, 0, header);

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "absent elevation has object records");
}

TEST_CASE("parse_binary_map_object_records rejects excessive first count before reading more counts", "[map][binary]")
{
    std::vector<std::byte> bytes;
    append_i32(bytes, 1);
    append_i32(bytes, 2);

    const auto parsed = qmap::parse_binary_map_object_records(bytes, 0);

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "object count mismatch");
}

TEST_CASE("parse_binary_map_object_records preserves known type-specific tails", "[map][binary]")
{
    const auto bytes = example_map_with_object_records();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);

    const auto parsed = qmap::parse_binary_map_object_records(bytes, scripts.value().end_offset, header.value());

    REQUIRE(parsed);
    REQUIRE(parsed.value().records.size() == 2);
    CHECK(parsed.value().records[0].prefix.pid == 0x05000010);
    CHECK(parsed.value().records[0].object_type == qmap::BinaryObjectType::misc);
    CHECK(parsed.value().records[0].tail.size == 4 * sizeof(std::int32_t));
    CHECK(parsed.value().records[0].raw.size == 26 * sizeof(std::int32_t));
    CHECK(parsed.value().records[1].prefix.pid == 0x01000002);
    CHECK(parsed.value().records[1].object_type == qmap::BinaryObjectType::critter);
    CHECK(parsed.value().records[1].tail.size == 10 * sizeof(std::int32_t));
    CHECK(parsed.value().records[1].raw.size == 32 * sizeof(std::int32_t));
    CHECK(parsed.value().end_offset == bytes.size());
}

TEST_CASE("parse_binary_map_object_records keeps generic misc PID 0x0500000C prefix-only", "[map][binary]")
{
    auto bytes = example_map_with_scripts();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);
    append_i32(bytes, 2);
    append_i32(bytes, 2);
    append_object_prefix(bytes, 204, 0, 0x0500000C, -1);
    append_object_prefix(bytes, 318, 0, 0x03000001, -1);
    append_i32(bytes, 0);
    append_i32(bytes, 0);

    const auto parsed = qmap::parse_binary_map_object_records(bytes, scripts.value().end_offset, header.value());

    REQUIRE(parsed);
    REQUIRE(parsed.value().records.size() == 2);
    CHECK(parsed.value().records[0].prefix.pid == 0x0500000C);
    CHECK(parsed.value().records[0].tail.empty());
    CHECK(parsed.value().records[0].raw.size == 22 * sizeof(std::int32_t));
    CHECK(parsed.value().records[1].prefix.obj_id == 318);
    CHECK(parsed.value().records[1].prefix.raw.offset == parsed.value().records[0].raw.end());
}

TEST_CASE("parse_binary_map_object_records uses prototype tails for scenery", "[map][binary]")
{
    auto bytes = example_map_with_scripts();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);
    append_i32(bytes, 2);
    append_i32(bytes, 2);
    append_object_prefix(bytes, 100, 0, 0x02000001, -1);
    append_i32(bytes, 1234);
    append_object_prefix(bytes, 200, 0, 0x03000001, -1);
    append_i32(bytes, 0);
    append_i32(bytes, 0);

    qmap::PrototypeDatabase prototypes;
    prototypes.add(qmap::PrototypeRecord{
        0x02000001,
        qmap::BinaryObjectType::scenery,
        0,
    });

    const auto parsed = qmap::parse_binary_map_object_records(
        bytes,
        scripts.value().end_offset,
        header.value(),
        prototypes
    );

    REQUIRE(parsed);
    REQUIRE(parsed.value().records.size() == 2);
    CHECK(parsed.value().records[0].prefix.pid == 0x02000001);
    CHECK(parsed.value().records[0].tail.size == sizeof(std::int32_t));
    CHECK(parsed.value().records[1].prefix.obj_id == 200);
    CHECK(parsed.value().records[1].prefix.raw.offset == parsed.value().records[0].raw.end());
}

TEST_CASE("parse_binary_map_object_records accepts prototype metadata without unvalidated cursor movement", "[map][binary]")
{
    auto bytes = example_map_with_object_records();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);

    qmap::PrototypeDatabase prototypes;
    prototypes.add(qmap::PrototypeRecord{
        0x00000001,
        qmap::BinaryObjectType::item,
        3,
    });
    prototypes.add(qmap::PrototypeRecord{
        0x00000002,
        qmap::BinaryObjectType::item,
        0,
    });

    const auto parsed = qmap::parse_binary_map_object_records(
        bytes,
        scripts.value().end_offset,
        header.value(),
        prototypes
    );

    if (!parsed) {
        INFO(parsed.error().message);
    }
    REQUIRE(parsed);
    REQUIRE(parsed.value().records.size() == 2);
    CHECK(parsed.value().records[0].prefix.pid == 0x05000010);
    CHECK(parsed.value().records[0].tail.size == 4 * sizeof(std::int32_t));
    CHECK(parsed.value().records[1].prefix.pid == 0x01000002);
    CHECK(parsed.value().records[1].tail.size == 10 * sizeof(std::int32_t));
}

TEST_CASE("parse_binary_map_object_records requires prototype metadata for item tails", "[map][binary]")
{
    auto bytes = example_map_with_scripts();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);
    append_i32(bytes, 1);
    append_i32(bytes, 1);
    append_object_prefix(bytes, 100, 0, 0x00000001, -1);
    append_i32(bytes, 0);
    append_i32(bytes, 0);

    const auto parsed = qmap::parse_binary_map_object_records(bytes, scripts.value().end_offset, header.value());

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "elevation 0 object 0: prototype metadata required for item PID 1");
}

TEST_CASE("parse_binary_map_object_records uses prototype tails for top-level items", "[map][binary]")
{
    auto bytes = example_map_with_scripts();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);
    append_i32(bytes, 1);
    append_i32(bytes, 1);
    append_object_prefix(bytes, 100, 0, 0x00000001, -1);
    append_i32(bytes, 50);
    append_i32(bytes, 51);
    const auto object_end = bytes.size();
    append_i32(bytes, 0);
    append_i32(bytes, 0);

    qmap::PrototypeDatabase prototypes;
    prototypes.add(qmap::PrototypeRecord{
        0x00000001,
        qmap::BinaryObjectType::item,
        3,
    });
    prototypes.add(qmap::PrototypeRecord{
        0x00000002,
        qmap::BinaryObjectType::item,
        0,
    });

    const auto parsed = qmap::parse_binary_map_object_records(
        bytes,
        scripts.value().end_offset,
        header.value(),
        prototypes
    );

    REQUIRE(parsed);
    REQUIRE(parsed.value().records.size() == 1);
    CHECK(parsed.value().records[0].tail.size == 2 * sizeof(std::int32_t));
    CHECK(parsed.value().records[0].raw.end() == object_end);
    CHECK(parsed.value().diagnostics.empty());
}

TEST_CASE("parse_binary_map_object_records follows present elevation blocks", "[map][binary]")
{
    const auto bytes = example_map_with_object_records();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);

    qmap::PrototypeDatabase prototypes;
    prototypes.add(qmap::PrototypeRecord{
        0x00000002,
        qmap::BinaryObjectType::item,
        0,
    });

    const auto parsed = qmap::parse_binary_map_object_records(
        bytes,
        scripts.value().end_offset,
        header.value(),
        prototypes
    );

    REQUIRE(parsed);
    CHECK(parsed.value().total_count == 2);
    CHECK(parsed.value().elevation_counts[0] == 1);
    CHECK(parsed.value().elevation_counts[1] == 0);
    CHECK(parsed.value().elevation_counts[2] == 1);
    REQUIRE(parsed.value().records.size() == 2);
    CHECK(parsed.value().records[0].prefix.obj_id == 100);
    CHECK(parsed.value().records[0].prefix.elevation == 0);
    CHECK(parsed.value().records[0].tail.size == 4 * sizeof(std::int32_t));
    CHECK(parsed.value().records[1].prefix.obj_id == 200);
    CHECK(parsed.value().records[1].prefix.elevation == 2);
    CHECK(parsed.value().records[1].tail.size == 10 * sizeof(std::int32_t));
    CHECK(parsed.value().end_offset == bytes.size());
}

TEST_CASE("parse_binary_map_object_records parses inventory object records", "[map][binary]")
{
    const auto bytes = example_map_with_inventory_object();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);

    qmap::PrototypeDatabase prototypes;
    prototypes.add(qmap::PrototypeRecord{
        0x00000002,
        qmap::BinaryObjectType::item,
        0,
    });

    const auto parsed = qmap::parse_binary_map_object_records(
        bytes,
        scripts.value().end_offset,
        header.value(),
        prototypes
    );

    REQUIRE(parsed);
    CHECK(parsed.value().total_count == 1);
    REQUIRE(parsed.value().records.size() == 1);
    const auto& parent = parsed.value().records[0];
    CHECK(parent.prefix.obj_id == 100);
    CHECK(parent.prefix.inventory_count == 1);
    CHECK(parent.prefix.inventory_size == 4);
    CHECK(parent.inventory_quantities == std::vector<std::int32_t>{3});
    REQUIRE(parent.inventory.size() == 1);
    CHECK(parent.inventory[0].prefix.obj_id == 101);
    CHECK(parent.inventory[0].object_type == qmap::BinaryObjectType::item);
    CHECK(parent.inventory[0].prefix.tile == -1);
    CHECK(parent.inventory[0].prefix.pid == 0x00000002);
    CHECK(parent.inventory[0].tail.empty());
    CHECK(parent.raw.end() == bytes.size() - 2 * sizeof(std::int32_t));
    CHECK(parsed.value().end_offset == bytes.size());
}

TEST_CASE("parse_binary_map_object_records uses prototype tails for inventory items", "[map][binary]")
{
    auto bytes = example_map_with_scripts();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);
    append_i32(bytes, 1);
    append_i32(bytes, 1);
    append_object_prefix(bytes, 100, 0, 0x03000001, 50331649, 2, 8);
    append_i32(bytes, 2);
    append_object_prefix(bytes, 101, -1, 0x00000001, -1, 0, 0, -1);
    append_i32(bytes, 50);
    append_i32(bytes, 51);
    append_i32(bytes, 5);
    append_object_prefix(bytes, 102, -1, 0x00000002, -1, 0, 0, -1);
    append_i32(bytes, 0);
    append_i32(bytes, 0);

    qmap::PrototypeDatabase prototypes;
    prototypes.add(qmap::PrototypeRecord{
        0x00000001,
        qmap::BinaryObjectType::item,
        3,
    });
    prototypes.add(qmap::PrototypeRecord{
        0x00000002,
        qmap::BinaryObjectType::item,
        0,
    });

    const auto parsed = qmap::parse_binary_map_object_records(
        bytes,
        scripts.value().end_offset,
        header.value(),
        prototypes
    );

    REQUIRE(parsed);
    REQUIRE(parsed.value().records.size() == 1);
    const auto& parent = parsed.value().records[0];
    CHECK(parent.inventory_quantities == std::vector<std::int32_t>{2, 5});
    REQUIRE(parent.inventory.size() == 2);
    CHECK(parent.inventory[0].prefix.obj_id == 101);
    CHECK(parent.inventory[0].tail.size == 2 * sizeof(std::int32_t));
    CHECK(parent.inventory[1].prefix.obj_id == 102);
    CHECK(parent.inventory[1].tail.empty());
    CHECK(parsed.value().diagnostics.empty());
    CHECK(parsed.value().end_offset == bytes.size());
}

TEST_CASE("parse_binary_map_object_records rejects direct inventory children without quantity", "[map][binary]")
{
    auto bytes = example_map_with_scripts();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);
    append_i32(bytes, 1);
    append_i32(bytes, 1);
    append_object_prefix(bytes, 100, 0, 0x03000001, 50331649, 1, 4);
    append_object_prefix(bytes, 101, -1, 0x00000002, -1, 0, 0, -1);
    append_i32(bytes, 0);
    append_i32(bytes, 0);

    qmap::PrototypeDatabase prototypes;
    prototypes.add(qmap::PrototypeRecord{
        0x00000002,
        qmap::BinaryObjectType::item,
        0,
    });

    const auto parsed = qmap::parse_binary_map_object_records(
        bytes,
        scripts.value().end_offset,
        header.value(),
        prototypes
    );

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message.rfind("elevation 0 object 0:", 0) == 0);
}

TEST_CASE("parse_binary_map_object_records rejects negative inventory counts", "[map][binary]")
{
    auto bytes = example_map_with_scripts();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);
    append_i32(bytes, 1);
    append_i32(bytes, 1);
    append_object_prefix(bytes, 100, 0, 0x03000001, 50331649, -1);

    const auto parsed = qmap::parse_binary_map_object_records(bytes, scripts.value().end_offset, header.value());

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "elevation 0 object 0: negative inventory object count");
}

TEST_CASE("parse_binary_map_object_records rejects negative inventory slot capacity", "[map][binary]")
{
    auto bytes = example_map_with_scripts();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);
    append_i32(bytes, 1);
    append_i32(bytes, 1);
    append_object_prefix(bytes, 100, 0, 0x03000001, 50331649, 0, -1);

    const auto parsed = qmap::parse_binary_map_object_records(bytes, scripts.value().end_offset, header.value());

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "elevation 0 object 0: negative inventory slot capacity");
}

TEST_CASE("parse_binary_scenery_tail decodes preserved scenery tail fields", "[map][binary]")
{
    std::vector<std::byte> bytes;
    append_i32_repeated(bytes, 3, 9000);

    const auto parsed = qmap::parse_binary_scenery_tail(bytes, qmap::Range{0, bytes.size()});

    REQUIRE(parsed);
    CHECK(parsed.value().flags == 9000);
    CHECK(parsed.value().door_flags == 9001);
    CHECK(parsed.value().destination == 9002);
}

TEST_CASE("parse_binary_item_tail decodes prototype subtype tails", "[map][binary]")
{
    std::vector<std::byte> weapon_bytes;
    append_i32(weapon_bytes, 12);
    append_i32(weapon_bytes, 0x00000002);
    const auto weapon = qmap::parse_binary_item_tail(
        weapon_bytes,
        qmap::Range{0, weapon_bytes.size()},
        qmap::PrototypeRecord{0x00000001, qmap::BinaryObjectType::item, qmap::item_weapon}
    );
    REQUIRE(weapon);
    CHECK(weapon.value().weapon_ammo_count == 12);
    CHECK(weapon.value().weapon_ammo_pid == 0x00000002);
    CHECK_FALSE(weapon.value().ammo_quantity);

    std::vector<std::byte> ammo_bytes;
    append_i32(ammo_bytes, 24);
    const auto ammo = qmap::parse_binary_item_tail(
        ammo_bytes,
        qmap::Range{0, ammo_bytes.size()},
        qmap::PrototypeRecord{0x00000002, qmap::BinaryObjectType::item, qmap::item_ammo}
    );
    REQUIRE(ammo);
    CHECK(ammo.value().ammo_quantity == 24);

    std::vector<std::byte> misc_bytes;
    append_i32(misc_bytes, 5);
    const auto misc = qmap::parse_binary_item_tail(
        misc_bytes,
        qmap::Range{0, misc_bytes.size()},
        qmap::PrototypeRecord{0x00000003, qmap::BinaryObjectType::item, qmap::item_misc}
    );
    REQUIRE(misc);
    CHECK(misc.value().misc_charges == 5);

    std::vector<std::byte> key_bytes;
    append_i32(key_bytes, 1234);
    const auto key = qmap::parse_binary_item_tail(
        key_bytes,
        qmap::Range{0, key_bytes.size()},
        qmap::PrototypeRecord{0x00000004, qmap::BinaryObjectType::item, qmap::item_key}
    );
    REQUIRE(key);
    CHECK(key.value().key_code == 1234);
}

TEST_CASE("parse_binary_item_tail rejects subtype size mismatch", "[map][binary]")
{
    std::vector<std::byte> bytes;
    append_i32(bytes, 12);

    const auto parsed = qmap::parse_binary_item_tail(
        bytes,
        qmap::Range{0, bytes.size()},
        qmap::PrototypeRecord{0x00000001, qmap::BinaryObjectType::item, qmap::item_weapon}
    );

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "item tail size does not match prototype subtype");
}

TEST_CASE("parse_binary_scenery_subtype_tail decodes prototype subtype tails", "[map][binary]")
{
    std::vector<std::byte> door_bytes;
    append_i32(door_bytes, 0x10);
    const auto door = qmap::parse_binary_scenery_subtype_tail(
        door_bytes,
        qmap::Range{0, door_bytes.size()},
        qmap::PrototypeRecord{0x02000001, qmap::BinaryObjectType::scenery, qmap::scenery_door},
        qmap::fallout_2_map_version
    );
    REQUIRE(door);
    CHECK(door.value().door_walkthrough == 0x10);
    CHECK_FALSE(door.value().destination_tile_and_elevation);

    std::vector<std::byte> stairs_bytes;
    append_i32(stairs_bytes, 20000);
    append_i32(stairs_bytes, 2);
    const auto stairs = qmap::parse_binary_scenery_subtype_tail(
        stairs_bytes,
        qmap::Range{0, stairs_bytes.size()},
        qmap::PrototypeRecord{0x02000002, qmap::BinaryObjectType::scenery, qmap::scenery_stairs},
        qmap::fallout_2_map_version
    );
    REQUIRE(stairs);
    CHECK(stairs.value().destination_tile_and_elevation == 20000);
    CHECK(stairs.value().destination_map == 2);

    std::vector<std::byte> elevator_bytes;
    append_i32(elevator_bytes, 3);
    append_i32(elevator_bytes, 4);
    const auto elevator = qmap::parse_binary_scenery_subtype_tail(
        elevator_bytes,
        qmap::Range{0, elevator_bytes.size()},
        qmap::PrototypeRecord{0x02000004, qmap::BinaryObjectType::scenery, qmap::scenery_elevator},
        qmap::fallout_2_map_version
    );
    REQUIRE(elevator);
    CHECK(elevator.value().elevator_type == 3);
    CHECK(elevator.value().elevator_level == 4);

    std::vector<std::byte> ladder_bytes;
    append_i32(ladder_bytes, 21000);
    const auto ladder = qmap::parse_binary_scenery_subtype_tail(
        ladder_bytes,
        qmap::Range{0, ladder_bytes.size()},
        qmap::PrototypeRecord{0x02000003, qmap::BinaryObjectType::scenery, qmap::scenery_ladder_up},
        qmap::fallout_1_map_version
    );
    REQUIRE(ladder);
    CHECK(ladder.value().destination_tile_and_elevation == 21000);
    CHECK_FALSE(ladder.value().destination_map);
}

TEST_CASE("parse_binary_critter_tail decodes preserved critter tail fields", "[map][binary]")
{
    const auto bytes = example_map_with_object_records();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);
    auto objects = qmap::parse_binary_map_object_records(bytes, scripts.value().end_offset, header.value());
    REQUIRE(objects);

    const auto parsed = qmap::parse_binary_critter_tail(bytes, objects.value().records[1].tail);

    REQUIRE(parsed);
    CHECK(parsed.value().reaction == 9100);
    CHECK(parsed.value().damage_last_turn == 9103);
    CHECK(parsed.value().group_id == 9105);
    CHECK(parsed.value().poison == 9109);
}

TEST_CASE("parse_binary_misc_tail decodes preserved misc tail fields", "[map][binary]")
{
    auto bytes = example_map_with_scripts();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);
    append_i32(bytes, 1);
    append_i32(bytes, 1);
    append_object_prefix(bytes, 300, 0, 0x05000001, -1);
    append_i32(bytes, 0);
    append_i32(bytes, 0);
    append_i32(bytes, 0);
    auto objects = qmap::parse_binary_map_object_records(bytes, scripts.value().end_offset, header.value());
    REQUIRE(objects);

    std::vector<std::byte> tail_bytes;
    append_i32_repeated(tail_bytes, 5, 9200);

    const auto parsed = qmap::parse_binary_misc_tail(tail_bytes, qmap::Range{0, tail_bytes.size()});

    REQUIRE(parsed);
    CHECK(parsed.value().flags == 9200);
    CHECK(parsed.value().dest_map == 9201);
    CHECK(parsed.value().dest_tile == 9202);
    CHECK(parsed.value().dest_elevation == 9203);
    CHECK(parsed.value().dest_rotation == 9204);
}

TEST_CASE("binary tail accessors reject invalid ranges", "[map][binary]")
{
    const auto bytes = example_map_with_object_records();

    const auto parsed = qmap::parse_binary_scenery_tail(bytes, qmap::Range{bytes.size() + 1, 4});

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "invalid scenery tail range");
}

TEST_CASE("parse_binary_map_object_records rejects unknown object types", "[map][binary]")
{
    auto bytes = example_map_with_scripts();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);
    append_i32(bytes, 1);
    append_i32(bytes, 1);
    append_object_prefix(bytes, 100, 0, 0x0A000001, 50331649);

    const auto parsed = qmap::parse_binary_map_object_records(bytes, scripts.value().end_offset, header.value());

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "elevation 0 object 0: unsupported object pid type 10 from pid 167772161");
}

TEST_CASE("parse_binary_map_object_records rejects truncated known tails", "[map][binary]")
{
    auto bytes = example_map_with_object_records();
    bytes.resize(bytes.size() - 1);
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);

    const auto parsed = qmap::parse_binary_map_object_records(bytes, scripts.value().end_offset, header.value());

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "elevation 2 object 0: unexpected end of input");
}

TEST_CASE("parse_binary_map composes header variables tiles scripts and objects", "[map][binary]")
{
    const auto bytes = example_map_with_object_records();

    const auto parsed = qmap::parse_binary_map(bytes);

    REQUIRE(parsed);
    CHECK(parsed.value().header.filename_string() == "TEST.MAP");
    CHECK(parsed.value().variables.map_vars.size() == 7);
    CHECK(parsed.value().tiles.elevations[0].size() == 40000);
    CHECK(parsed.value().scripts.by_type[1].size() == 1);
    CHECK(parsed.value().objects.records.size() == 2);
    CHECK(parsed.value().objects.end_offset == bytes.size());
}

TEST_CASE("parse_binary_map accepts prototype metadata for composed object parsing", "[map][binary]")
{
    auto bytes = example_map_with_object_records();

    qmap::PrototypeDatabase prototypes;
    prototypes.add(qmap::PrototypeRecord{
        0x00000001,
        qmap::BinaryObjectType::item,
        3,
    });

    const auto parsed = qmap::parse_binary_map(bytes, prototypes);

    REQUIRE(parsed);
    REQUIRE(parsed.value().objects.records.size() == 2);
    CHECK(parsed.value().objects.records[0].tail.size == 4 * sizeof(std::int32_t));
    CHECK(parsed.value().objects.end_offset == bytes.size());
}

TEST_CASE("parse_binary_map parses real fixture object records with prototype metadata", "[map][binary][fixture]")
{
    const auto proto_root = local_proto_root();
    if (!std::filesystem::exists(proto_root)) {
        SKIP("requires extracted Fallout proto data under .local_fallout2_data/proto");
    }

    const auto loaded = qmap::load_prototype_database(proto_root);
    REQUIRE(loaded);

    struct FixtureExpectation {
        const char* filename;
        std::size_t top_level_records;
        std::size_t records_with_inventory;
        std::size_t inventory_records;
    };

    constexpr FixtureExpectation fixtures[] = {
        {"ARVILL2.map", 1141, 1141, 0},
        {"BROKEN1.map", 3533, 3686, 153},
        {"BROKEN2.map", 7102, 7150, 48},
        {"Newr1.map", 3910, 4226, 316},
        {"Newr2.map", 5229, 5773, 544},
    };

    FixtureObjectSummary corpus_summary;
    for (const auto& fixture : fixtures) {
        CAPTURE(fixture.filename);
        const auto bytes = load_binary_fixture(fixture.filename);
        const auto parsed = qmap::parse_binary_map(bytes, loaded.value());

        REQUIRE(parsed);
        CHECK(parsed.value().objects.records.size() == fixture.top_level_records);
        CHECK(count_records_including_inventory(parsed.value().objects.records)
            == fixture.records_with_inventory);
        const auto summary = summarize_fixture_objects(
            bytes,
            parsed.value().header,
            parsed.value().objects,
            loaded.value()
        );
        CHECK(summary.top_level_records == fixture.top_level_records);
        CHECK(summary.records_with_inventory == fixture.records_with_inventory);
        CHECK(summary.inventory_records == fixture.inventory_records);
        corpus_summary.inventory_parent_records += summary.inventory_parent_records;
        corpus_summary.item_tails += summary.item_tails;
        corpus_summary.weapon_tails += summary.weapon_tails;
        corpus_summary.ammo_tails += summary.ammo_tails;
        corpus_summary.misc_item_tails += summary.misc_item_tails;
        corpus_summary.key_tails += summary.key_tails;
        corpus_summary.scenery_tails += summary.scenery_tails;
        corpus_summary.door_tails += summary.door_tails;
        corpus_summary.stairs_tails += summary.stairs_tails;
        corpus_summary.elevator_tails += summary.elevator_tails;
        corpus_summary.ladder_tails += summary.ladder_tails;
        corpus_summary.misc_exit_grid_tails += summary.misc_exit_grid_tails;
        corpus_summary.critter_tails += summary.critter_tails;
        corpus_summary.max_inventory_depth = std::max(
            corpus_summary.max_inventory_depth,
            summary.max_inventory_depth
        );
        CHECK(parsed.value().objects.diagnostics.empty());
    }

    CHECK(corpus_summary.inventory_parent_records > 0);
    CHECK(corpus_summary.max_inventory_depth > 0);
    CHECK(corpus_summary.item_tails > 0);
    CHECK(corpus_summary.weapon_tails > 0);
    CHECK(corpus_summary.ammo_tails > 0);
    CHECK(corpus_summary.misc_item_tails > 0);
    CHECK(corpus_summary.key_tails > 0);
    CHECK(corpus_summary.scenery_tails > 0);
    CHECK(corpus_summary.door_tails > 0);
    CHECK(corpus_summary.stairs_tails > 0);
    CHECK(corpus_summary.elevator_tails == 0);
    CHECK(corpus_summary.ladder_tails > 0);
    CHECK(corpus_summary.misc_exit_grid_tails > 0);
    CHECK(corpus_summary.critter_tails > 0);
}
