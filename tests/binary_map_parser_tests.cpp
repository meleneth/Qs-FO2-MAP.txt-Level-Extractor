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
