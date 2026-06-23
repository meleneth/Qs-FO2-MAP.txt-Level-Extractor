#include <catch2/catch_test_macros.hpp>

#include "binary_map_parser.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace {

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
    append_object_prefix(bytes, 100, 0, 0x02000001, 50331649);
    append_i32_repeated(bytes, 3, 9000);
    append_i32(bytes, 1);
    append_object_prefix(bytes, 200, 2, 0x01000002, 67108865);
    append_i32_repeated(bytes, 11, 9100);
    return bytes;
}

std::vector<std::byte> example_map_with_inventory_object()
{
    auto bytes = example_map_with_scripts();
    append_i32(bytes, 1);
    append_i32(bytes, 1);
    append_object_prefix(bytes, 100, 0, 0x02000001, 50331649, 1, 4);
    append_i32_repeated(bytes, 3, 9000);
    append_i32(bytes, 3);
    append_object_prefix(bytes, 101, -1, 0x00000002, -1, 0, 0, -1);
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
    CHECK(parsed.error().message == "object count mismatch");
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
    CHECK(parsed.value().records[0].prefix.pid == 0x02000001);
    CHECK(parsed.value().records[0].tail.size == 3 * sizeof(std::int32_t));
    CHECK(parsed.value().records[0].raw.size == 25 * sizeof(std::int32_t));
    CHECK(parsed.value().records[1].prefix.pid == 0x01000002);
    CHECK(parsed.value().records[1].tail.size == 11 * sizeof(std::int32_t));
    CHECK(parsed.value().records[1].raw.size == 33 * sizeof(std::int32_t));
    CHECK(parsed.value().end_offset == bytes.size());
}

TEST_CASE("parse_binary_map_object_records follows present elevation blocks", "[map][binary]")
{
    const auto bytes = example_map_with_object_records();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);

    const auto parsed = qmap::parse_binary_map_object_records(bytes, scripts.value().end_offset, header.value());

    REQUIRE(parsed);
    CHECK(parsed.value().total_count == 2);
    CHECK(parsed.value().elevation_counts[0] == 1);
    CHECK(parsed.value().elevation_counts[1] == 0);
    CHECK(parsed.value().elevation_counts[2] == 1);
    REQUIRE(parsed.value().records.size() == 2);
    CHECK(parsed.value().records[0].prefix.obj_id == 100);
    CHECK(parsed.value().records[0].prefix.elevation == 0);
    CHECK(parsed.value().records[0].tail.size == 3 * sizeof(std::int32_t));
    CHECK(parsed.value().records[1].prefix.obj_id == 200);
    CHECK(parsed.value().records[1].prefix.elevation == 2);
    CHECK(parsed.value().records[1].tail.size == 11 * sizeof(std::int32_t));
    CHECK(parsed.value().end_offset == bytes.size());
}

TEST_CASE("parse_binary_map_object_records parses inventory object records", "[map][binary]")
{
    const auto bytes = example_map_with_inventory_object();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);

    const auto parsed = qmap::parse_binary_map_object_records(bytes, scripts.value().end_offset, header.value());

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
    CHECK(parent.inventory[0].prefix.tile == -1);
    CHECK(parent.inventory[0].prefix.pid == 0x00000002);
    CHECK(parent.inventory[0].tail.empty());
    CHECK(parent.raw.end() == bytes.size() - sizeof(std::int32_t));
    CHECK(parsed.value().end_offset == bytes.size());
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
    append_object_prefix(bytes, 100, 0, 0x02000001, 50331649, -1);
    append_i32_repeated(bytes, 3, 9000);

    const auto parsed = qmap::parse_binary_map_object_records(bytes, scripts.value().end_offset, header.value());

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "negative inventory object count");
}

TEST_CASE("parse_binary_scenery_tail decodes preserved scenery tail fields", "[map][binary]")
{
    const auto bytes = example_map_with_object_records();
    auto header = qmap::parse_binary_map_header(bytes);
    REQUIRE(header);
    auto scripts = qmap::parse_binary_map_scripts(bytes, header.value());
    REQUIRE(scripts);
    auto objects = qmap::parse_binary_map_object_records(bytes, scripts.value().end_offset, header.value());
    REQUIRE(objects);

    const auto parsed = qmap::parse_binary_scenery_tail(bytes, objects.value().records[0].tail);

    REQUIRE(parsed);
    CHECK(parsed.value().flags == 9000);
    CHECK(parsed.value().door_flags == 9001);
    CHECK(parsed.value().destination == 9002);
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
    CHECK(parsed.value().flags == 9100);
    CHECK(parsed.value().damage_last_turn == 9101);
    CHECK(parsed.value().team == 9106);
    CHECK(parsed.value().poison == 9110);
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
    append_i32_repeated(bytes, 5, 9200);
    append_i32(bytes, 0);
    auto objects = qmap::parse_binary_map_object_records(bytes, scripts.value().end_offset, header.value());
    REQUIRE(objects);

    const auto parsed = qmap::parse_binary_misc_tail(bytes, objects.value().records[0].tail);

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
    CHECK(parsed.error().message == "unsupported object pid type 10 from pid 167772161");
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
    CHECK(parsed.error().message == "unexpected end of input");
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
