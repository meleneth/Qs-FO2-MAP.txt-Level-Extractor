#include <catch2/catch_test_macros.hpp>

#include "binary_map_patch_planner.h"
#include "prototype_metadata.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace {

constexpr std::uint32_t elevation_one_tile_flag = 0x20000000u;
constexpr std::uint32_t elevation_two_tile_flag = 0x40000000u;
constexpr std::int32_t scenery_generic_subtype = 5;
constexpr std::int32_t scenery_stairs_subtype = 1;

void append_i32(std::vector<std::byte>& bytes, std::int32_t value)
{
    const auto unsigned_value = static_cast<std::uint32_t>(value);
    bytes.push_back(static_cast<std::byte>((unsigned_value >> 24) & 0xFF));
    bytes.push_back(static_cast<std::byte>((unsigned_value >> 16) & 0xFF));
    bytes.push_back(static_cast<std::byte>((unsigned_value >> 8) & 0xFF));
    bytes.push_back(static_cast<std::byte>(unsigned_value & 0xFF));
}

std::int32_t encoded_tile(int tile, int elevation)
{
    auto encoded = static_cast<std::uint32_t>(tile);
    if (elevation == 1) {
        encoded |= elevation_one_tile_flag;
    } else if (elevation == 2) {
        encoded |= elevation_two_tile_flag;
    }
    return static_cast<std::int32_t>(encoded);
}

qmap::BinaryObjectRecord object_record(
    std::int32_t obj_id,
    int elevation,
    std::int32_t pid,
    qmap::BinaryObjectType type,
    std::int32_t script_id = -1,
    qmap::Range raw = {}
)
{
    qmap::BinaryObjectRecord record;
    record.object_type = type;
    record.prefix.obj_id = obj_id;
    record.prefix.elevation = elevation;
    record.prefix.pid = pid;
    record.prefix.script_id = script_id;
    record.raw = raw;
    return record;
}

qmap::BinaryScriptRecord script_record(
    qmap::BinaryScriptType type,
    std::int32_t script_id,
    std::uint32_t obj_id = 0,
    qmap::Range raw = {}
)
{
    qmap::BinaryScriptRecord record;
    record.type = type;
    record.scr_id = script_id;
    record.scr_obj_id = obj_id;
    record.raw = raw;
    return record;
}

qmap::BinaryMap map_with_elevations(std::initializer_list<int> elevations)
{
    qmap::BinaryMap map;
    constexpr int absent_flags[] = {0x2, 0x4, 0x8};
    map.header.map_flags = 0xE;
    for (const auto elevation : elevations) {
        map.header.map_flags &= ~absent_flags[elevation];
    }
    return map;
}

qmap::PrototypeDatabase default_prototypes()
{
    qmap::PrototypeDatabase prototypes;
    prototypes.add(qmap::PrototypeRecord{
        0x02000001,
        qmap::BinaryObjectType::scenery,
        scenery_generic_subtype,
    });
    prototypes.add(qmap::PrototypeRecord{
        0x02000002,
        qmap::BinaryObjectType::scenery,
        scenery_generic_subtype,
    });
    return prototypes;
}

qmap::Result<qmap::BinaryReplaceElevationPlan> plan_replace(
    std::span<const std::byte> source_bytes,
    std::span<const std::byte> destination_bytes,
    const qmap::BinaryMap& source,
    const qmap::BinaryMap& destination,
    qmap::BinaryReplaceElevationRequest request
)
{
    auto prototypes = default_prototypes();
    return qmap::plan_binary_replace_elevation(
        source_bytes,
        destination_bytes,
        source,
        destination,
        prototypes,
        request
    );
}

qmap::Result<qmap::BinaryReplaceElevationPlan> plan_replace(
    std::span<const std::byte> source_bytes,
    const qmap::BinaryMap& source,
    const qmap::BinaryMap& destination,
    qmap::BinaryReplaceElevationRequest request
)
{
    return plan_replace(
        source_bytes,
        {},
        source,
        destination,
        request
    );
}

