#include "map_weird_scan.h"

#include <cstdint>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace qmap {
namespace {

constexpr std::int32_t no_script_id = -1;
constexpr std::int32_t first_exit_grid_pid = 0x05000010;
constexpr std::int32_t last_exit_grid_pid = 0x05000017;
constexpr int max_tile = 39999;

std::string severity_name(WeirdMapIssueSeverity severity)
{
    switch (severity) {
    case WeirdMapIssueSeverity::info:
        return "info";
    case WeirdMapIssueSeverity::warning:
        return "warning";
    case WeirdMapIssueSeverity::error:
        return "error";
    }
    return "unknown";
}

void add_issue(
    WeirdMapScanReport& report,
    WeirdMapIssueSeverity severity,
    std::string category,
    std::string message,
    std::size_t offset
)
{
    report.issues.push_back({
        severity,
        std::move(category),
        std::move(message),
        offset,
    });
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

void collect_object_records(
    const std::vector<BinaryObjectRecord>& records,
    std::vector<const BinaryObjectRecord*>& output
)
{
    for (const auto& record : records) {
        output.push_back(&record);
        collect_object_records(record.inventory, output);
    }
}

void collect_script_records(
    const BinaryMapScripts& scripts,
    std::vector<const BinaryScriptRecord*>& output
)
{
    for (const auto& records : scripts.by_type) {
        for (const auto& script : records) {
            output.push_back(&script);
        }
    }
}

bool is_exit_grid_pid(std::int32_t pid)
{
    return pid >= first_exit_grid_pid && pid <= last_exit_grid_pid;
}

bool is_valid_exit_tile(std::int32_t tile)
{
    return tile >= 0 && tile <= max_tile;
}

bool is_valid_rotation(std::int32_t rotation)
{
    return rotation >= 0 && rotation <= 5;
}

} // namespace

WeirdMapScanReport scan_weird_binary_map(
    const BinaryMap& map,
    std::span<const std::byte> bytes
)
{
    WeirdMapScanReport report;
    report.top_level_objects = map.objects.records.size();

    std::vector<const BinaryObjectRecord*> objects;
    collect_object_records(map.objects.records, objects);
    report.objects_including_inventory = objects.size();

    std::vector<const BinaryScriptRecord*> scripts;
    collect_script_records(map.scripts, scripts);

    std::unordered_set<std::int32_t> object_ids;
    collect_object_ids(map.objects.records, object_ids);

    std::unordered_set<std::int32_t> script_ids;
    collect_script_ids(map.scripts, script_ids);

    for (const auto* object : objects) {
        if (object->object_type == BinaryObjectType::critter) {
            ++report.critters;
        }
        if (object->prefix.inventory_count > 0 || !object->inventory.empty()) {
            ++report.objects_with_inventory;
            report.inventory_items += object->inventory.size();
        }
        if (object->prefix.inventory_count != static_cast<std::int32_t>(object->inventory.size())) {
            add_issue(
                report,
                WeirdMapIssueSeverity::error,
                "inventory",
                "object " + std::to_string(object->prefix.obj_id)
                    + " inventory_count="
                    + std::to_string(object->prefix.inventory_count)
                    + " but parsed inventory items="
                    + std::to_string(object->inventory.size()),
                object->prefix.raw.offset
            );
        }

        if (object->prefix.script_id != no_script_id
            && object->prefix.script_id != 0
            && !script_ids.contains(object->prefix.script_id)) {
            add_issue(
                report,
                WeirdMapIssueSeverity::error,
                "missing-script",
                "object " + std::to_string(object->prefix.obj_id)
                    + " references missing script "
                    + std::to_string(object->prefix.script_id),
                object->prefix.raw.offset
            );
        }

        if (object->object_type == BinaryObjectType::misc && is_exit_grid_pid(object->prefix.pid)) {
            ++report.exit_grids;
            auto parsed = parse_binary_misc_tail(bytes, object->tail);
            if (!parsed) {
                add_issue(
                    report,
                    WeirdMapIssueSeverity::error,
                    "exit-grid",
                    "exit grid object " + std::to_string(object->prefix.obj_id)
                        + " has an undecodable tail: "
                        + parsed.error().message,
                    parsed.error().offset
                );
                continue;
            }

            if (!is_valid_exit_tile(parsed.value().dest_tile)
                || !is_valid_elevation(parsed.value().dest_elevation)
                || !is_valid_rotation(parsed.value().dest_rotation)) {
                add_issue(
                    report,
                    WeirdMapIssueSeverity::warning,
                    "exit-grid",
                    "exit grid object " + std::to_string(object->prefix.obj_id)
                        + " points to dest_map="
                        + std::to_string(parsed.value().dest_map)
                        + " tile="
                        + std::to_string(parsed.value().dest_tile)
                        + " elevation="
                        + std::to_string(parsed.value().dest_elevation)
                        + " rotation="
                        + std::to_string(parsed.value().dest_rotation),
                    object->tail.offset
                );
            }
        }
    }

    for (const auto* script : scripts) {
        if ((script->type == BinaryScriptType::object || script->type == BinaryScriptType::critter)
            && script->scr_obj_id != 0
            && !object_ids.contains(static_cast<std::int32_t>(script->scr_obj_id))) {
            add_issue(
                report,
                WeirdMapIssueSeverity::error,
                "dangling-script-object",
                "script " + std::to_string(script->scr_id)
                    + " references missing object "
                    + std::to_string(script->scr_obj_id),
                script->raw.offset
            );
        }
    }

    return report;
}

std::string format_weird_map_scan_report(
    const WeirdMapScanReport& report,
    const std::string& input_name
)
{
    std::ostringstream output;
    output << "kind: binary map weird scan\n";
    output << "file: " << input_name << '\n';
    output << "status: " << (report.issues.empty() ? "clean" : "issues") << '\n';
    output << "issues: " << report.issues.size() << '\n';
    output << "summary:\n";
    output << "  top_level_objects: " << report.top_level_objects << '\n';
    output << "  objects_including_inventory: " << report.objects_including_inventory << '\n';
    output << "  critters: " << report.critters << '\n';
    output << "  objects_with_inventory: " << report.objects_with_inventory << '\n';
    output << "  inventory_items: " << report.inventory_items << '\n';
    output << "  exit_grids: " << report.exit_grids << '\n';
    for (const auto& issue : report.issues) {
        output << "issue: severity=" << severity_name(issue.severity)
               << " category=" << issue.category
               << " offset=" << issue.offset
               << " message=\"" << issue.message << "\"\n";
    }
    return output.str();
}

} // namespace qmap
