#include "cli_stats.h"

#include "text_map_records.h"

#include <array>
#include <sstream>

namespace qmap::cli {
namespace {

void append_range(std::ostream& output, const char* label, Range range)
{
    output << "  " << label << ": offset=" << range.offset
           << " size=" << range.size
           << " end=" << range.end() << '\n';
}

std::string script_type_name(int type)
{
    switch (static_cast<BinaryScriptType>(type)) {
    case BinaryScriptType::system:
        return "system";
    case BinaryScriptType::spatial:
        return "spatial";
    case BinaryScriptType::timed:
        return "timed";
    case BinaryScriptType::object:
        return "object";
    case BinaryScriptType::critter:
        return "critter";
    }
    return "unknown";
}

std::string object_type_name(BinaryObjectType type)
{
    switch (type) {
    case BinaryObjectType::item:
        return "item";
    case BinaryObjectType::critter:
        return "critter";
    case BinaryObjectType::scenery:
        return "scenery";
    case BinaryObjectType::wall:
        return "wall";
    case BinaryObjectType::tile:
        return "tile";
    case BinaryObjectType::misc:
        return "misc";
    case BinaryObjectType::interface_object:
        return "interface";
    case BinaryObjectType::inventory:
        return "inventory";
    case BinaryObjectType::head:
        return "head";
    case BinaryObjectType::background:
        return "background";
    }
    return "unknown";
}

} // namespace

std::string format_text_map_stats(std::string_view text, const ParsedTextMap& map)
{
    std::ostringstream output;
    output << "kind: map txt\n";
    output << "status: parsed\n";
    append_range(output, "header", map.header);
    for (int elevation = 0; elevation < elevation_count; ++elevation) {
        const auto label = "elevation " + std::to_string(elevation);
        if (map.elevations[elevation]) {
            append_range(output, label.c_str(), *map.elevations[elevation]);
        } else {
            output << "  " << label << ": absent\n";
        }
    }
    append_range(output, "scripts", map.scripts);
    append_range(output, "objects", map.objects);

    output << "text scripts:\n";
    const auto scripts_view = map.scripts_view(text);
    if (!scripts_view) {
        output << "  status: invalid range\n";
    } else {
        auto scripts = parse_text_scripts(*scripts_view);
        if (!scripts) {
            output << "  status: parse failed\n";
            output << "  error: " << scripts.error().message << '\n';
            output << "  error_offset: " << scripts.error().offset << '\n';
        } else {
            std::array<std::size_t, script_type_count> counts{};
            for (const auto& script : scripts.value()) {
                counts[static_cast<std::size_t>(script_type_index(script.script_type))] += 1;
            }
            output << "  total: " << scripts.value().size() << '\n';
            for (int type = 0; type < script_type_count; ++type) {
                output << "  " << script_type_name(type) << ": " << counts[static_cast<std::size_t>(type)] << '\n';
            }
        }
    }

    output << "text objects:\n";
    const auto objects_view = map.objects_view(text);
    if (!objects_view) {
        output << "  status: invalid range\n";
    } else {
        auto objects = parse_text_objects(*objects_view);
        if (!objects) {
            output << "  status: parse failed\n";
            output << "  error: " << objects.error().message << '\n';
            output << "  error_offset: " << objects.error().offset << '\n';
        } else {
            std::array<std::size_t, elevation_count> elevation_counts{};
            std::size_t without_elevation = 0;
            for (const auto& object : objects.value()) {
                if (object.elevation && is_valid_elevation(*object.elevation)) {
                    elevation_counts[static_cast<std::size_t>(*object.elevation)] += 1;
                } else {
                    without_elevation += 1;
                }
            }

            output << "  total: " << objects.value().size() << '\n';
            for (int elevation = 0; elevation < elevation_count; ++elevation) {
                output << "  elevation " << elevation << ": "
                       << elevation_counts[static_cast<std::size_t>(elevation)] << '\n';
            }
            output << "  without_elevation: " << without_elevation << '\n';
        }
    }

    return output.str();
}

std::string format_binary_map_stats(
    const BinaryMapHeader& header,
    const BinaryMapVariables& variables,
    const BinaryMapTiles& tiles,
    const BinaryMapScripts& scripts,
    const BinaryMapObjectCounts& objects,
    const std::optional<BinaryObjectPrefix>& first_object,
    std::optional<BinaryObjectRecord> first_record,
    std::span<const std::byte> bytes,
    std::optional<std::size_t> parsed_object_records_count,
    std::optional<std::size_t> parsed_object_records_total_count,
    std::optional<Error> object_records_error
)
{
    std::ostringstream output;
    output << "kind: binary map\n";
    output << "status: parsed\n";
    output << "header:\n";
    output << "  version: " << header.version << '\n';
    output << "  filename: " << header.filename_string() << '\n';
    output << "  map_id: " << header.map_id << '\n';
    output << "  map_flags: " << header.map_flags << '\n';
    output << "  start: tile=" << header.dude_start
           << " elevation=" << header.elev_start
           << " rotation=" << header.face_start << '\n';
    output << "variables:\n";
    output << "  map: " << variables.map_vars.size() << '\n';
    output << "  local: " << variables.local_vars.size() << '\n';
    output << "elevations:\n";
    for (int elevation = 0; elevation < binary_map_elevation_count; ++elevation) {
        output << "  elevation " << elevation << ": ";
        if (tiles.elevations[elevation].empty()) {
            output << "absent\n";
        } else {
            output << "tile_bytes=" << tiles.elevations[elevation].size() << '\n';
        }
    }
    output << "scripts:\n";
    for (int type = 0; type < binary_script_type_count; ++type) {
        output << "  " << script_type_name(type) << ": " << scripts.by_type[type].size() << '\n';
    }
    output << "  section_end: " << scripts.end_offset << '\n';
    output << "objects:\n";
    output << "  total: " << objects.total_count << '\n';
    if (objects.first_counted_elevation >= 0) {
        output << "  first_counted_elevation: " << objects.first_counted_elevation << '\n';
        output << "  first_elevation_count: "
               << objects.elevation_counts[objects.first_counted_elevation] << '\n';
    }
    output << "  data_offset: " << objects.data_offset << '\n';
    if (object_records_error) {
        output << "  object_records_status: incomplete\n";
        output << "  object_records_error: " << object_records_error->message << '\n';
        output << "  object_records_error_offset: " << object_records_error->offset << '\n';
    } else if (parsed_object_records_count) {
        output << "  object_records_status: parsed\n";
        output << "  object_records_parsed: " << *parsed_object_records_count << '\n';
        if (parsed_object_records_total_count) {
            output << "  object_records_parsed_with_inventory: "
                   << *parsed_object_records_total_count << '\n';
        }
    } else {
        output << "  object_records_status: not_attempted\n";
    }
    if (first_object) {
        output << "  first_object:\n";
        output << "    pid: " << first_object->pid << '\n';
        const auto type = first_record
            ? std::optional<BinaryObjectType>{first_record->object_type}
            : binary_object_type_from_pid(first_object->pid);
        output << "    type: " << (type ? object_type_name(*type) : "unknown") << '\n';
        output << "    elevation: " << first_object->elevation << '\n';
        output << "    script_id: " << first_object->script_id << '\n';
        output << "    inventory_count: " << first_object->inventory_count << '\n';
        output << "    inventory_size: " << first_object->inventory_size << '\n';
        output << "    unknown_10: " << first_object->unknown_10 << '\n';
        output << "    unknown_11: " << first_object->unknown_11 << '\n';
        if (first_record && type) {
            output << "    raw_range: offset=" << first_record->raw.offset
                   << " size=" << first_record->raw.size
                   << " end=" << first_record->raw.end() << '\n';
            output << "    tail_range: offset=" << first_record->tail.offset
                   << " size=" << first_record->tail.size
                   << " end=" << first_record->tail.end() << '\n';
            if (*type == BinaryObjectType::scenery) {
                auto tail = parse_binary_scenery_tail(bytes, first_record->tail);
                if (tail) {
                    output << "    scenery_flags: " << tail.value().flags << '\n';
                    output << "    scenery_destination: " << tail.value().destination << '\n';
                }
            } else if (*type == BinaryObjectType::critter) {
                auto tail = parse_binary_critter_tail(bytes, first_record->tail);
                if (tail) {
                    output << "    critter_team: " << tail.value().team << '\n';
                    output << "    critter_hit_points: " << tail.value().hit_points << '\n';
                }
            } else if (*type == BinaryObjectType::misc) {
                auto tail = parse_binary_misc_tail(bytes, first_record->tail);
                if (tail) {
                    output << "    misc_dest_map: " << tail.value().dest_map << '\n';
                    output << "    misc_dest_elevation: " << tail.value().dest_elevation << '\n';
                }
            }
            if (!first_record->inventory.empty()) {
                const auto& inventory = first_record->inventory.front();
                output << "    first_inventory_quantity: "
                       << first_record->inventory_quantities.front() << '\n';
                output << "    first_inventory_pid: " << inventory.prefix.pid << '\n';
                output << "    first_inventory_type: " << object_type_name(inventory.object_type) << '\n';
            }
        }
    }
    return output.str();
}

} // namespace qmap::cli
