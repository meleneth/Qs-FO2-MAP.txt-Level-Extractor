#include "binary_map_parser.h"

#include "byte_reader.h"

#include <algorithm>
#include <array>
#include <string>

namespace qmap {
namespace {

// Fallout MAP layout references:
// - https://falloutmods.fandom.com/wiki/MAP_File_Format
// - https://fodev.net/files/fo2/map.html
// Object records are serialized as a total count followed by per-elevation
// count + object-array blocks. Script blocks are serialized in groups of 16
// with padding records and a two-word footer/check block.
constexpr std::array<std::int32_t, binary_map_elevation_count> map_elevation_absent_flags = {
    0x2,
    0x4,
    0x8,
};
constexpr std::size_t tile_count_per_elevation = 10000;
constexpr std::size_t tile_bytes_per_elevation = tile_count_per_elevation * sizeof(std::uint32_t);
constexpr int serialized_script_block_capacity = 16;
constexpr std::size_t critter_tail_words = 11;
constexpr std::size_t scenery_tail_words = 3;
constexpr std::size_t misc_tail_words = 5;
constexpr int pid_type_shift = 24;
constexpr std::uint32_t pid_type_mask = 0xFFu;

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
    for (int elevation = 0; elevation < binary_map_elevation_count; ++elevation) {
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

std::optional<BinaryScriptType> script_type_from_high_byte(std::uint32_t script_id)
{
    const auto raw_type = static_cast<int>(script_id >> 24);
    if (raw_type < 0 || raw_type >= binary_script_type_count) {
        return std::nullopt;
    }
    return static_cast<BinaryScriptType>(raw_type);
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
    for (int slot = parsed_in_block; slot < serialized_script_block_capacity; ++slot) {
        auto script_id = read_u32(reader);
        if (!script_id) {
            return Result<void>::fail(script_id.error());
        }
        auto script_next = read_i32(reader);
        if (!script_next) {
            return Result<void>::fail(script_next.error());
        }

        const auto inferred_type =
            script_type_from_high_byte(script_id.value()).value_or(BinaryScriptType::system);
        const auto remaining_size =
            (static_cast<std::size_t>(script_record_word_count(inferred_type)) - 2)
            * sizeof(std::int32_t);
        auto remaining = reader.read_bytes(remaining_size);
        if (!remaining) {
            return Result<void>::fail(remaining.error());
        }
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

Result<BinaryObjectPrefix> parse_object_prefix(ByteReader& reader)
{
    const auto start = reader.offset();
    BinaryObjectPrefix record;

    auto obj_id = read_i32(reader);
    if (!obj_id) {
        return Result<BinaryObjectPrefix>::fail(obj_id.error());
    }
    record.obj_id = obj_id.value();

    auto tile = read_i32(reader);
    if (!tile) {
        return Result<BinaryObjectPrefix>::fail(tile.error());
    }
    record.tile = tile.value();

    auto x = read_i32(reader);
    if (!x) {
        return Result<BinaryObjectPrefix>::fail(x.error());
    }
    record.x = x.value();

    auto y = read_i32(reader);
    if (!y) {
        return Result<BinaryObjectPrefix>::fail(y.error());
    }
    record.y = y.value();

    auto screen_x = read_i32(reader);
    if (!screen_x) {
        return Result<BinaryObjectPrefix>::fail(screen_x.error());
    }
    record.screen_x = screen_x.value();

    auto screen_y = read_i32(reader);
    if (!screen_y) {
        return Result<BinaryObjectPrefix>::fail(screen_y.error());
    }
    record.screen_y = screen_y.value();

    auto frame = read_i32(reader);
    if (!frame) {
        return Result<BinaryObjectPrefix>::fail(frame.error());
    }
    record.frame = frame.value();

    auto rotation = read_i32(reader);
    if (!rotation) {
        return Result<BinaryObjectPrefix>::fail(rotation.error());
    }
    record.rotation = rotation.value();

    auto fid = read_i32(reader);
    if (!fid) {
        return Result<BinaryObjectPrefix>::fail(fid.error());
    }
    record.fid = fid.value();

    auto flags = read_i32(reader);
    if (!flags) {
        return Result<BinaryObjectPrefix>::fail(flags.error());
    }
    record.flags = flags.value();

    auto elevation = read_i32(reader);
    if (!elevation) {
        return Result<BinaryObjectPrefix>::fail(elevation.error());
    }
    record.elevation = elevation.value();

    auto pid = read_i32(reader);
    if (!pid) {
        return Result<BinaryObjectPrefix>::fail(pid.error());
    }
    record.pid = pid.value();

    auto cid = read_i32(reader);
    if (!cid) {
        return Result<BinaryObjectPrefix>::fail(cid.error());
    }
    record.cid = cid.value();

    auto light_radius = read_i32(reader);
    if (!light_radius) {
        return Result<BinaryObjectPrefix>::fail(light_radius.error());
    }
    record.light_radius = light_radius.value();

    auto light_intensity = read_i32(reader);
    if (!light_intensity) {
        return Result<BinaryObjectPrefix>::fail(light_intensity.error());
    }
    record.light_intensity = light_intensity.value();

    auto outline_color = read_i32(reader);
    if (!outline_color) {
        return Result<BinaryObjectPrefix>::fail(outline_color.error());
    }
    record.outline_color = outline_color.value();

    auto script_id = read_i32(reader);
    if (!script_id) {
        return Result<BinaryObjectPrefix>::fail(script_id.error());
    }
    record.script_id = script_id.value();

    auto script_index = read_i32(reader);
    if (!script_index) {
        return Result<BinaryObjectPrefix>::fail(script_index.error());
    }
    record.script_index = script_index.value();

    auto inventory_count = read_i32(reader);
    if (!inventory_count) {
        return Result<BinaryObjectPrefix>::fail(inventory_count.error());
    }
    record.inventory_count = inventory_count.value();

    auto inventory_size = read_i32(reader);
    if (!inventory_size) {
        return Result<BinaryObjectPrefix>::fail(inventory_size.error());
    }
    record.inventory_size = inventory_size.value();

    auto unknown_10 = read_i32(reader);
    if (!unknown_10) {
        return Result<BinaryObjectPrefix>::fail(unknown_10.error());
    }
    record.unknown_10 = unknown_10.value();

    auto unknown_11 = read_i32(reader);
    if (!unknown_11) {
        return Result<BinaryObjectPrefix>::fail(unknown_11.error());
    }
    record.unknown_11 = unknown_11.value();
    record.raw = Range{start, reader.offset() - start};

    return Result<BinaryObjectPrefix>::ok(record);
}

std::size_t known_object_tail_size(BinaryObjectType type)
{
    switch (type) {
    case BinaryObjectType::critter:
        return critter_tail_words * sizeof(std::int32_t);
    case BinaryObjectType::scenery:
        return scenery_tail_words * sizeof(std::int32_t);
    case BinaryObjectType::misc:
        return misc_tail_words * sizeof(std::int32_t);
    case BinaryObjectType::item:
    case BinaryObjectType::wall:
    case BinaryObjectType::tile:
    case BinaryObjectType::interface_object:
    case BinaryObjectType::inventory:
    case BinaryObjectType::head:
    case BinaryObjectType::background:
        return 0;
    }
    return 0;
}

Result<BinaryObjectRecord> parse_object_record(ByteReader& reader)
{
    const auto record_start = reader.offset();
    auto prefix = parse_object_prefix(reader);
    if (!prefix) {
        return Result<BinaryObjectRecord>::fail(prefix.error());
    }

    const auto object_type = binary_object_type_from_pid(prefix.value().pid);
    if (!object_type) {
        const auto message = std::string{"unsupported object pid type "}
            + std::to_string(prefix.value().pid_type())
            + " from pid "
            + std::to_string(prefix.value().pid);
        return Result<BinaryObjectRecord>::fail({message, prefix.value().raw.offset});
    }

    const auto tail_start = reader.offset();
    auto tail_bytes = reader.read_bytes(known_object_tail_size(*object_type));
    if (!tail_bytes) {
        return Result<BinaryObjectRecord>::fail(tail_bytes.error());
    }

    BinaryObjectRecord record;
    record.prefix = prefix.value();
    record.tail = Range{tail_start, tail_bytes.value().size()};
    record.raw = Range{record_start, reader.offset() - record_start};
    return Result<BinaryObjectRecord>::ok(record);
}

Result<BinaryMapObjectRecords> parse_object_records_after_counts(ByteReader& reader, std::size_t object_section_offset)
{
    BinaryMapObjectRecords objects;
    auto total_count = read_i32(reader);
    if (!total_count) {
        return Result<BinaryMapObjectRecords>::fail(total_count.error());
    }
    if (total_count.value() < 0) {
        return Result<BinaryMapObjectRecords>::fail({"negative object count", object_section_offset});
    }
    objects.total_count = total_count.value();

    std::int64_t summed_counts = 0;
    for (auto& count : objects.elevation_counts) {
        auto parsed = read_i32(reader);
        if (!parsed) {
            return Result<BinaryMapObjectRecords>::fail(parsed.error());
        }
        if (parsed.value() < 0) {
            return Result<BinaryMapObjectRecords>::fail({"negative elevation object count", reader.offset() - 4});
        }
        count = parsed.value();
        summed_counts += parsed.value();
        if (summed_counts > objects.total_count) {
            return Result<BinaryMapObjectRecords>::fail({"object count mismatch", object_section_offset});
        }
    }

    if (summed_counts != objects.total_count) {
        return Result<BinaryMapObjectRecords>::fail({"object count mismatch", object_section_offset});
    }

    for (std::int32_t index = 0; index < objects.total_count; ++index) {
        auto record = parse_object_record(reader);
        if (!record) {
            return Result<BinaryMapObjectRecords>::fail(record.error());
        }
        objects.records.push_back(record.value());
    }
    objects.end_offset = reader.offset();

    return Result<BinaryMapObjectRecords>::ok(std::move(objects));
}

} // namespace

std::string BinaryMapHeader::filename_string() const
{
    const auto end = std::find(filename.begin(), filename.end(), '\0');
    return std::string(filename.begin(), end);
}

bool BinaryMapHeader::has_elevation(int elevation) const
{
    if (elevation < 0 || elevation >= binary_map_elevation_count) {
        return false;
    }
    return (map_flags & map_elevation_absent_flags[static_cast<std::size_t>(elevation)]) == 0;
}

int BinaryObjectPrefix::pid_type() const
{
    return static_cast<int>((static_cast<std::uint32_t>(pid) >> pid_type_shift) & pid_type_mask);
}

std::optional<BinaryObjectType> binary_object_type_from_pid(std::int32_t pid)
{
    const auto type = static_cast<int>((static_cast<std::uint32_t>(pid) >> pid_type_shift) & pid_type_mask);
    if (type < 0 || type > static_cast<int>(BinaryObjectType::background)) {
        return std::nullopt;
    }
    return static_cast<BinaryObjectType>(type);
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
    for (std::int32_t index = 0; index < header.mvar_count; ++index) {
        auto value = read_i32(reader);
        if (!value) {
            return Result<BinaryMapVariables>::fail(value.error());
        }
        variables.map_vars.push_back(value.value());
    }

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
    for (int elevation = 0; elevation < binary_map_elevation_count; ++elevation) {
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

    scripts.end_offset = reader.offset();
    return Result<BinaryMapScripts>::ok(std::move(scripts));
}

Result<BinaryMapObjectPrefixes> parse_binary_map_object_prefixes(
    std::span<const std::byte> bytes,
    std::size_t object_section_offset
)
{
    ByteReader reader(bytes);
    auto skipped = reader.read_bytes(object_section_offset);
    if (!skipped) {
        return Result<BinaryMapObjectPrefixes>::fail(skipped.error());
    }

    BinaryMapObjectPrefixes objects;
    auto total_count = read_i32(reader);
    if (!total_count) {
        return Result<BinaryMapObjectPrefixes>::fail(total_count.error());
    }
    if (total_count.value() < 0) {
        return Result<BinaryMapObjectPrefixes>::fail({"negative object count", object_section_offset});
    }
    objects.total_count = total_count.value();

    std::int64_t summed_counts = 0;
    for (auto& count : objects.elevation_counts) {
        auto parsed = read_i32(reader);
        if (!parsed) {
            return Result<BinaryMapObjectPrefixes>::fail(parsed.error());
        }
        if (parsed.value() < 0) {
            return Result<BinaryMapObjectPrefixes>::fail({"negative elevation object count", reader.offset() - 4});
        }
        count = parsed.value();
        summed_counts += parsed.value();
        if (summed_counts > objects.total_count) {
            return Result<BinaryMapObjectPrefixes>::fail({"object count mismatch", object_section_offset});
        }
    }

    if (summed_counts != objects.total_count) {
        return Result<BinaryMapObjectPrefixes>::fail({"object count mismatch", object_section_offset});
    }

    for (std::int32_t index = 0; index < objects.total_count; ++index) {
        auto record = parse_object_prefix(reader);
        if (!record) {
            return Result<BinaryMapObjectPrefixes>::fail(record.error());
        }
        objects.records.push_back(record.value());
    }
    objects.end_offset = reader.offset();

    return Result<BinaryMapObjectPrefixes>::ok(std::move(objects));
}

Result<BinaryMapObjectCounts> parse_binary_map_object_counts(
    std::span<const std::byte> bytes,
    std::size_t object_section_offset,
    const BinaryMapHeader& header
)
{
    ByteReader reader(bytes);
    auto skipped = reader.read_bytes(object_section_offset);
    if (!skipped) {
        return Result<BinaryMapObjectCounts>::fail(skipped.error());
    }

    BinaryMapObjectCounts counts;
    auto total_count = read_i32(reader);
    if (!total_count) {
        return Result<BinaryMapObjectCounts>::fail(total_count.error());
    }
    if (total_count.value() < 0) {
        return Result<BinaryMapObjectCounts>::fail({"negative object count", object_section_offset});
    }
    counts.total_count = total_count.value();

    for (int elevation = 0; elevation < binary_map_elevation_count; ++elevation) {
        counts.elevation_counts[elevation] = 0;
        if (header.has_elevation(elevation)) {
            counts.first_counted_elevation = elevation;
            break;
        }
    }

    if (counts.first_counted_elevation >= 0) {
        auto parsed = read_i32(reader);
        if (!parsed) {
            return Result<BinaryMapObjectCounts>::fail(parsed.error());
        }
        if (parsed.value() < 0) {
            return Result<BinaryMapObjectCounts>::fail({"negative elevation object count", reader.offset() - 4});
        }
        if (parsed.value() > counts.total_count) {
            return Result<BinaryMapObjectCounts>::fail({"object count mismatch", object_section_offset});
        }
        counts.elevation_counts[counts.first_counted_elevation] = parsed.value();
    }

    counts.data_offset = reader.offset();
    return Result<BinaryMapObjectCounts>::ok(counts);
}

Result<BinaryObjectBlockHeader> parse_first_binary_object_block_header(
    std::span<const std::byte> bytes,
    std::size_t object_section_offset,
    const BinaryMapHeader& header
)
{
    ByteReader reader(bytes);
    auto skipped = reader.read_bytes(object_section_offset);
    if (!skipped) {
        return Result<BinaryObjectBlockHeader>::fail(skipped.error());
    }

    BinaryObjectBlockHeader block;
    auto total_count = read_i32(reader);
    if (!total_count) {
        return Result<BinaryObjectBlockHeader>::fail(total_count.error());
    }
    if (total_count.value() < 0) {
        return Result<BinaryObjectBlockHeader>::fail({"negative object count", object_section_offset});
    }
    block.total_count = total_count.value();

    for (int elevation = 0; elevation < binary_map_elevation_count; ++elevation) {
        if (!header.has_elevation(elevation)) {
            continue;
        }

        auto block_count = read_i32(reader);
        if (!block_count) {
            return Result<BinaryObjectBlockHeader>::fail(block_count.error());
        }
        if (block_count.value() < 0) {
            return Result<BinaryObjectBlockHeader>::fail({"negative elevation object count", reader.offset() - 4});
        }
        if (block_count.value() > block.total_count) {
            return Result<BinaryObjectBlockHeader>::fail({"object count mismatch", object_section_offset});
        }
        block.elevation = elevation;
        block.block_count = block_count.value();
        block.objects_offset = reader.offset();
        return Result<BinaryObjectBlockHeader>::ok(block);
    }

    block.objects_offset = reader.offset();
    return Result<BinaryObjectBlockHeader>::ok(block);
}

Result<std::optional<BinaryObjectPrefix>> parse_first_binary_object_prefix(
    std::span<const std::byte> bytes,
    std::size_t object_section_offset,
    const BinaryMapHeader& header
)
{
    ByteReader reader(bytes);
    auto skipped = reader.read_bytes(object_section_offset);
    if (!skipped) {
        return Result<std::optional<BinaryObjectPrefix>>::fail(skipped.error());
    }

    auto total_count = read_i32(reader);
    if (!total_count) {
        return Result<std::optional<BinaryObjectPrefix>>::fail(total_count.error());
    }
    if (total_count.value() < 0) {
        return Result<std::optional<BinaryObjectPrefix>>::fail({"negative object count", object_section_offset});
    }

    for (int elevation = 0; elevation < binary_map_elevation_count; ++elevation) {
        if (!header.has_elevation(elevation)) {
            continue;
        }

        auto block_count = read_i32(reader);
        if (!block_count) {
            return Result<std::optional<BinaryObjectPrefix>>::fail(block_count.error());
        }
        if (block_count.value() < 0) {
            return Result<std::optional<BinaryObjectPrefix>>::fail({
                "negative elevation object count",
                reader.offset() - 4,
            });
        }
        if (block_count.value() > total_count.value()) {
            return Result<std::optional<BinaryObjectPrefix>>::fail({
                "object count mismatch",
                object_section_offset,
            });
        }
        if (block_count.value() == 0) {
            continue;
        }

        auto prefix = parse_object_prefix(reader);
        if (!prefix) {
            return Result<std::optional<BinaryObjectPrefix>>::fail(prefix.error());
        }
        return Result<std::optional<BinaryObjectPrefix>>::ok(prefix.value());
    }

    return Result<std::optional<BinaryObjectPrefix>>::ok(std::nullopt);
}

Result<BinarySceneryTail> parse_binary_scenery_tail(std::span<const std::byte> bytes, Range tail)
{
    if (tail.offset > bytes.size() || tail.size > bytes.size() - tail.offset) {
        return Result<BinarySceneryTail>::fail({"invalid scenery tail range", tail.offset});
    }

    ByteReader reader(bytes.subspan(tail.offset, tail.size));
    BinarySceneryTail parsed;
    auto flags = read_i32(reader);
    if (!flags) {
        return Result<BinarySceneryTail>::fail(flags.error());
    }
    parsed.flags = flags.value();
    auto door_flags = read_i32(reader);
    if (!door_flags) {
        return Result<BinarySceneryTail>::fail(door_flags.error());
    }
    parsed.door_flags = door_flags.value();
    auto destination = read_i32(reader);
    if (!destination) {
        return Result<BinarySceneryTail>::fail(destination.error());
    }
    parsed.destination = destination.value();
    return Result<BinarySceneryTail>::ok(parsed);
}

Result<BinaryCritterTail> parse_binary_critter_tail(std::span<const std::byte> bytes, Range tail)
{
    if (tail.offset > bytes.size() || tail.size > bytes.size() - tail.offset) {
        return Result<BinaryCritterTail>::fail({"invalid critter tail range", tail.offset});
    }

    ByteReader reader(bytes.subspan(tail.offset, tail.size));
    BinaryCritterTail parsed;
    auto flags = read_i32(reader);
    if (!flags) {
        return Result<BinaryCritterTail>::fail(flags.error());
    }
    parsed.flags = flags.value();
    auto damage_last_turn = read_i32(reader);
    if (!damage_last_turn) {
        return Result<BinaryCritterTail>::fail(damage_last_turn.error());
    }
    parsed.damage_last_turn = damage_last_turn.value();
    auto combat_flags = read_i32(reader);
    if (!combat_flags) {
        return Result<BinaryCritterTail>::fail(combat_flags.error());
    }
    parsed.combat_flags = combat_flags.value();
    auto action_points = read_i32(reader);
    if (!action_points) {
        return Result<BinaryCritterTail>::fail(action_points.error());
    }
    parsed.action_points = action_points.value();
    auto combat_result = read_i32(reader);
    if (!combat_result) {
        return Result<BinaryCritterTail>::fail(combat_result.error());
    }
    parsed.combat_result = combat_result.value();
    auto ai_packet = read_i32(reader);
    if (!ai_packet) {
        return Result<BinaryCritterTail>::fail(ai_packet.error());
    }
    parsed.ai_packet = ai_packet.value();
    auto team = read_i32(reader);
    if (!team) {
        return Result<BinaryCritterTail>::fail(team.error());
    }
    parsed.team = team.value();
    auto last_hit_cid = read_i32(reader);
    if (!last_hit_cid) {
        return Result<BinaryCritterTail>::fail(last_hit_cid.error());
    }
    parsed.last_hit_cid = last_hit_cid.value();
    auto hit_points = read_i32(reader);
    if (!hit_points) {
        return Result<BinaryCritterTail>::fail(hit_points.error());
    }
    parsed.hit_points = hit_points.value();
    auto radiation = read_i32(reader);
    if (!radiation) {
        return Result<BinaryCritterTail>::fail(radiation.error());
    }
    parsed.radiation = radiation.value();
    auto poison = read_i32(reader);
    if (!poison) {
        return Result<BinaryCritterTail>::fail(poison.error());
    }
    parsed.poison = poison.value();
    return Result<BinaryCritterTail>::ok(parsed);
}

Result<BinaryMiscTail> parse_binary_misc_tail(std::span<const std::byte> bytes, Range tail)
{
    if (tail.offset > bytes.size() || tail.size > bytes.size() - tail.offset) {
        return Result<BinaryMiscTail>::fail({"invalid misc tail range", tail.offset});
    }

    ByteReader reader(bytes.subspan(tail.offset, tail.size));
    BinaryMiscTail parsed;
    auto flags = read_i32(reader);
    if (!flags) {
        return Result<BinaryMiscTail>::fail(flags.error());
    }
    parsed.flags = flags.value();
    auto dest_map = read_i32(reader);
    if (!dest_map) {
        return Result<BinaryMiscTail>::fail(dest_map.error());
    }
    parsed.dest_map = dest_map.value();
    auto dest_tile = read_i32(reader);
    if (!dest_tile) {
        return Result<BinaryMiscTail>::fail(dest_tile.error());
    }
    parsed.dest_tile = dest_tile.value();
    auto dest_elevation = read_i32(reader);
    if (!dest_elevation) {
        return Result<BinaryMiscTail>::fail(dest_elevation.error());
    }
    parsed.dest_elevation = dest_elevation.value();
    auto dest_rotation = read_i32(reader);
    if (!dest_rotation) {
        return Result<BinaryMiscTail>::fail(dest_rotation.error());
    }
    parsed.dest_rotation = dest_rotation.value();
    return Result<BinaryMiscTail>::ok(parsed);
}

Result<BinaryMapObjectRecords> parse_binary_map_object_records(
    std::span<const std::byte> bytes,
    std::size_t object_section_offset
)
{
    ByteReader reader(bytes);
    auto skipped = reader.read_bytes(object_section_offset);
    if (!skipped) {
        return Result<BinaryMapObjectRecords>::fail(skipped.error());
    }

    return parse_object_records_after_counts(reader, object_section_offset);
}

Result<BinaryMapObjectRecords> parse_binary_map_object_records(
    std::span<const std::byte> bytes,
    std::size_t object_section_offset,
    const BinaryMapHeader& header
)
{
    ByteReader reader(bytes);
    auto skipped = reader.read_bytes(object_section_offset);
    if (!skipped) {
        return Result<BinaryMapObjectRecords>::fail(skipped.error());
    }

    BinaryMapObjectRecords objects;
    auto total_count = read_i32(reader);
    if (!total_count) {
        return Result<BinaryMapObjectRecords>::fail(total_count.error());
    }
    if (total_count.value() < 0) {
        return Result<BinaryMapObjectRecords>::fail({"negative object count", object_section_offset});
    }
    objects.total_count = total_count.value();

    std::int64_t summed_counts = 0;
    for (int elevation = 0; elevation < binary_map_elevation_count; ++elevation) {
        objects.elevation_counts[elevation] = 0;
        if (!header.has_elevation(elevation)) {
            continue;
        }

        auto block_count = read_i32(reader);
        if (!block_count) {
            return Result<BinaryMapObjectRecords>::fail(block_count.error());
        }
        if (block_count.value() < 0) {
            return Result<BinaryMapObjectRecords>::fail({"negative elevation object count", reader.offset() - 4});
        }
        objects.elevation_counts[elevation] = block_count.value();
        summed_counts += block_count.value();
        if (summed_counts > objects.total_count) {
            return Result<BinaryMapObjectRecords>::fail({"object count mismatch", object_section_offset});
        }

        for (std::int32_t index = 0; index < block_count.value(); ++index) {
            auto record = parse_object_record(reader);
            if (!record) {
                return Result<BinaryMapObjectRecords>::fail(record.error());
            }
            objects.records.push_back(record.value());
        }
    }

    if (summed_counts != objects.total_count) {
        return Result<BinaryMapObjectRecords>::fail({"object count mismatch", object_section_offset});
    }

    objects.end_offset = reader.offset();
    return Result<BinaryMapObjectRecords>::ok(std::move(objects));
}

Result<BinaryMap> parse_binary_map(std::span<const std::byte> bytes)
{
    BinaryMap map;

    auto header = parse_binary_map_header(bytes);
    if (!header) {
        return Result<BinaryMap>::fail(header.error());
    }
    map.header = header.value();

    auto variables = parse_binary_map_variables(bytes, map.header);
    if (!variables) {
        return Result<BinaryMap>::fail(variables.error());
    }
    map.variables = std::move(variables.value());

    auto tiles = parse_binary_map_tiles(bytes, map.header);
    if (!tiles) {
        return Result<BinaryMap>::fail(tiles.error());
    }
    map.tiles = tiles.value();

    auto scripts = parse_binary_map_scripts(bytes, map.header);
    if (!scripts) {
        return Result<BinaryMap>::fail(scripts.error());
    }
    map.scripts = std::move(scripts.value());

    auto objects = parse_binary_map_object_records(bytes, map.scripts.end_offset, map.header);
    if (!objects) {
        return Result<BinaryMap>::fail(objects.error());
    }
    map.objects = std::move(objects.value());

    return Result<BinaryMap>::ok(std::move(map));
}

} // namespace qmap
