#pragma once

#include "binary_map_patch_planner.h"
#include "qmap_result.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace qmap {

struct BinaryReplaceElevationWriteRequest {
    std::span<const std::byte> source_bytes;
    std::span<const std::byte> destination_bytes;
    BinaryReplaceElevationPlan plan;
    std::size_t destination_object_section_offset = 0;
    std::array<std::size_t, binary_script_type_count> destination_script_count_offsets{};
    const BinaryMapHeader* destination_header = nullptr;
    const BinaryMapScripts* source_scripts = nullptr;
    const BinaryMapScripts* destination_scripts = nullptr;
    const BinaryMapObjectRecords* source_objects = nullptr;
    const BinaryMapObjectRecords* destination_objects = nullptr;
};

struct BinaryI32Patch {
    std::size_t offset = 0;
    std::int32_t value = 0;
};

Result<std::vector<std::byte>> replace_binary_range(
    std::span<const std::byte> bytes,
    Range range,
    std::span<const std::byte> replacement
);

Result<std::vector<std::byte>> remove_binary_ranges(
    std::span<const std::byte> bytes,
    std::span<const Range> ranges
);

Result<std::size_t> adjust_binary_offset_after_removing_ranges(
    std::size_t offset,
    std::span<const Range> removed_ranges
);

Result<std::vector<std::byte>> insert_binary_ranges(
    std::span<const std::byte> bytes,
    std::size_t offset,
    std::span<const std::byte> source_bytes,
    std::span<const Range> source_ranges
);

Result<std::vector<std::byte>> copy_binary_ranges_with_i32_patches(
    std::span<const std::byte> source_bytes,
    std::span<const Range> source_ranges,
    std::span<const BinaryI32Patch> patches
);

Result<std::vector<std::byte>> patch_binary_i32_be(
    std::span<const std::byte> bytes,
    std::size_t offset,
    std::int32_t value
);

Result<std::vector<std::byte>> patch_binary_i32_be_all(
    std::span<const std::byte> bytes,
    std::span<const BinaryI32Patch> patches
);

Result<std::vector<BinaryI32Patch>> build_binary_replace_elevation_source_rewrite_patches(
    const BinaryReplaceElevationPlan& plan
);

Result<std::vector<std::byte>> patch_binary_replace_elevation_object_counts(
    std::span<const std::byte> bytes,
    std::size_t object_section_offset,
    const BinaryReplaceElevationPlan& plan
);

Result<std::vector<std::byte>> patch_binary_replace_elevation_script_counts(
    std::span<const std::byte> bytes,
    std::array<std::size_t, binary_script_type_count> script_count_offsets,
    const BinaryReplaceElevationPlan& plan
);

Result<std::vector<std::byte>> patch_binary_replace_elevation_tiles(
    std::span<const std::byte> source_bytes,
    std::span<const std::byte> destination_bytes,
    const BinaryReplaceElevationPlan& plan
);

Result<std::vector<std::byte>> patch_binary_replace_elevation_header_flags(
    std::span<const std::byte> destination_bytes,
    const BinaryReplaceElevationPlan& plan
);

Result<std::vector<std::byte>> write_binary_replace_elevation_patch(
    const BinaryReplaceElevationWriteRequest& request
);

} // namespace qmap
