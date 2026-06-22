#pragma once

#include "qmap_result.h"

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

Result<BinaryMapHeader> parse_binary_map_header(std::span<const std::byte> bytes);
Result<BinaryMapVariables> parse_binary_map_variables(
    std::span<const std::byte> bytes,
    const BinaryMapHeader& header
);

} // namespace qmap