qmap::Result<qmap::BinaryReplaceElevationPlan> plan_replace(
    std::span<const std::byte> source_bytes,
    std::span<const std::byte> destination_bytes,
    const qmap::BinaryMap& source,
    const qmap::BinaryMap& destination,
    const qmap::PrototypeDatabase& prototypes,
    qmap::BinaryReplaceElevationRequest request
)
{
    return qmap::plan_binary_replace_elevation(
        source_bytes,
        destination_bytes,
        source,
        destination,
        prototypes,
        request
    );
}

qmap::Result<qmap::BinaryReplaceElevationPlan> plan_replace(
    std::span<const std::byte> source_bytes,
    const qmap::BinaryMap& source,
    const qmap::BinaryMap& destination,
    const qmap::PrototypeDatabase& prototypes,
    qmap::BinaryReplaceElevationRequest request
)
{
    return plan_replace(
        source_bytes,
        {},
        source,
        destination,
        prototypes,
        request
    );
}

} // namespace

TEST_CASE("plan_binary_replace_elevation summarizes whole elevation replacement", "[map][binary][patch]")
{
    std::vector<std::byte> source_bytes(400);
    std::vector<std::byte> destination_bytes(700);

    auto source = map_with_elevations({0});
    source.tiles.elevations[0] = std::span<const std::byte>(source_bytes).subspan(8, 8);
    source.objects.records.push_back(object_record(
        10,
        0,
        0x02000001,
        qmap::BinaryObjectType::scenery,
        0x03000010,
        qmap::Range{100, 20}
    ));
    source.objects.records.back().prefix.offsets.obj_id = 100;
    source.objects.records.back().prefix.offsets.elevation = 104;
    source.objects.records.back().prefix.offsets.script_id = 108;
    source.objects.records.back().inventory.push_back(object_record(
        11,
        -1,
        0x00000001,
        qmap::BinaryObjectType::item,
        -1,
        qmap::Range{120, 12}
    ));
    source.objects.records.back().inventory.back().prefix.offsets.obj_id = 120;
    source.objects.records.back().inventory.back().prefix.offsets.elevation = 124;
    source.objects.records.back().inventory.back().prefix.offsets.script_id = 128;
    auto spatial = script_record(qmap::BinaryScriptType::spatial, 0x01000005);
    spatial.raw = qmap::Range{200, 72};
    spatial.spatial_tile = encoded_tile(123, 0);
    spatial.offsets.scr_id = 200;
    spatial.offsets.spatial_tile = 208;
    spatial.offsets.scr_obj_id = 228;
    source.scripts.by_type[1].push_back(spatial);
    auto object_script = script_record(
        qmap::BinaryScriptType::object,
        0x03000010,
        10,
        qmap::Range{300, 64}
    );
    object_script.offsets.scr_id = 300;
    object_script.offsets.scr_obj_id = 320;
    source.scripts.by_type[3].push_back(object_script);

    auto destination = map_with_elevations({2});
    destination.objects.total_count = 3;
    destination.objects.elevation_counts = {2, 0, 1};
    destination.tiles.elevations[2] = std::span<const std::byte>(destination_bytes).subspan(12, 8);
    destination.objects.records.push_back(object_record(
        20,
        2,
        0x02000002,
        qmap::BinaryObjectType::scenery,
        0x03000020,
        qmap::Range{400, 28}
    ));
    auto destination_spatial = script_record(
        qmap::BinaryScriptType::spatial,
        0x01000020,
        0,
        qmap::Range{500, 72}
    );
    destination_spatial.spatial_tile = encoded_tile(456, 2);
    destination.scripts.by_type[1].push_back(destination_spatial);
    destination.scripts.by_type[3].push_back(script_record(
        qmap::BinaryScriptType::object,
        0x03000020,
        20,
        qmap::Range{600, 64}
    ));

    const auto planned = plan_replace(
        source_bytes,
        destination_bytes,
        source,
        destination,
        qmap::BinaryReplaceElevationRequest{0, 2}
    );

    REQUIRE(planned);
    CHECK(planned.value().source_elevation == 0);
    CHECK(planned.value().destination_elevation == 2);
    CHECK(planned.value().destination_was_present);
    CHECK(planned.value().source_tile_range.offset == 8);
    CHECK(planned.value().source_tile_range.size == 8);
    CHECK(planned.value().destination_tile_range.offset == 12);
    CHECK(planned.value().destination_tile_range.size == 8);
    CHECK(planned.value().source_tile_bytes == 8);
    CHECK(planned.value().destination_tile_bytes == 8);
    CHECK(planned.value().deleted_top_level_objects == 1);
    CHECK(planned.value().deleted_objects_including_inventory == 1);
    CHECK(planned.value().copied_top_level_objects == 1);
    CHECK(planned.value().copied_objects_including_inventory == 2);
    CHECK(planned.value().deleted_attached_scripts == 1);
    CHECK(planned.value().deleted_spatial_scripts == 1);
    CHECK(planned.value().copied_attached_scripts == 1);
    CHECK(planned.value().copied_spatial_scripts == 1);
    CHECK(planned.value().destination_total_objects_before == 3);
    CHECK(planned.value().destination_total_objects_after == 3);
    CHECK(planned.value().destination_object_counts_before[0] == 2);
    CHECK(planned.value().destination_object_counts_before[1] == 0);
    CHECK(planned.value().destination_object_counts_before[2] == 1);
    CHECK(planned.value().destination_object_counts_after[0] == 2);
    CHECK(planned.value().destination_object_counts_after[1] == 0);
    CHECK(planned.value().destination_object_counts_after[2] == 1);
    CHECK(planned.value().destination_script_counts_before[1] == 1);
    CHECK(planned.value().destination_script_counts_before[3] == 1);
    CHECK(planned.value().destination_script_counts_after[1] == 1);
    CHECK(planned.value().destination_script_counts_after[3] == 1);
    REQUIRE(planned.value().deleted_objects.size() == 1);
    CHECK(planned.value().deleted_objects[0].object_id == 20);
    CHECK(planned.value().deleted_objects[0].object_type == qmap::BinaryObjectType::scenery);
    CHECK(planned.value().deleted_objects[0].raw.offset == 400);
    CHECK(planned.value().deleted_objects[0].raw.size == 28);
    REQUIRE(planned.value().deleted_scripts.size() == 2);
    CHECK(planned.value().deleted_scripts[0].script_id == 0x01000020);
    CHECK(planned.value().deleted_scripts[0].script_type == qmap::BinaryScriptType::spatial);
    CHECK(planned.value().deleted_scripts[0].raw.offset == 500);
    CHECK(planned.value().deleted_scripts[0].raw.size == 72);
    CHECK(planned.value().deleted_scripts[1].script_id == 0x03000020);
    CHECK(planned.value().deleted_scripts[1].script_type == qmap::BinaryScriptType::object);
    CHECK(planned.value().deleted_scripts[1].raw.offset == 600);
    CHECK(planned.value().deleted_scripts[1].raw.size == 64);
    REQUIRE(planned.value().copied_objects.size() == 2);
    CHECK(planned.value().copied_objects[0].object_id == 10);
    CHECK(planned.value().copied_objects[0].elevation == 0);
    CHECK(planned.value().copied_objects[0].script_id == 0x03000010);
    CHECK(planned.value().copied_objects[0].object_type == qmap::BinaryObjectType::scenery);
    CHECK(planned.value().copied_objects[0].raw.offset == 100);
    CHECK(planned.value().copied_objects[0].raw.size == 20);
    CHECK(planned.value().copied_objects[0].offsets.obj_id == 100);
    CHECK(planned.value().copied_objects[0].offsets.elevation == 104);
    CHECK(planned.value().copied_objects[0].offsets.script_id == 108);
    CHECK(planned.value().copied_objects[1].object_id == 11);
    CHECK(planned.value().copied_objects[1].elevation == -1);
    CHECK(planned.value().copied_objects[1].script_id == -1);
    CHECK(planned.value().copied_objects[1].object_type == qmap::BinaryObjectType::item);
    CHECK(planned.value().copied_objects[1].raw.offset == 120);
    CHECK(planned.value().copied_objects[1].raw.size == 12);
    CHECK(planned.value().copied_objects[1].offsets.obj_id == 120);
    CHECK(planned.value().copied_objects[1].offsets.elevation == 124);
    CHECK(planned.value().copied_objects[1].offsets.script_id == 128);
    REQUIRE(planned.value().copied_scripts.size() == 2);
    CHECK(planned.value().copied_scripts[0].script_id == 0x01000005);
    CHECK(planned.value().copied_scripts[0].script_type == qmap::BinaryScriptType::spatial);
    CHECK(planned.value().copied_scripts[0].object_id == 0);
    CHECK(planned.value().copied_scripts[0].spatial_tile == encoded_tile(123, 0));
    CHECK(planned.value().copied_scripts[0].raw.offset == 200);
    CHECK(planned.value().copied_scripts[0].raw.size == 72);
    CHECK(planned.value().copied_scripts[0].offsets.scr_id == 200);
    REQUIRE(planned.value().copied_scripts[0].offsets.spatial_tile);
    CHECK(*planned.value().copied_scripts[0].offsets.spatial_tile == 208);
    CHECK(planned.value().copied_scripts[0].offsets.scr_obj_id == 228);
    CHECK(planned.value().copied_scripts[1].script_id == 0x03000010);
    CHECK(planned.value().copied_scripts[1].script_type == qmap::BinaryScriptType::object);
    CHECK(planned.value().copied_scripts[1].object_id == 10);
    CHECK(planned.value().copied_scripts[1].raw.offset == 300);
    CHECK(planned.value().copied_scripts[1].raw.size == 64);
    CHECK(planned.value().copied_scripts[1].offsets.scr_id == 300);
    CHECK_FALSE(planned.value().copied_scripts[1].offsets.spatial_tile);
    CHECK(planned.value().copied_scripts[1].offsets.scr_obj_id == 320);
    REQUIRE(planned.value().object_id_mappings.size() == 2);
    CHECK(planned.value().object_id_mappings[0].old_id == 10);
    CHECK(planned.value().object_id_mappings[0].new_id != 10);
    CHECK(planned.value().object_id_mappings[1].old_id == 11);
    REQUIRE(planned.value().script_id_mappings.size() == 2);
    CHECK(planned.value().script_id_mappings[0].old_id == 0x01000005);
    CHECK(planned.value().script_id_mappings[1].old_id == 0x03000010);
}

