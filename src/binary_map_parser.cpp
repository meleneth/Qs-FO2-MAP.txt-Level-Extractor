#include "binary_map_parser.h"

#include "byte_reader.h"

#include <algorithm>

namespace qmap {
namespace {

constexpr std::int32_t map_elev_0_absent = 0x2;
constexpr std::int32_t map_elev_1_absent = 0x4;
constexpr std::int32_t map_elev_2_absent = 0x8;

Result<std::int32_t> read_i32(ByteReader& reader)
{
    auto value = reader.read_i32_be();
    if (!value) {
        return Result<std::int32_t>::fail(value.error());
    }
    return Result<std::int32_t>::ok(value.value());
}

Result<std::uint32_t> read_u32(ByteReader& reader)
{
    auto value = reader.read_u32_be();
    if (!value) {
        return Result<std::uint32_t>::fail(value.error());
    }
    return Result<std::uint32_t>::ok(value.value());
}

} // namespace

std::string BinaryMapHeader::filename_string() const
{
    const auto end = std::find(filename.begin(), filename.end(), '\0');
    return std::string(filename.begin(), end);
}

bool BinaryMapHeader::has_elevation(int elevation) const
{
    switch (elevation) {
    case 0:
        return (map_flags & map_elev_0_absent) == 0;
    case 1:
        return (map_flags & map_elev_1_absent) == 0;
    case 2:
        return (map_flags & map_elev_2_absent) == 0;
    default:
        return false;
    }
}

Result<BinaryMapHeader> parse_binary_map_header(std::span<const std::byte> bytes)
{
    ByteReader reader(bytes);
    BinaryMapHeader header;

    auto version = read_u32(reader);
    if (!version) {
        return Result<BinaryMapHeader>::fail(version.error());
    }
    header.version = version.value();

    auto filename = reader.read_bytes(binary_map_filename_size);
    if (!filename) {
        return Result<BinaryMapHeader>::fail(filename.error());
    }
    for (std::size_t index = 0; index < header.filename.size(); ++index) {
        header.filename[index] = static_cast<char>(filename.value()[index]);
    }

    auto dude_start = read_i32(reader);
    if (!dude_start) {
        return Result<BinaryMapHeader>::fail(dude_start.error());
    }
    header.dude_start = dude_start.value();

    auto elev_start = read_i32(reader);
    if (!elev_start) {
        return Result<BinaryMapHeader>::fail(elev_start.error());
    }
    header.elev_start = elev_start.value();

    auto face_start = read_i32(reader);
    if (!face_start) {
        return Result<BinaryMapHeader>::fail(face_start.error());
    }
    header.face_start = face_start.value();

    auto lvar_count = read_i32(reader);
    if (!lvar_count) {
        return Result<BinaryMapHeader>::fail(lvar_count.error());
    }
    header.lvar_count = lvar_count.value();

    auto map_script_id = read_i32(reader);
    if (!map_script_id) {
        return Result<BinaryMapHeader>::fail(map_script_id.error());
    }
    header.map_script_id = map_script_id.value();

    auto map_flags = read_i32(reader);
    if (!map_flags) {
        return Result<BinaryMapHeader>::fail(map_flags.error());
    }
    header.map_flags = map_flags.value();

    auto light_level = read_i32(reader);
    if (!light_level) {
        return Result<BinaryMapHeader>::fail(light_level.error());
    }
    header.light_level = light_level.value();

    auto mvar_count = read_i32(reader);
    if (!mvar_count) {
        return Result<BinaryMapHeader>::fail(mvar_count.error());
    }
    header.mvar_count = mvar_count.value();

    auto map_id = read_i32(reader);
    if (!map_id) {
        return Result<BinaryMapHeader>::fail(map_id.error());
    }
    header.map_id = map_id.value();

    auto game_ticks = read_u32(reader);
    if (!game_ticks) {
        return Result<BinaryMapHeader>::fail(game_ticks.error());
    }
    header.game_ticks = game_ticks.value();

    for (auto& word : header.unknown) {
        auto value = read_i32(reader);
        if (!value) {
            return Result<BinaryMapHeader>::fail(value.error());
        }
        word = value.value();
    }

    return Result<BinaryMapHeader>::ok(header);
}

Result<BinaryMapVariables> parse_binary_map_variables(
    std::span<const std::byte> bytes,
    const BinaryMapHeader& header
)
{
    if (header.mvar_count < 0) {
        return Result<BinaryMapVariables>::fail({"negative map variable count", 0});
    }
    if (header.lvar_count < 0) {
        return Result<BinaryMapVariables>::fail({"negative local variable count", 0});
    }

    ByteReader reader(bytes);
    auto skipped_header = reader.read_bytes(binary_map_header_size);
    if (!skipped_header) {
        return Result<BinaryMapVariables>::fail(skipped_header.error());
    }

    BinaryMapVariables variables;
    variables.map_vars.reserve(static_cast<std::size_t>(header.mvar_count));
    for (std::int32_t index = 0; index < header.mvar_count; ++index) {
        auto value = read_i32(reader);
        if (!value) {
            return Result<BinaryMapVariables>::fail(value.error());
        }
        variables.map_vars.push_back(value.value());
    }

    variables.local_vars.reserve(static_cast<std::size_t>(header.lvar_count));
    for (std::int32_t index = 0; index < header.lvar_count; ++index) {
        auto value = read_i32(reader);
        if (!value) {
            return Result<BinaryMapVariables>::fail(value.error());
        }
        variables.local_vars.push_back(value.value());
    }

    return Result<BinaryMapVariables>::ok(std::move(variables));
}

} // namespace qmap
