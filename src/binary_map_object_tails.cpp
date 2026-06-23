#include "binary_map_parser.h"

#include "byte_reader.h"

#include <cstdint>
#include <optional>

namespace qmap {
namespace {

constexpr std::size_t critter_tail_words = 10;
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
    case BinaryObjectType::item:
    case BinaryObjectType::scenery:
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
    auto reaction = read_i32(reader);
    if (!reaction) {
        return Result<BinaryCritterTail>::fail(reaction.error());
    }
    parsed.reaction = reaction.value();
    auto current_mp = read_i32(reader);
    if (!current_mp) {
        return Result<BinaryCritterTail>::fail(current_mp.error());
    }
    parsed.current_mp = current_mp.value();
    auto combat_results = read_i32(reader);
    if (!combat_results) {
        return Result<BinaryCritterTail>::fail(combat_results.error());
    }
    parsed.combat_results = combat_results.value();
    auto damage_last_turn = read_i32(reader);
    if (!damage_last_turn) {
        return Result<BinaryCritterTail>::fail(damage_last_turn.error());
    }
    parsed.damage_last_turn = damage_last_turn.value();
    auto ai_packet = read_i32(reader);
    if (!ai_packet) {
        return Result<BinaryCritterTail>::fail(ai_packet.error());
    }
    parsed.ai_packet = ai_packet.value();
    auto group_id = read_i32(reader);
    if (!group_id) {
        return Result<BinaryCritterTail>::fail(group_id.error());
    }
    parsed.group_id = group_id.value();
    auto who_hit_me = read_i32(reader);
    if (!who_hit_me) {
        return Result<BinaryCritterTail>::fail(who_hit_me.error());
    }
    parsed.who_hit_me = who_hit_me.value();
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

} // namespace qmap