TEST_CASE("plan_binary_replace_elevation allows absent destination elevation", "[map][binary][patch]")
{
    auto source = map_with_elevations({0});
    auto destination = map_with_elevations({1});
    std::vector<std::byte> source_bytes;

    const auto planned = plan_replace(
        source_bytes,
        source,
        destination,
        qmap::BinaryReplaceElevationRequest{0, 2}
    );

    REQUIRE(planned);
    CHECK_FALSE(planned.value().destination_was_present);
}

TEST_CASE("plan_binary_replace_elevation rejects absent source elevation", "[map][binary][patch]")
{
    auto source = map_with_elevations({1});
    auto destination = map_with_elevations({2});
    std::vector<std::byte> source_bytes;

    const auto planned = plan_replace(
        source_bytes,
        source,
        destination,
        qmap::BinaryReplaceElevationRequest{0, 2}
    );

    REQUIRE_FALSE(planned);
    CHECK(planned.error().message == "source elevation is absent");
}

TEST_CASE("plan_binary_replace_elevation rejects source tile span outside source bytes", "[map][binary][patch]")
{
    std::vector<std::byte> source_bytes(8);
    std::vector<std::byte> unbacked_tile_bytes(8);

    auto source = map_with_elevations({0});
    source.tiles.elevations[0] = unbacked_tile_bytes;
    auto destination = map_with_elevations({2});

    const auto planned = plan_replace(
        source_bytes,
        {},
        source,
        destination,
        qmap::BinaryReplaceElevationRequest{0, 2}
    );

    REQUIRE_FALSE(planned);
    CHECK(planned.error().message == "source tile bytes are not backed by the source map buffer");
}

