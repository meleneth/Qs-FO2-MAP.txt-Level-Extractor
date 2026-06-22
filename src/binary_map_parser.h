#pragma once

#include "qmap_result.h"
#include "qmap_types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace qmap {

constexpr std::size_t binary_map_filename_size = 16;
constexpr std::size_t binary_map_unknown_header_words = 44;
constexpr std::size_t binary_map_header_size = 236;

struct BinaryMapHeader {
    std::uint32_t version = 0;
    std::array<char, binary_map_filename_size> filename{};
    std::int32_t dude_start = 0;
    std::int32_t elev_start = 0;
    std::int32_t face_start = 0;
    std::int32_t lvar_count = 0;
    std::int32_t map_script_id = 0;
    std::int32_t map_flags = 0;
    std::int32_t light_level = 0;
    std::int32_t mvar_count = 0;
    std::int32_t map_id = 0;
    std::uint32_t game_ticks = 0;
    std::array<std::int32_t, binary_map_unknown_header_words> unknown{};

    std::string filename_string() const;
    bool has_elevation(int elevation) const;
};

struct BinaryMapVariables {
    std::vector<std::int32_t> map_vars;
    std::vector<std::int32_t> local_vars;
};

struct BinaryMapTiles {
    std::array<std::span<const std::byte>, 3> elevations{};
};

enum class BinaryScriptType : int {
    system = 0,
    spatial = 1,
    timed = 2,
    object = 3,
    critter = 4,
};

constexpr int binary_script_type_count = 5;

struct BinaryScriptRecord {
    Range raw;
    BinaryScriptType type = BinaryScriptType::system;
    std::int32_t scr_id = 0;
    std::int32_t scr_next = 0;
    std::int32_t spatial_tile = 0;
    std::int32_t spatial_radius = 0;
    std::int32_t time = 0;
    std::int32_t scr_flags = 0;
    std::int32_t scr_index = 0;
    std::int32_t program_ptr = 0;
    std::uint32_t scr_obj_id = 0;
    std::int32_t lvar_offset = 0;
    std::int32_t lvar_count = 0;
    std::int32_t last_used_value = 0;
    std::int32_t current_action = 0;
    std::int32_t fixed_param = 0;
    std::int32_t action_id = 0;
    std::int32_t override_flags = 0;
    std::int32_t unknown_1 = 0;
    std::int32_t how_much = 0;
    std::int32_t unknown_2 = 0;
};

struct BinaryMapScripts {
    std::array<std::vector<BinaryScriptRecord>, binary_script_type_count> by_type;
    std::size_t end_offset = 0;
};

struct BinaryObjectPrefix {
    Range raw;
    std::int32_t obj_id = 0;
    std::int32_t tile = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t screen_x = 0;
    std::int32_t screen_y = 0;
    std::int32_t frame = 0;
    std::int32_t rotation = 0;
    std::int32_t fid = 0;
    std::int32_t flags = 0;
    std::int32_t elevation = 0;
    std::int32_t pid = 0;
    std::int32_t cid = 0;
    std::int32_t light_radius = 0;
    std::int32_t light_intensity = 0;
    std::int32_t outline_color = 0;
    std::int32_t script_id = 0;
    std::int32_t script_index = 0;
    std::int32_t inventory_count = 0;
    std::int32_t inventory_size = 0;

    int pid_type() const;
};

enum class BinaryObjectType : int {
    item = 0,
    critter = 1,
    scenery = 2,
    wall = 3,
    tile = 4,
    misc = 5,
    interface_object = 6,
    inventory = 7,
    head = 8,
    background = 9,
};

std::optional<BinaryObjectType> binary_object_type_from_pid(std::int32_t pid);

struct BinaryMapObjectPrefixes {
    std::int32_t total_count = 0;
    std::array<std::int32_t, 3> elevation_counts{};
    std::vector<BinaryObjectPrefix> records;
    std::size_t end_offset = 0;
};

struct BinaryMapObjectCounts {
    std::int32_t total_count = 0;
    std::array<std::int32_t, 3> elevation_counts{};
    int first_counted_elevation = -1;
    std::size_t data_offset = 0;
};

struct BinaryObjectBlockHeader {
    std::int32_t total_count = 0;
    int elevation = -1;
    std::int32_t block_count = 0;
    std::size_t objects_offset = 0;
};

struct BinaryObjectRecord {
    BinaryObjectPrefix prefix;
    Range raw;
    Range tail;
};

struct BinaryMapObjectRecords {
    std::int32_t total_count = 0;
    std::array<std::int32_t, 3> elevation_counts{};
    std::vector<BinaryObjectRecord> records;
    std::size_t end_offset = 0;
};

struct BinaryMap {
    BinaryMapHeader header;
    BinaryMapVariables variables;
    BinaryMapTiles tiles;
    BinaryMapScripts scripts;
    BinaryMapObjectRecords objects;
};

Result<BinaryMapHeader> parse_binary_map_header(std::span<const std::byte> bytes);
Result<BinaryMapVariables> parse_binary_map_variables(
    std::span<const std::byte> bytes,
    const BinaryMapHeader& header
);
Result<BinaryMapTiles> parse_binary_map_tiles(
    std::span<const std::byte> bytes,
    const BinaryMapHeader& header
);
Result<BinaryMapScripts> parse_binary_map_scripts(
    std::span<const std::byte> bytes,
    const BinaryMapHeader& header
);
Result<BinaryMapObjectPrefixes> parse_binary_map_object_prefixes(
    std::span<const std::byte> bytes,
    std::size_t object_section_offset
);
Result<BinaryMapObjectCounts> parse_binary_map_object_counts(
    std::span<const std::byte> bytes,
    std::size_t object_section_offset,
    const BinaryMapHeader& header
);
Result<BinaryObjectBlockHeader> parse_first_binary_object_block_header(
    std::span<const std::byte> bytes,
    std::size_t object_section_offset,
    const BinaryMapHeader& header
);
Result<std::optional<BinaryObjectPrefix>> parse_first_binary_object_prefix(
    std::span<const std::byte> bytes,
    std::size_t object_section_offset,
    const BinaryMapHeader& header
);
Result<BinaryMapObjectRecords> parse_binary_map_object_records(
    std::span<const std::byte> bytes,
    std::size_t object_section_offset
);
Result<BinaryMapObjectRecords> parse_binary_map_object_records(
    std::span<const std::byte> bytes,
    std::size_t object_section_offset,
    const BinaryMapHeader& header
);
Result<BinaryMap> parse_binary_map(std::span<const std::byte> bytes);

} // namespace qmap
