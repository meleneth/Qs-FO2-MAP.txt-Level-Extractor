#pragma once

#include "binary_map_parser.h"
#include "qmap_result.h"

#include <cstddef>
#include <cstdint>
#include <array>
#include <span>
#include <vector>

namespace qmap {

class PrototypeDatabase;

struct BinaryReplaceElevationRequest {
    int source_elevation = 0;
    int destination_elevation = 0;
};

struct BinaryIdMapping {
    std::int32_t old_id = 0;
    std::int32_t new_id = 0;
};

struct BinaryPlannedObjectCopy {
    std::int32_t object_id = 0;
    std::int32_t elevation = 0;
    std::int32_t script_id = 0;
    BinaryObjectType object_type = BinaryObjectType::item;
    Range raw;
    BinaryObjectPrefix::FieldOffsets offsets;
};

struct BinaryPlannedScriptCopy {
    std::int32_t script_id = 0;
    BinaryScriptType script_type = BinaryScriptType::system;
    std::uint32_t object_id = 0;
    std::int32_t spatial_tile = 0;
    Range raw;
    BinaryScriptRecord::FieldOffsets offsets;
};

struct BinaryPlannedObjectDeletion {
    std::int32_t object_id = 0;
    BinaryObjectType object_type = BinaryObjectType::item;
    Range raw;
};

struct BinaryPlannedScriptDeletion {
    std::int32_t script_id = 0;
    BinaryScriptType script_type = BinaryScriptType::system;
    Range raw;
};

struct BinaryExitGridLink {
    std::int32_t object_id = 0;
    std::int32_t dest_map = 0;
    std::int32_t dest_tile = 0;
    std::int32_t dest_elevation = 0;
    std::int32_t dest_rotation = 0;
};

struct BinaryReplaceElevationPlan {
    int source_elevation = 0;
    int destination_elevation = 0;
    bool destination_was_present = false;
    Range source_tile_range;
    Range destination_tile_range;
    std::size_t source_tile_bytes = 0;
    std::size_t destination_tile_bytes = 0;
    std::size_t deleted_top_level_objects = 0;
    std::size_t deleted_objects_including_inventory = 0;
    std::size_t copied_top_level_objects = 0;
    std::size_t copied_objects_including_inventory = 0;
    std::size_t deleted_spatial_scripts = 0;
    std::size_t copied_spatial_scripts = 0;
    std::size_t deleted_attached_scripts = 0;
    std::size_t copied_attached_scripts = 0;
    std::int32_t destination_total_objects_before = 0;
    std::int32_t destination_total_objects_after = 0;
    std::array<std::int32_t, binary_map_elevation_count> destination_object_counts_before{};
    std::array<std::int32_t, binary_map_elevation_count> destination_object_counts_after{};
    std::array<std::size_t, binary_script_type_count> destination_script_counts_before{};
    std::array<std::size_t, binary_script_type_count> destination_script_counts_after{};
    std::vector<BinaryPlannedObjectDeletion> deleted_objects;
    std::vector<BinaryPlannedScriptDeletion> deleted_scripts;
    std::vector<BinaryPlannedObjectCopy> copied_objects;
    std::vector<BinaryPlannedScriptCopy> copied_scripts;
    std::vector<BinaryIdMapping> object_id_mappings;
    std::vector<BinaryIdMapping> script_id_mappings;
    std::vector<BinaryExitGridLink> preserved_exit_grids;
};

Result<BinaryReplaceElevationPlan> plan_binary_replace_elevation(
    std::span<const std::byte> source_bytes,
    std::span<const std::byte> destination_bytes,
    const BinaryMap& source,
    const BinaryMap& destination,
    const PrototypeDatabase& prototypes,
    BinaryReplaceElevationRequest request
);

} // namespace qmap