TEST_CASE("plan_binary_replace_elevation rejects copied object raw range outside source bytes", "[map][binary][patch]")
{
    std::vector<std::byte> source_bytes(64);

    auto source = map_with_elevations({0});
    source.tiles.elevations[0] = source_bytes;
    source.objects.records.push_back(object_record(
        10,
        0,
        0x00000001,
        qmap::BinaryObjectType::item,
        -1,
        qmap::Range{60, 8}
    ));
    auto destination = map_with_elevations({2});

    const auto planned = plan_replace(
        source_bytes,
        source,
        destination,
        qmap::BinaryReplaceElevationRequest{0, 2}
    );

    REQUIRE_FALSE(planned);
    CHECK(planned.error().message == "copied object raw range is outside the source map buffer");
    CHECK(planned.error().offset == 60);
}

TEST_CASE("plan_binary_replace_elevation rejects deleted script raw range outside destination bytes", "[map][binary][patch]")
{
    std::vector<std::byte> source_bytes(64);
    std::vector<std::byte> destination_bytes(64);

    auto source = map_with_elevations({0});
    source.tiles.elevations[0] = source_bytes;
    auto destination = map_with_elevations({2});
    auto destination_spatial = script_record(
        qmap::BinaryScriptType::spatial,
        0x01000020,
        0,
        qmap::Range{61, 4}
    );
    destination_spatial.spatial_tile = encoded_tile(456, 2);
    destination.scripts.by_type[1].push_back(destination_spatial);

    const auto planned = plan_replace(
        source_bytes,
        destination_bytes,
        source,
        destination,
        qmap::BinaryReplaceElevationRequest{0, 2}
    );

    REQUIRE_FALSE(planned);
    CHECK(planned.error().message == "deleted script raw range is outside the destination map buffer");
    CHECK(planned.error().offset == 61);
}

