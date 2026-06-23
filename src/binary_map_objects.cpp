#include "binary_map_parser.h"

#include "byte_reader.h"

#include <cstdint>
#include <optional>
#include <string>

namespace qmap {
namespace {

constexpr std::size_t critter_tail_words = 11;
constexpr std::size_t scenery_tail_words = 3;
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
    auto tail_bytes = reader.read_bytes(modeled_binary_object_tail_size(*object_type));
    if (!tail_bytes) {
        return Result<BinaryObjectRecord>::fail(tail_bytes.error());
    }

    BinaryObjectRecord record;
    record.prefix = prefix.value();
    record.object_type = *object_type;
    record.tail = Range{tail_start, tail_bytes.value().size()};

    if (record.prefix.inventory_count < 0) {
        return Result<BinaryObjectRecord>::fail({"negative inventory object count", record.prefix.raw.offset + 0x48});
    }

    // MAP object inventory entries are stored immediately after the owner:
    // a 4-byte quantity followed by another full map object record.
    for (std::int32_t index = 0; index < record.prefix.inventory_count; ++index) {
        auto quantity = read_i32(reader);
        if (!quantity) {
            return Result<BinaryObjectRecord>::fail(quantity.error());
        }

        auto inventory_object = parse_object_record(reader);
        if (!inventory_object) {
            return Result<BinaryObjectRecord>::fail(inventory_object.error());
        }

        record.inventory_quantities.push_back(quantity.value());
        record.inventory.push_back(std::move(inventory_object.value()));
    }

    record.raw = Range{record_start, reader.offset() - record_start};
    return Result<BinaryObjectRecord>::ok(record);
}

Error object_record_error_context(const Error& error, std::optional<int> elevation, std::int32_t object_index)
{
    std::string message;
    if (elevation) {
        message = "elevation " + std::to_string(*elevation) + " object ";
    } else {
        message = "object ";
    }
    message += std::to_string(object_index) + ": " + error.message;
    return Error{message, error.offset};
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
            return Result<BinaryMapObjectRecords>::fail(
                object_record_error_context(record.error(), std::nullopt, index)
            );
        }
        objects.records.push_back(record.value());
    }
    objects.end_offset = reader.offset();

    return Result<BinaryMapObjectRecords>::ok(std::move(objects));
}

} // namespace

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

std::size_t modeled_binary_object_tail_size(BinaryObjectType type)
{
    // Sizes the parser currently consumes from MAP object records. Subtype
    // specific item/scenery details still need prototype-level classification.
    switch (type) {
    case BinaryObjectType::critter:
        return critter_tail_words * sizeof(std::int32_t);
    case BinaryObjectType::scenery:
        return scenery_tail_words * sizeof(std::int32_t);
    case BinaryObjectType::item:
    case BinaryObjectType::wall:
    case BinaryObjectType::tile:
    case BinaryObjectType::interface_object:
    case BinaryObjectType::inventory:
    case BinaryObjectType::head:
    case BinaryObjectType::background:
    case BinaryObjectType::misc:
        return 0;
    }
    return 0;
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
    if (tail.size >= 2 * sizeof(std::int32_t)) {
        auto dest_map = read_i32(reader);
        if (!dest_map) {
            return Result<BinaryMiscTail>::fail(dest_map.error());
        }
        parsed.dest_map = dest_map.value();
    }
    if (tail.size >= 3 * sizeof(std::int32_t)) {
        auto dest_tile = read_i32(reader);
        if (!dest_tile) {
            return Result<BinaryMiscTail>::fail(dest_tile.error());
        }
        parsed.dest_tile = dest_tile.value();
    }
    if (tail.size >= 4 * sizeof(std::int32_t)) {
        auto dest_elevation = read_i32(reader);
        if (!dest_elevation) {
            return Result<BinaryMiscTail>::fail(dest_elevation.error());
        }
        parsed.dest_elevation = dest_elevation.value();
    }
    if (tail.size >= 5 * sizeof(std::int32_t)) {
        auto dest_rotation = read_i32(reader);
        if (!dest_rotation) {
            return Result<BinaryMiscTail>::fail(dest_rotation.error());
        }
        parsed.dest_rotation = dest_rotation.value();
    }
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
                return Result<BinaryMapObjectRecords>::fail(
                    object_record_error_context(record.error(), elevation, index)
                );
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

} // namespace qmap
