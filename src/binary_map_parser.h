#pragma once

#include "qmap_result.h"
#include "qmap_types.h"

#include <array>
#include <cstddef>
#include <cstdint>
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

} // namespace qmap