TEST_CASE("plan_binary_replace_elevation rejects invalid elevation arguments", "[map][binary][patch]")
{
    auto source = map_with_elevations({0});
    auto destination = map_with_elevations({2});
    std::vector<std::byte> source_bytes;

    const auto invalid_source = plan_replace(
        source_bytes,
        source,
        destination,
        qmap::BinaryReplaceElevationRequest{-1, 2}
    );
    REQUIRE_FALSE(invalid_source);
    CHECK(invalid_source.error().message == "invalid source elevation");

    const auto invalid_destination = plan_replace(
        source_bytes,
        source,
        destination,
        qmap::BinaryReplaceElevationRequest{0, 3}
    );
    REQUIRE_FALSE(invalid_destination);
    CHECK(invalid_destination.error().message == "invalid destination elevation");
}

TEST_CASE("plan_binary_replace_elevation rejects missing copied object scripts", "[map][binary][patch]")
{
    auto source = map_with_elevations({0});
    source.objects.records.push_back(object_record(
        10,
        0,
        0x02000001,
        qmap::BinaryObjectType::scenery,
        0x03000010
    ));
    auto destination = map_with_elevations({2});
    std::vector<std::byte> source_bytes;

    const auto planned = plan_replace(
        source_bytes,
        source,
        destination,
        qmap::BinaryReplaceElevationRequest{0, 2}
    );

    REQUIRE_FALSE(planned);
    CHECK(planned.error().message == "copied object 10 references missing script 50331664");
}

TEST_CASE("plan_binary_replace_elevation rejects copied scripts referencing outside objects", "[map][binary][patch]")
{
    auto source = map_with_elevations({0, 1});
    source.objects.records.push_back(object_record(
        10,
        0,
        0x02000001,
        qmap::BinaryObjectType::scenery,
        0x03000010
    ));
    source.objects.records.push_back(object_record(
        99,
        1,
        0x02000002,
        qmap::BinaryObjectType::scenery
    ));
    source.scripts.by_type[3].push_back(script_record(
        qmap::BinaryScriptType::object,
        0x03000010,
        99
    ));
    auto destination = map_with_elevations({2});
    std::vector<std::byte> source_bytes;

    const auto planned = plan_replace(
        source_bytes,
        source,
        destination,
        qmap::BinaryReplaceElevationRequest{0, 2}
    );

    REQUIRE_FALSE(planned);
    CHECK(planned.error().message == "copied script 50331664 references object 99 outside the copied elevation");
}

TEST_CASE("plan_binary_replace_elevation rejects spatial scripts with undecodable elevation", "[map][binary][patch]")
{
    auto source = map_with_elevations({0});
    auto spatial = script_record(qmap::BinaryScriptType::spatial, 0x01000005);
    spatial.spatial_tile = static_cast<std::int32_t>(0x6000007Bu);
    source.scripts.by_type[1].push_back(spatial);
    auto destination = map_with_elevations({2});
    std::vector<std::byte> source_bytes;

    const auto planned = plan_replace(
        source_bytes,
        source,
        destination,
        qmap::BinaryReplaceElevationRequest{0, 2}
    );

    REQUIRE_FALSE(planned);
    CHECK(planned.error().message == "spatial script has undecodable elevation in built_tile");
}

