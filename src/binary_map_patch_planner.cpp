#include "binary_map_patch_planner.h"

#include "prototype_metadata.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace qmap {
namespace {

constexpr std::int32_t no_script_id = -1;
constexpr std::uint32_t elevation_one_tile_flag = 0x20000000u;
constexpr std::uint32_t elevation_two_tile_flag = 0x40000000u;
constexpr std::uint32_t tile_elevation_mask = elevation_one_tile_flag | elevation_two_tile_flag;
constexpr int script_type_shift = 24;
constexpr std::int32_t min_object_id = 1;
constexpr std::int32_t max_object_id = std::numeric_limits<std::int32_t>::max();
constexpr std::int32_t script_id_type_mask = 0xFF000000;
constexpr std::int32_t script_id_index_mask = 0x00FFFFFF;

Result<Range> range_from_backing_bytes(
    std::span<const std::byte> backing,
    std::span<const std::byte> view,
    std::string error_message
)
{
    if (view.empty()) {
        return Result<Range>::ok({});
    }
    if (backing.empty()) {
        return Result<Range>::fail({std::move(error_message), 0});
    }

    const auto backing_begin = reinterpret_cast<std::uintptr_t>(backing.data());
    const auto backing_end = backing_begin + backing.size();
    const auto view_begin = reinterpret_cast<std::uintptr_t>(view.data());
    const auto view_end = view_begin + view.size();
    if (view_begin < backing_begin || view_end > backing_end || view_end < view_begin) {
        return Result<Range>::fail({std::move(error_message), 0});
    }

    return Result<Range>::ok(Range{view_begin - backing_begin, view.size()});
}

Result<void> validate_range_in_bytes(
    std::span<const std::byte> bytes,
    Range range,
    std::string error_message
)
{
    if (range.offset > bytes.size() || range.size > bytes.size() - range.offset) {
        return Result<void>::fail({std::move(error_message), range.offset});
    }
    return Result<void>::ok();
}

std::optional<int> elevation_from_encoded_tile(std::int32_t tile)
{
    const auto encoded = static_cast<std::uint32_t>(tile);
    if ((encoded & tile_elevation_mask) == 0) {
        return 0;
    }
    if ((encoded & tile_elevation_mask) == tile_elevation_mask) {
        return std::nullopt;
    }
    if (encoded & elevation_one_tile_flag) {
        return 1;
    }
    if (encoded & elevation_two_tile_flag) {
        return 2;
    }
    return std::nullopt;
}

int script_type_id(BinaryScriptType type)
{
    return static_cast<int>(type);
}

std::int32_t first_script_id_for_type(BinaryScriptType type)
{
    return static_cast<std::int32_t>(script_type_id(type) << script_type_shift);
}

void collect_object_ids(
    const std::vector<BinaryObjectRecord>& records,
    std::unordered_set<std::int32_t>& ids
)
{
    for (const auto& record : records) {
        ids.insert(record.prefix.obj_id);
        collect_object_ids(record.inventory, ids);
    }
}

void collect_script_ids(
    const BinaryMapScripts& scripts,
    std::unordered_set<std::int32_t>& ids
)
{
    for (const auto& records : scripts.by_type) {
        for (const auto& script : records) {
            ids.insert(script.scr_id);
        }
    }
}

std::array<std::size_t, binary_script_type_count> script_counts_by_type(
    const BinaryMapScripts& scripts
)
{
    std::array<std::size_t, binary_script_type_count> counts{};
    for (std::size_t type = 0; type < counts.size(); ++type) {
        counts[type] = scripts.by_type[type].size();
    }
    return counts;
}

std::size_t count_objects_including_inventory(const std::vector<BinaryObjectRecord>& records)
{
    std::size_t count = 0;
    for (const auto& record : records) {
        ++count;
        count += count_objects_including_inventory(record.inventory);
    }
    return count;
}

void collect_objects_including_inventory(
    const std::vector<BinaryObjectRecord>& records,
    std::vector<const BinaryObjectRecord*>& output
)
{
    for (const auto& record : records) {
        output.push_back(&record);
        collect_objects_including_inventory(record.inventory, output);
    }
}

std::vector<const BinaryObjectRecord*> top_level_objects_on_elevation(
    const BinaryMap& map,
    int elevation
)
{
    std::vector<const BinaryObjectRecord*> selected;
    for (const auto& record : map.objects.records) {
        if (record.prefix.elevation == elevation) {
            selected.push_back(&record);
        }
    }
    return selected;
}

std::vector<const BinaryObjectRecord*> flatten_selected_objects(
    const std::vector<const BinaryObjectRecord*>& top_level
)
{
    std::vector<const BinaryObjectRecord*> selected;
    for (const auto* record : top_level) {
        selected.push_back(record);
        collect_objects_including_inventory(record->inventory, selected);
    }
    return selected;
}

Result<std::array<std::size_t, binary_script_type_count>> planned_script_counts_after(
    std::array<std::size_t, binary_script_type_count> before,
    const std::vector<const BinaryScriptRecord*>& deleted_scripts,
    const std::vector<const BinaryScriptRecord*>& copied_scripts
)
{
    auto after = before;
    for (const auto* script : deleted_scripts) {
        const auto type = script_type_id(script->type);
        if (after[type] == 0) {
            return Result<std::array<std::size_t, binary_script_type_count>>::fail({
                "planned script deletion would make destination script counts negative",
                script->raw.offset,
            });
        }
        --after[type];
    }
    for (const auto* script : copied_scripts) {
        ++after[script_type_id(script->type)];
    }
    return Result<std::array<std::size_t, binary_script_type_count>>::ok(after);
}

Result<std::int32_t> reserve_next_object_id(std::unordered_set<std::int32_t>& used_ids)
{
    std::int32_t highest = 0;
    for (const auto id : used_ids) {
        if (id > highest) {
            highest = id;
        }
    }
    if (highest >= max_object_id) {
        return Result<std::int32_t>::fail({"object ID space is exhausted", 0});
    }

    const auto candidate = std::max(min_object_id, highest + 1);
    used_ids.insert(candidate);
    return Result<std::int32_t>::ok(candidate);
}

Result<std::int32_t> reserve_next_script_id(
    std::unordered_set<std::int32_t>& used_ids,
    BinaryScriptType type
)
{
    const auto first = first_script_id_for_type(type);
    const auto last = first | script_id_index_mask;
    std::int32_t highest = first - 1;
    for (const auto id : used_ids) {
        if ((id & script_id_type_mask) == first && id > highest) {
            highest = id;
        }
    }
    if (highest >= last) {
        return Result<std::int32_t>::fail({
            std::string{"script ID space is exhausted for type "}
                + std::to_string(script_type_id(type)),
            0,
        });
    }

    const auto candidate = highest + 1;
    used_ids.insert(candidate);
    return Result<std::int32_t>::ok(candidate);
}

std::unordered_map<std::int32_t, const BinaryScriptRecord*> scripts_by_id(
    const BinaryMapScripts& scripts
)
{
    std::unordered_map<std::int32_t, const BinaryScriptRecord*> by_id;
    for (const auto& records : scripts.by_type) {
        for (const auto& script : records) {
            by_id[script.scr_id] = &script;
        }
    }
    return by_id;
}

std::vector<const BinaryScriptRecord*> spatial_scripts_on_elevation(
    const BinaryMap& map,
    int elevation
)
{
    std::vector<const BinaryScriptRecord*> selected;
    for (const auto& script : map.scripts.by_type[script_type_id(BinaryScriptType::spatial)]) {
        const auto script_elevation = elevation_from_encoded_tile(script.spatial_tile);
        if (script_elevation && *script_elevation == elevation) {
            selected.push_back(&script);
        }
    }
    return selected;
}

Result<std::vector<const BinaryScriptRecord*>> checked_spatial_scripts_on_elevation(
    const BinaryMap& map,
    int elevation
)
{
    std::vector<const BinaryScriptRecord*> selected;
    for (const auto& script : map.scripts.by_type[script_type_id(BinaryScriptType::spatial)]) {
        const auto script_elevation = elevation_from_encoded_tile(script.spatial_tile);
        if (!script_elevation) {
            return Result<std::vector<const BinaryScriptRecord*>>::fail({
                "spatial script has undecodable elevation in built_tile",
                script.raw.offset,
            });
        }
        if (*script_elevation == elevation) {
            selected.push_back(&script);
        }
    }
    return Result<std::vector<const BinaryScriptRecord*>>::ok(std::move(selected));
}

std::size_t count_spatial_scripts_on_elevation(const BinaryMap& map, int elevation)
{
    return spatial_scripts_on_elevation(map, elevation).size();
}

std::vector<const BinaryScriptRecord*> attached_scripts_for_objects(
    const BinaryMap& map,
    const std::vector<const BinaryObjectRecord*>& objects
)
{
    std::unordered_set<std::int32_t> object_ids;
    for (const auto* object : objects) {
        object_ids.insert(object->prefix.obj_id);
    }

    std::vector<const BinaryScriptRecord*> selected;
    for (const auto type : {BinaryScriptType::object, BinaryScriptType::critter}) {
        for (const auto& script : map.scripts.by_type[script_type_id(type)]) {
            if (object_ids.contains(static_cast<std::int32_t>(script.scr_obj_id))) {
                selected.push_back(&script);
            }
        }
    }
    return selected;
}

bool is_attached_script_type(BinaryObjectType type)
{
    return type == BinaryObjectType::critter || type == BinaryObjectType::scenery
        || type == BinaryObjectType::item || type == BinaryObjectType::misc
        || type == BinaryObjectType::wall;
}

Result<void> collect_attached_scripts_for_copied_objects(
    const BinaryMap& source,
    const std::vector<const BinaryObjectRecord*>& copied_objects,
    std::vector<const BinaryScriptRecord*>& copied_scripts
)
{
    const auto by_id = scripts_by_id(source.scripts);
    std::unordered_set<std::int32_t> copied_object_ids;
    for (const auto* object : copied_objects) {
        copied_object_ids.insert(object->prefix.obj_id);
    }

    std::unordered_set<std::int32_t> copied_script_ids;
    for (const auto* object : copied_objects) {
        if (object->prefix.script_id == no_script_id || object->prefix.script_id == 0) {
            continue;
        }
        const auto found = by_id.find(object->prefix.script_id);
        if (found == by_id.end()) {
            return Result<void>::fail({
                std::string{"copied object "}
                    + std::to_string(object->prefix.obj_id)
                    + " references missing script "
                    + std::to_string(object->prefix.script_id),
                object->prefix.raw.offset,
            });
        }

        const auto& script = *found->second;
        if (is_attached_script_type(object->object_type)
            && !copied_object_ids.contains(static_cast<std::int32_t>(script.scr_obj_id))) {
            return Result<void>::fail({
                std::string{"copied script "}
                    + std::to_string(script.scr_id)
                    + " references object "
                    + std::to_string(script.scr_obj_id)
                    + " outside the copied elevation",
                script.raw.offset,
            });
        }

        if (copied_script_ids.insert(script.scr_id).second) {
            copied_scripts.push_back(&script);
        }
    }

    return Result<void>::ok();
}

Result<void> reject_unavailable_local_variables(
    const BinaryMap& destination,
    const std::vector<const BinaryScriptRecord*>& copied_scripts
)
{
    const auto destination_local_var_count = destination.variables.local_vars.size();
    for (const auto* script : copied_scripts) {
        if (script->lvar_count <= 0) {
            continue;
        }
        if (script->lvar_offset < 0) {
            return Result<void>::fail({
                std::string{"copied script "}
                    + std::to_string(script->scr_id)
                    + " has a negative local variable offset",
                script->raw.offset,
            });
        }

        const auto offset = static_cast<std::size_t>(script->lvar_offset);
        const auto count = static_cast<std::size_t>(script->lvar_count);
        if (offset > destination_local_var_count
            || count > destination_local_var_count - offset) {
            return Result<void>::fail({
                std::string{"copied script "}
                    + std::to_string(script->scr_id)
                    + " requires local variables outside the destination map's local variable range",
                script->raw.offset,
            });
        }
    }

    return Result<void>::ok();
}

Result<void> add_object_id_mappings(
    const std::vector<const BinaryObjectRecord*>& copied_objects,
    std::unordered_set<std::int32_t>& used_object_ids,
    BinaryReplaceElevationPlan& plan
)
{
    for (const auto* object : copied_objects) {
        auto reserved = reserve_next_object_id(used_object_ids);
        if (!reserved) {
            return Result<void>::fail({
                reserved.error().message,
                object->prefix.raw.offset,
            });
        }
        plan.object_id_mappings.push_back({
            object->prefix.obj_id,
            reserved.value(),
        });
    }
    return Result<void>::ok();
}

Result<void> add_script_id_mappings(
    const std::vector<const BinaryScriptRecord*>& copied_scripts,
    std::unordered_set<std::int32_t>& used_script_ids,
    BinaryReplaceElevationPlan& plan
)
{
    for (const auto* script : copied_scripts) {
        auto reserved = reserve_next_script_id(used_script_ids, script->type);
        if (!reserved) {
            return Result<void>::fail({
                reserved.error().message,
                script->raw.offset,
            });
        }
        plan.script_id_mappings.push_back({
            script->scr_id,
            reserved.value(),
        });
    }
    return Result<void>::ok();
}

Result<void> add_copied_object_records(
    std::span<const std::byte> source_bytes,
    const std::vector<const BinaryObjectRecord*>& copied_objects,
    BinaryReplaceElevationPlan& plan
)
{
    plan.copied_objects.reserve(copied_objects.size());
    for (const auto* object : copied_objects) {
        auto valid_range = validate_range_in_bytes(
            source_bytes,
            object->raw,
            "copied object raw range is outside the source map buffer"
        );
        if (!valid_range) {
            return Result<void>::fail(valid_range.error());
        }
        plan.copied_objects.push_back({
            object->prefix.obj_id,
            object->prefix.elevation,
            object->prefix.script_id,
            object->object_type,
            object->raw,
            object->prefix.offsets,
        });
    }
    return Result<void>::ok();
}

Result<void> add_copied_script_records(
    std::span<const std::byte> source_bytes,
    const std::vector<const BinaryScriptRecord*>& copied_scripts,
    BinaryReplaceElevationPlan& plan
)
{
    plan.copied_scripts.reserve(copied_scripts.size());
    for (const auto* script : copied_scripts) {
        auto valid_range = validate_range_in_bytes(
            source_bytes,
            script->raw,
            "copied script raw range is outside the source map buffer"
        );
        if (!valid_range) {
            return Result<void>::fail(valid_range.error());
        }
        plan.copied_scripts.push_back({
            script->scr_id,
            script->type,
            script->scr_obj_id,
            script->spatial_tile,
            script->raw,
            script->offsets,
        });
    }
    return Result<void>::ok();
}

Result<void> add_deleted_object_records(
    std::span<const std::byte> destination_bytes,
    const std::vector<const BinaryObjectRecord*>& deleted_objects,
    BinaryReplaceElevationPlan& plan
)
{
    plan.deleted_objects.reserve(deleted_objects.size());
    for (const auto* object : deleted_objects) {
        auto valid_range = validate_range_in_bytes(
            destination_bytes,
            object->raw,
            "deleted object raw range is outside the destination map buffer"
        );
        if (!valid_range) {
            return Result<void>::fail(valid_range.error());
        }
        plan.deleted_objects.push_back({
            object->prefix.obj_id,
            object->object_type,
            object->raw,
        });
    }
    return Result<void>::ok();
}

Result<void> add_deleted_script_records(
    std::span<const std::byte> destination_bytes,
    const std::vector<const BinaryScriptRecord*>& deleted_scripts,
    BinaryReplaceElevationPlan& plan
)
{
    plan.deleted_scripts.reserve(deleted_scripts.size());
    for (const auto* script : deleted_scripts) {
        auto valid_range = validate_range_in_bytes(
            destination_bytes,
            script->raw,
            "deleted script raw range is outside the destination map buffer"
        );
        if (!valid_range) {
            return Result<void>::fail(valid_range.error());
        }
        plan.deleted_scripts.push_back({
            script->scr_id,
            script->type,
            script->raw,
        });
    }
    return Result<void>::ok();
}

Result<void> add_exit_grid_links(
    std::span<const std::byte> source_bytes,
    const std::vector<const BinaryObjectRecord*>& copied_objects,
    BinaryReplaceElevationPlan& plan
)
{
    for (const auto* object : copied_objects) {
        if (object->object_type != BinaryObjectType::misc || object->tail.empty()) {
            continue;
        }
        auto parsed = parse_binary_misc_tail(source_bytes, object->tail);
        if (!parsed) {
            return Result<void>::fail(parsed.error());
        }
        plan.preserved_exit_grids.push_back({
            object->prefix.obj_id,
            parsed.value().dest_map,
            parsed.value().dest_tile,
            parsed.value().dest_elevation,
            parsed.value().dest_rotation,
        });
    }
    return Result<void>::ok();
}

Result<void> reject_unsupported_elevation_linking_scenery(
    const std::vector<const BinaryObjectRecord*>& copied_objects,
    const PrototypeDatabase& prototypes
)
{
    for (const auto* object : copied_objects) {
        if (object->object_type != BinaryObjectType::scenery) {
            continue;
        }

        const auto prototype = prototypes.find(object->prefix.pid);
        if (!prototype) {
            return Result<void>::fail({
                std::string{"prototype metadata missing for copied scenery PID "}
                    + std::to_string(object->prefix.pid),
                object->prefix.raw.offset,
            });
        }

        if (prototype_is_elevation_linking_scenery(prototype.value())) {
            return Result<void>::fail({
                std::string{"copied scenery PID "}
                    + std::to_string(object->prefix.pid)
                    + " has an elevation-linking subtype that replace-elevation cannot rewrite yet",
                object->prefix.raw.offset,
            });
        }
    }

    return Result<void>::ok();
}

} // namespace

