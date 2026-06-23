#include "prototype_metadata.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

void append_i32_be(std::vector<std::byte>& bytes, std::int32_t value)
{
    const auto unsigned_value = static_cast<std::uint32_t>(value);
    bytes.push_back(static_cast<std::byte>((unsigned_value >> 24) & 0xFF));
    bytes.push_back(static_cast<std::byte>((unsigned_value >> 16) & 0xFF));
    bytes.push_back(static_cast<std::byte>((unsigned_value >> 8) & 0xFF));
    bytes.push_back(static_cast<std::byte>(unsigned_value & 0xFF));
}

std::vector<std::byte> prototype_bytes(std::int32_t pid, std::int32_t subtype)
{
    std::vector<std::byte> bytes;
    append_i32_be(bytes, pid);
    bytes.resize(0x20);
    append_i32_be(bytes, subtype);
    return bytes;
}

std::vector<std::byte> bare_prototype_bytes(std::int32_t pid)
{
    std::vector<std::byte> bytes;
    append_i32_be(bytes, pid);
    return bytes;
}

void write_binary_file(const std::filesystem::path& path, const std::vector<std::byte>& bytes)
{
    std::ofstream output(path, std::ios::binary);
    for (const auto value : bytes) {
        output.put(static_cast<char>(value));
    }
}

void write_text_file(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream output(path, std::ios::binary);
    output << text;
}

std::filesystem::path make_temp_proto_root()
{
    const auto root = std::filesystem::temp_directory_path()
        / "qmap_prototype_metadata_tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

void write_kind(
    const std::filesystem::path& root,
    std::string directory,
    std::string list_name,
    std::string pro_name,
    const std::vector<std::byte>& bytes
)
{
    const auto dir = root / directory;
    std::filesystem::create_directories(dir);
    write_text_file(dir / list_name, pro_name + "\n");
    write_binary_file(dir / pro_name, bytes);
}

} // namespace

TEST_CASE("parse_prototype_list maps non-empty lines to one-based indexes", "[prototype]")
{
    const auto parsed = qmap::parse_prototype_list(
        "00000003.pro ; comment\r\n"
        "\r\n"
        "0000000A.pro extra text\n"
    );

    REQUIRE(parsed);
    REQUIRE(parsed.value().size() == 2);
    CHECK(parsed.value()[0].index == 1);
    CHECK(parsed.value()[0].filename == "00000003.pro");
    CHECK(parsed.value()[1].index == 3);
    CHECK(parsed.value()[1].filename == "0000000A.pro");
}

TEST_CASE("parse_prototype_record reads big-endian pid and subtype", "[prototype]")
{
    const auto parsed = qmap::parse_prototype_record(
        prototype_bytes(0x0200002A, 4),
        qmap::BinaryObjectType::scenery
    );

    REQUIRE(parsed);
    CHECK(parsed.value().pid == 0x0200002A);
    CHECK(parsed.value().object_type == qmap::BinaryObjectType::scenery);
    CHECK(parsed.value().subtype == 4);
}

TEST_CASE("parse_prototype_record rejects short typed prototypes", "[prototype]")
{
    const auto parsed = qmap::parse_prototype_record(
        bare_prototype_bytes(0x00000001),
        qmap::BinaryObjectType::item
    );

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "prototype file too short for subtype");
}

TEST_CASE("object_tail_size_from_prototype resolves item subtype map tails", "[prototype]")
{
    CHECK(qmap::object_tail_size_from_prototype({
        0x00000001,
        qmap::BinaryObjectType::item,
        3,
    }, 20) == 8);
    CHECK(qmap::object_tail_size_from_prototype({
        0x00000002,
        qmap::BinaryObjectType::item,
        4,
    }, 20) == 4);
    CHECK(qmap::object_tail_size_from_prototype({
        0x00000003,
        qmap::BinaryObjectType::item,
        1,
    }, 20) == 0);
    CHECK(qmap::object_tail_size_from_prototype({
        0x00000004,
        qmap::BinaryObjectType::item,
        99,
    }, 20) == std::nullopt);
}

TEST_CASE("object_tail_size_from_prototype resolves scenery subtype map tails", "[prototype]")
{
    CHECK(qmap::object_tail_size_from_prototype({
        0x02000001,
        qmap::BinaryObjectType::scenery,
        0,
    }, 20) == 4);
    CHECK(qmap::object_tail_size_from_prototype({
        0x02000002,
        qmap::BinaryObjectType::scenery,
        1,
    }, 20) == 8);
    CHECK(qmap::object_tail_size_from_prototype({
        0x02000003,
        qmap::BinaryObjectType::scenery,
        3,
    }, 19) == 4);
    CHECK(qmap::object_tail_size_from_prototype({
        0x02000004,
        qmap::BinaryObjectType::scenery,
        3,
    }, 20) == 8);
    CHECK(qmap::object_tail_size_from_prototype({
        0x02000005,
        qmap::BinaryObjectType::scenery,
        5,
    }, 20) == 0);
}

TEST_CASE("load_prototype_database reads extracted proto tree", "[prototype]")
{
    const auto root = make_temp_proto_root();
    write_kind(root, "ITEMS", "ITEMS.LST", "00000001.PRO", prototype_bytes(0x00000001, 3));
    write_kind(root, "CRITTERS", "CRITTERS.LST", "00000001.PRO", bare_prototype_bytes(0x01000001));
    write_kind(root, "SCENERY", "SCENERY.LST", "00000001.PRO", prototype_bytes(0x02000001, 2));
    write_kind(root, "WALLS", "WALLS.LST", "00000001.PRO", bare_prototype_bytes(0x03000001));
    write_kind(root, "TILES", "TILES.LST", "00000001.PRO", bare_prototype_bytes(0x04000001));
    write_kind(root, "MISC", "MISC.LST", "00000001.PRO", bare_prototype_bytes(0x05000001));

    const auto database = qmap::load_prototype_database(root);

    REQUIRE(database);
    CHECK(database.value().size() == 6);

    const auto item = database.value().find(0x00000001);
    REQUIRE(item);
    CHECK(item->subtype == 3);

    const auto scenery = database.value().find(0x02000001);
    REQUIRE(scenery);
    CHECK(scenery->subtype == 2);

    CHECK(database.value().find(0x05000002) == std::nullopt);

    std::filesystem::remove_all(root);
}
