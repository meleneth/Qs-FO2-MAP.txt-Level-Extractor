#include "binary_map_parser.h"

#include "byte_reader.h"

#include <algorithm>

namespace qmap {
namespace {

constexpr std::int32_t map_elev_0_absent = 0x2;
constexpr std::int32_t map_elev_1_absent = 0x4;
constexpr std::int32_t map_elev_2_absent = 0x8;
constexpr std::size_t tile_count_per_elevation = 10000;
constexpr std::size_t tile_bytes_per_elevation = tile_count_per_elevation * sizeof(std::uint32_t);
constexpr int serialized_script_block_capacity = 16;

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

std::size_t variable_byte_count(const BinaryMapHeader& header)
{
    return (static_cast<std::size_t>(header.mvar_count) + static_cast<std::size_t>(header.lvar_count))
        * sizeof(std::int32_t);
}

std::size_t tile_byte_count(const BinaryMapHeader& header)
{
    std::size_t size = 0;
    for (int elevation = 0; elevation < 3; ++elevation) {
        if (header.has_elevation(elevation)) {
            size += tile_bytes_per_elevation;
        }
    }
    return size;
}

bool has_valid_counts(const BinaryMapHeader& header)
{
    return header.mvar_count >= 0 && header.lvar_count >= 0;
}

Result<void> skip_map_prefix(ByteReader& reader, const BinaryMapHeader& header, bool include_tiles)
{
    if (header.mvar_count < 0) {
        return Result<void>::fail({"negative map variable count", 0});
    }
    if (header.lvar_count < 0) {
        return Result<void>::fail({"negative local variable count", 0});
    }

    auto prefix_size = binary_map_header_size + variable_byte_count(header);
    if (include_tiles) {
        prefix_size += tile_byte_count(header);
    }
    auto prefix = reader.read_bytes(prefix_size);
    if (!prefix) {
        return Result<void>::fail(prefix.error());
    }
    return Result<void>::ok();
}

int script_type_index(BinaryScriptType type)
{
    return static_cast<int>(type);
}

int script_record_word_count(BinaryScriptType type)
{
    int words = 16;
    if (type == BinaryScriptType::spatial) {
        words += 2;
    }
    if (type == BinaryScriptType::timed) {
        words += 1;
    }
    return words;
}

Result<BinaryScriptRecord> parse_script_record(ByteReader& reader, BinaryScriptType type)
{
    const auto start = reader.offset();
    BinaryScriptRecord record;
    record.type = type;

    auto scr_id = read_i32(reader);
    if (!scr_id) {
        return Result<BinaryScriptRecord>::fail(scr_id.error());
    }
    record.scr_id = scr_id.value();

    auto scr_next = read_i32(reader);
    if (!scr_next) {
        return Result<BinaryScriptRecord>::fail(scr_next.error());
    }
    record.scr_next = scr_next.value();

    if (type == BinaryScriptType::spatial) {
        auto spatial_tile = read_i32(reader);
        if (!spatial_tile) {
            return Result<BinaryScriptRecord>::fail(spatial_tile.error());
        }
        record.spatial_tile = spatial_tile.value();

        auto spatial_radius = read_i32(reader);
        if (!spatial_radius) {
            return Result<BinaryScriptRecord>::fail(spatial_radius.error());
        }
        record.spatial_radius = spatial_radius.value();
    }

    if (type == BinaryScriptType::timed) {
        auto time = read_i32(reader);
        if (!time) {
            return Result<BinaryScriptRecord>::fail(time.error());
        }
        record.time = time.value();
    }

    auto scr_flags = read_i32(reader);
    if (!scr_flags) {
        return Result<BinaryScriptRecord>::fail(scr_flags.error());
    }
    record.scr_flags = scr_flags.value();

    auto scr_index = read_i32(reader);
    if (!scr_index) {
        return Result<BinaryScriptRecord>::fail(scr_index.error());
    }
    record.scr_index = scr_index.value();

    auto program_ptr = read_i32(reader);
    if (!program_ptr) {
        return Result<BinaryScriptRecord>::fail(program_ptr.error());
    }
    record.program_ptr = program_ptr.value();

    auto scr_obj_id = read_u32(reader);
    if (!scr_obj_id) {
        return Result<BinaryScriptRecord>::fail(scr_obj_id.error());
    }
    record.scr_obj_id = scr_obj_id.value();

    auto lvar_offset = read_i32(reader);
    if (!lvar_offset) {
        return Result<BinaryScriptRecord>::fail(lvar_offset.error());
    }
    record.lvar_offset = lvar_offset.value();

    auto lvar_count = read_i32(reader);
    if (!lvar_count) {
        return Result<BinaryScriptRecord>::fail(lvar_count.error());
    }
    record.lvar_count = lvar_count.value();

    auto last_used_value = read_i32(reader);
    if (!last_used_value) {
        return Result<BinaryScriptRecord>::fail(last_used_value.error());
    }
    record.last_used_value = last_used_value.value();

    auto current_action = read_i32(reader);
    if (!current_action) {
        return Result<BinaryScriptRecord>::fail(current_action.error());
    }
    record.current_action = current_action.value();

    auto fixed_param = read_i32(reader);
    if (!fixed_param) {
        return Result<BinaryScriptRecord>::fail(fixed_param.error());
    }
    record.fixed_param = fixed_param.value();

    auto action_id = read_i32(reader);
    if (!action_id) {
        return Result<BinaryScriptRecord>::fail(action_id.error());
    }
    record.action_id = action_id.value();

    auto override_flags = read_i32(reader);
    if (!override_flags) {
        return Result<BinaryScriptRecord>::fail(override_flags.error());
    }
    record.override_flags = override_flags.value();

    auto unknown_1 = read_i32(reader);
    if (!unknown_1) {
        return Result<BinaryScriptRecord>::fail(unknown_1.error());
    }
    record.unknown_1 = unknown_1.value();

    auto how_much = read_i32(reader);
    if (!how_much) {
        return Result<BinaryScriptRecord>::fail(how_much.error());
    }
    record.how_much = how_much.value();

    auto unknown_2 = read_i32(reader);
    if (!unknown_2) {
        return Result<BinaryScriptRecord>::fail(unknown_2.error());
    }
    record.unknown_2 = unknown_2.value();
    record.raw = Range{start, reader.offset() - start};

    return Result<BinaryScriptRecord>::ok(record);
}

Result<void> skip_script_padding_records(ByteReader& reader, BinaryScriptType type, int parsed_in_block)
{
    const auto padding_count = serialized_script_block_capacity - parsed_in_block;
    const auto padding_size = static_cast<std::size_t>(padding_count)
        * static_cast<std::size_t>(script_record_word_count(type))
        * sizeof(std::int32_t);
    auto padding = reader.read_bytes(padding_size);
    if (!padding) {
        return Result<void>::fail(padding.error());
    }
    return Result<void>::ok();
}

Result<void> read_script_block_footer(ByteReader& reader, int expected_count)
{
    auto footer_count = read_i32(reader);
    if (!footer_count) {
        return Result<void>::fail(footer_count.error());
    }
    auto footer_next = read_i32(reader);
    if (!footer_next) {
        return Result<void>::fail(footer_next.error());
    }
    if (footer_count.value() != expected_count) {
        return Result<void>::fail({"script block footer count mismatch", reader.offset() - 8});
    }
    return Result<void>::ok();
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

Result<BinaryMapTiles> parse_binary_map_tiles(
    std::span<const std::byte> bytes,
    const BinaryMapHeader& header
)
{
    if (header.mvar_count < 0) {
        return Result<BinaryMapTiles>::fail({"negative map variable count", 0});
    }
    if (header.lvar_count < 0) {
        return Result<BinaryMapTiles>::fail({"negative local variable count", 0});
    }

    ByteReader reader(bytes);
    const auto variable_bytes = variable_byte_count(header);
    auto prefix = reader.read_bytes(binary_map_header_size + variable_bytes);
    if (!prefix) {
        return Result<BinaryMapTiles>::fail(prefix.error());
    }

    BinaryMapTiles tiles;
    for (int elevation = 0; elevation < 3; ++elevation) {
        if (!header.has_elevation(elevation)) {
            continue;
        }

        auto tile_bytes = reader.read_bytes(tile_bytes_per_elevation);
        if (!tile_bytes) {
            return Result<BinaryMapTiles>::fail(tile_bytes.error());
        }
        tiles.elevations[elevation] = tile_bytes.value();
    }

    return Result<BinaryMapTiles>::ok(tiles);
}

Result<BinaryMapScripts> parse_binary_map_scripts(
    std::span<const std::byte> bytes,
    const BinaryMapHeader& header
)
{
    if (!has_valid_counts(header)) {
        return Result<BinaryMapScripts>::fail({
            header.mvar_count < 0 ? "negative map variable count" : "negative local variable count",
            0,
        });
    }

    ByteReader reader(bytes);
    auto skipped = skip_map_prefix(reader, header, true);
    if (!skipped) {
        return Result<BinaryMapScripts>::fail(skipped.error());
    }

    BinaryMapScripts scripts;
    for (int type_index = 0; type_index < binary_script_type_count; ++type_index) {
        const auto type = static_cast<BinaryScriptType>(type_index);
        auto count = read_i32(reader);
        if (!count) {
            return Result<BinaryMapScripts>::fail(count.error());
        }
        if (count.value() < 0) {
            return Result<BinaryMapScripts>::fail({"negative script count", reader.offset() - 4});
        }

        auto& records = scripts.by_type[script_type_index(type)];
        records.reserve(static_cast<std::size_t>(count.value()));
        int remaining = count.value();
        while (remaining > 0) {
            const auto block_count =
                remaining > serialized_script_block_capacity ? serialized_script_block_capacity : remaining;
            for (int index = 0; index < block_count; ++index) {
                auto record = parse_script_record(reader, type);
                if (!record) {
                    return Result<BinaryMapScripts>::fail(record.error());
                }
                records.push_back(record.value());
            }

            remaining -= block_count;
            if (block_count < serialized_script_block_capacity) {
                auto skipped_padding = skip_script_padding_records(reader, type, block_count);
                if (!skipped_padding) {
                    return Result<BinaryMapScripts>::fail(skipped_padding.error());
                }
            }

            auto footer = read_script_block_footer(reader, block_count);
            if (!footer) {
                return Result<BinaryMapScripts>::fail(footer.error());
            }
        }
    }

    return Result<BinaryMapScripts>::ok(std::move(scripts));
}

} // namespace qmap