Result<BinaryReplaceElevationPlan> plan_binary_replace_elevation(
    std::span<const std::byte> source_bytes,
    std::span<const std::byte> destination_bytes,
    const BinaryMap& source,
    const BinaryMap& destination,
    const PrototypeDatabase& prototypes,
    BinaryReplaceElevationRequest request
)
{
    if (!is_valid_elevation(request.source_elevation)) {
        return Result<BinaryReplaceElevationPlan>::fail({"invalid source elevation", 0});
    }
    if (!is_valid_elevation(request.destination_elevation)) {
        return Result<BinaryReplaceElevationPlan>::fail({"invalid destination elevation", 0});
    }
    if (!source.header.has_elevation(request.source_elevation)) {
        return Result<BinaryReplaceElevationPlan>::fail({"source elevation is absent", 0});
    }

    BinaryReplaceElevationPlan plan;
    plan.source_elevation = request.source_elevation;
    plan.destination_elevation = request.destination_elevation;
    plan.destination_was_present = destination.header.has_elevation(request.destination_elevation);
    plan.destination_total_objects_before = destination.objects.total_count;
    plan.destination_object_counts_before = destination.objects.elevation_counts;
    plan.destination_script_counts_before = script_counts_by_type(destination.scripts);
    const auto source_tile_span = source.tiles.elevations[request.source_elevation];
    const auto destination_tile_span = destination.tiles.elevations[request.destination_elevation];
    auto source_tile_range = range_from_backing_bytes(
        source_bytes,
        source_tile_span,
        "source tile bytes are not backed by the source map buffer"
    );
    if (!source_tile_range) {
        return Result<BinaryReplaceElevationPlan>::fail(source_tile_range.error());
    }
    auto destination_tile_range = range_from_backing_bytes(
        destination_bytes,
        destination_tile_span,
        "destination tile bytes are not backed by the destination map buffer"
    );
    if (!destination_tile_range) {
        return Result<BinaryReplaceElevationPlan>::fail(destination_tile_range.error());
    }
    plan.source_tile_range = source_tile_range.value();
    plan.destination_tile_range = destination_tile_range.value();
    plan.source_tile_bytes = source_tile_span.size();
    plan.destination_tile_bytes = destination_tile_span.size();

    const auto destination_top_level = top_level_objects_on_elevation(
        destination,
        request.destination_elevation
    );
    const auto destination_objects = flatten_selected_objects(destination_top_level);
    const auto destination_spatial = spatial_scripts_on_elevation(
        destination,
        request.destination_elevation
    );
    const auto destination_attached = attached_scripts_for_objects(
        destination,
        destination_objects
    );
    plan.deleted_top_level_objects = destination_top_level.size();
    plan.deleted_objects_including_inventory = destination_objects.size();
    plan.deleted_spatial_scripts = destination_spatial.size();
    plan.deleted_attached_scripts = destination_attached.size();
    auto deleted_object_records = add_deleted_object_records(
        destination_bytes,
        destination_objects,
        plan
    );
    if (!deleted_object_records) {
        return Result<BinaryReplaceElevationPlan>::fail(deleted_object_records.error());
    }
    auto deleted_spatial_records = add_deleted_script_records(
        destination_bytes,
        destination_spatial,
        plan
    );
    if (!deleted_spatial_records) {
        return Result<BinaryReplaceElevationPlan>::fail(deleted_spatial_records.error());
    }
    auto deleted_attached_records = add_deleted_script_records(
        destination_bytes,
        destination_attached,
        plan
    );
    if (!deleted_attached_records) {
        return Result<BinaryReplaceElevationPlan>::fail(deleted_attached_records.error());
    }

    const auto source_top_level = top_level_objects_on_elevation(source, request.source_elevation);
    const auto copied_objects = flatten_selected_objects(source_top_level);
    plan.copied_top_level_objects = source_top_level.size();
    plan.copied_objects_including_inventory = copied_objects.size();
    plan.destination_object_counts_after = plan.destination_object_counts_before;
    plan.destination_object_counts_after[request.destination_elevation] =
        static_cast<std::int32_t>(source_top_level.size());
    plan.destination_total_objects_after = 0;
    for (const auto count : plan.destination_object_counts_after) {
        plan.destination_total_objects_after += count;
    }

    auto supported_scenery = reject_unsupported_elevation_linking_scenery(
        copied_objects,
        prototypes
    );
    if (!supported_scenery) {
        return Result<BinaryReplaceElevationPlan>::fail(supported_scenery.error());
    }

    auto copied_spatial = checked_spatial_scripts_on_elevation(
        source,
        request.source_elevation
    );
    if (!copied_spatial) {
        return Result<BinaryReplaceElevationPlan>::fail(copied_spatial.error());
    }
    std::vector<const BinaryScriptRecord*> copied_scripts = std::move(copied_spatial.value());
    plan.copied_spatial_scripts = copied_scripts.size();

    std::vector<const BinaryScriptRecord*> attached_scripts;
    auto collected = collect_attached_scripts_for_copied_objects(
        source,
        copied_objects,
        attached_scripts
    );
    if (!collected) {
        return Result<BinaryReplaceElevationPlan>::fail(collected.error());
    }
    plan.copied_attached_scripts = attached_scripts.size();
    copied_scripts.insert(copied_scripts.end(), attached_scripts.begin(), attached_scripts.end());
    std::vector<const BinaryScriptRecord*> deleted_scripts;
    deleted_scripts.reserve(destination_spatial.size() + destination_attached.size());
    deleted_scripts.insert(deleted_scripts.end(), destination_spatial.begin(), destination_spatial.end());
    deleted_scripts.insert(deleted_scripts.end(), destination_attached.begin(), destination_attached.end());
    auto script_counts_after = planned_script_counts_after(
        plan.destination_script_counts_before,
        deleted_scripts,
        copied_scripts
    );
    if (!script_counts_after) {
        return Result<BinaryReplaceElevationPlan>::fail(script_counts_after.error());
    }
    plan.destination_script_counts_after = script_counts_after.value();

    auto variables_available = reject_unavailable_local_variables(destination, copied_scripts);
    if (!variables_available) {
        return Result<BinaryReplaceElevationPlan>::fail(variables_available.error());
    }

    auto copied_object_records = add_copied_object_records(source_bytes, copied_objects, plan);
    if (!copied_object_records) {
        return Result<BinaryReplaceElevationPlan>::fail(copied_object_records.error());
    }
    auto copied_script_records = add_copied_script_records(source_bytes, copied_scripts, plan);
    if (!copied_script_records) {
        return Result<BinaryReplaceElevationPlan>::fail(copied_script_records.error());
    }

    std::unordered_set<std::int32_t> used_object_ids;
    collect_object_ids(destination.objects.records, used_object_ids);
    auto object_ids = add_object_id_mappings(copied_objects, used_object_ids, plan);
    if (!object_ids) {
        return Result<BinaryReplaceElevationPlan>::fail(object_ids.error());
    }

    std::unordered_set<std::int32_t> used_script_ids;
    collect_script_ids(destination.scripts, used_script_ids);
    auto script_ids = add_script_id_mappings(copied_scripts, used_script_ids, plan);
    if (!script_ids) {
        return Result<BinaryReplaceElevationPlan>::fail(script_ids.error());
    }

    auto exit_grids = add_exit_grid_links(source_bytes, copied_objects, plan);
    if (!exit_grids) {
        return Result<BinaryReplaceElevationPlan>::fail(exit_grids.error());
    }

    return Result<BinaryReplaceElevationPlan>::ok(std::move(plan));
}

} // namespace qmap