TEST_CASE("plan_binary_replace_elevation rejects copied scenery without prototype metadata", "[map][binary][patch]")
{
    auto source = map_with_elevations({0});
    source.objects.records.push_back(object_record(
        10,
        0,
        0x02000009,
        qmap::BinaryObjectType::scenery
    ));
    auto destination = map_with_elevations({2});
    const qmap::PrototypeDatabase prototypes;
    std::vector<std::byte> source_bytes;

    const auto planned = plan_replace(
        source_bytes,
        source,
        destination,
        prototypes,
        qmap::BinaryReplaceElevationRequest{0, 2}
    );

    REQUIRE_FALSE(planned);
    CHECK(planned.error().message == "prototype metadata missing for copied scenery PID 33554441");
}

TEST_CASE("plan_binary_replace_elevation rejects copied elevation-linking scenery", "[map][binary][patch]")
{
    auto source = map_with_elevations({0});
    source.objects.records.push_back(object_record(
        10,
        0,
        0x02000001,
        qmap::BinaryObjectType::scenery
    ));
    auto destination = map_with_elevations({2});
    auto prototypes = default_prototypes();
    prototypes.add(qmap::PrototypeRecord{
        0x02000001,
        qmap::BinaryObjectType::scenery,
        scenery_stairs_subtype,
    });
    std::vector<std::byte> source_bytes;

    const auto planned = plan_replace(
        source_bytes,
        source,
        destination,
        prototypes,
        qmap::BinaryReplaceElevationRequest{0, 2}
    );

    REQUIRE_FALSE(planned);
    CHECK(
        planned.error().message
        == "copied scenery PID 33554433 has an elevation-linking subtype that replace-elevation cannot rewrite yet"
    );
}

TEST_CASE("plan_binary_replace_elevation allows copied script local variables available in destination", "[map][binary][patch]")
{
    auto source = map_with_elevations({0});
    auto spatial = script_record(qmap::BinaryScriptType::spatial, 0x01000005);
    spatial.spatial_tile = encoded_tile(123, 0);
    spatial.lvar_offset = 1;
    spatial.lvar_count = 2;
    source.scripts.by_type[1].push_back(spatial);
    auto destination = map_with_elevations({2});
    destination.variables.local_vars = {10, 20, 30};
    std::vector<std::byte> source_bytes;

    const auto planned = plan_replace(
        source_bytes,
        source,
        destination,
        qmap::BinaryReplaceElevationRequest{0, 2}
    );

    REQUIRE(planned);
    CHECK(planned.value().copied_spatial_scripts == 1);
}

TEST_CASE("plan_binary_replace_elevation rejects copied script local variables outside destination", "[map][binary][patch]")
{
    auto source = map_with_elevations({0});
    auto spatial = script_record(qmap::BinaryScriptType::spatial, 0x01000005);
    spatial.spatial_tile = encoded_tile(123, 0);
    spatial.lvar_offset = 2;
    spatial.lvar_count = 2;
    source.scripts.by_type[1].push_back(spatial);
    auto destination = map_with_elevations({2});
    destination.variables.local_vars = {10, 20, 30};
    std::vector<std::byte> source_bytes;

    const auto planned = plan_replace(
        source_bytes,
        source,
        destination,
        qmap::BinaryReplaceElevationRequest{0, 2}
    );

    REQUIRE_FALSE(planned);
    CHECK(
        planned.error().message
        == "copied script 16777221 requires local variables outside the destination map's local variable range"
    );
}

TEST_CASE("plan_binary_replace_elevation rejects copied script negative local variable offset", "[map][binary][patch]")
{
    auto source = map_with_elevations({0});
    auto spatial = script_record(qmap::BinaryScriptType::spatial, 0x01000005);
    spatial.spatial_tile = encoded_tile(123, 0);
    spatial.lvar_offset = -1;
    spatial.lvar_count = 1;
    source.scripts.by_type[1].push_back(spatial);
    auto destination = map_with_elevations({2});
    destination.variables.local_vars = {10};
    std::vector<std::byte> source_bytes;

    const auto planned = plan_replace(
        source_bytes,
        source,
        destination,
        qmap::BinaryReplaceElevationRequest{0, 2}
    );

    REQUIRE_FALSE(planned);
    CHECK(planned.error().message == "copied script 16777221 has a negative local variable offset");
}

TEST_CASE("plan_binary_replace_elevation rejects exhausted object id space", "[map][binary][patch]")
{
    auto source = map_with_elevations({0});
    source.objects.records.push_back(object_record(
        10,
        0,
        0x00000001,
        qmap::BinaryObjectType::item
    ));
    auto destination = map_with_elevations({2});
    destination.objects.records.push_back(object_record(
        std::numeric_limits<std::int32_t>::max(),
        2,
        0x00000001,
        qmap::BinaryObjectType::item
    ));
    std::vector<std::byte> source_bytes;

    const auto planned = plan_replace(
        source_bytes,
        source,
        destination,
        qmap::BinaryReplaceElevationRequest{0, 2}
    );

    REQUIRE_FALSE(planned);
    CHECK(planned.error().message == "object ID space is exhausted");
}

TEST_CASE("plan_binary_replace_elevation rejects exhausted script id space", "[map][binary][patch]")
{
    auto source = map_with_elevations({0});
    auto spatial = script_record(qmap::BinaryScriptType::spatial, 0x01000005);
    spatial.spatial_tile = encoded_tile(123, 0);
    source.scripts.by_type[1].push_back(spatial);
    auto destination = map_with_elevations({2});
    destination.scripts.by_type[1].push_back(script_record(
        qmap::BinaryScriptType::spatial,
        0x01FFFFFF
    ));
    std::vector<std::byte> source_bytes;

    const auto planned = plan_replace(
        source_bytes,
        source,
        destination,
        qmap::BinaryReplaceElevationRequest{0, 2}
    );

    REQUIRE_FALSE(planned);
    CHECK(planned.error().message == "script ID space is exhausted for type 1");
}

TEST_CASE("plan_binary_replace_elevation reports copied exit grids", "[map][binary][patch]")
{
    std::vector<std::byte> source_bytes;
    append_i32(source_bytes, 9000);
    append_i32(source_bytes, 42);
    append_i32(source_bytes, 1234);
    append_i32(source_bytes, 1);
    append_i32(source_bytes, 3);

    auto source = map_with_elevations({0});
    auto exit_grid = object_record(
        10,
        0,
        0x05000010,
        qmap::BinaryObjectType::misc
    );
    exit_grid.tail = qmap::Range{0, source_bytes.size()};
    source.objects.records.push_back(exit_grid);
    auto destination = map_with_elevations({2});

    const auto planned = plan_replace(
        source_bytes,
        source,
        destination,
        qmap::BinaryReplaceElevationRequest{0, 2}
    );

    REQUIRE(planned);
    REQUIRE(planned.value().preserved_exit_grids.size() == 1);
    CHECK(planned.value().preserved_exit_grids[0].object_id == 10);
    CHECK(planned.value().preserved_exit_grids[0].dest_map == 42);
    CHECK(planned.value().preserved_exit_grids[0].dest_tile == 1234);
    CHECK(planned.value().preserved_exit_grids[0].dest_elevation == 1);
    CHECK(planned.value().preserved_exit_grids[0].dest_rotation == 3);
}

TEST_CASE("plan_binary_replace_elevation rejects undecodable exit-grid tails", "[map][binary][patch]")
{
    std::vector<std::byte> source_bytes;
    append_i32(source_bytes, 9000);

    auto source = map_with_elevations({0});
    auto exit_grid = object_record(
        10,
        0,
        0x05000010,
        qmap::BinaryObjectType::misc
    );
    exit_grid.tail = qmap::Range{0, source_bytes.size() + 4};
    source.objects.records.push_back(exit_grid);
    auto destination = map_with_elevations({2});

    const auto planned = plan_replace(
        source_bytes,
        source,
        destination,
        qmap::BinaryReplaceElevationRequest{0, 2}
    );

    REQUIRE_FALSE(planned);
    CHECK(planned.error().message == "invalid misc tail range");
}
