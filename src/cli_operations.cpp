#include "cli_operations.h"

#include "cli_file_io.h"
#include "cli_selection.h"
#include "cli_stats.h"
#include "prototype_metadata.h"
#include "binary_map_patch_writer.h"
#include "text_map_parser.h"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace qmap::cli {
namespace {

constexpr std::size_t dry_run_mapping_preview_limit = 10;

struct TextInput {
    std::string text;
    ParsedTextMap map;
};

TextInput read_text_map(const std::filesystem::path& path)
{
    auto text = read_text_file(path);
    auto parsed = parse_text_map(text);
    if (!parsed) {
        throw std::runtime_error(
            "failed to parse " + path.string() + ": " + parsed.error().message
        );
    }
    return {std::move(text), parsed.value()};
}

ParsedTextSource source_from(const TextInput& input)
{
    return {input.text, input.map};
}

std::size_t count_inventory_records(const BinaryObjectRecord& record)
{
    std::size_t count = 0;
    for (const auto& inventory_record : record.inventory) {
        count += 1 + count_inventory_records(inventory_record);
    }
    return count;
}

std::size_t count_object_records_including_inventory(const BinaryMapObjectRecords& objects)
{
    std::size_t count = objects.records.size();
    for (const auto& record : objects.records) {
        count += count_inventory_records(record);
    }
    return count;
}

void append_id_mapping_preview(
    std::ostringstream& output,
    std::string_view label,
    const std::vector<BinaryIdMapping>& mappings
)
{
    const auto preview_count = std::min(mappings.size(), dry_run_mapping_preview_limit);
    output << "  " << label << "_preview: " << preview_count << '\n';
    for (std::size_t index = 0; index < preview_count; ++index) {
        output << "  " << label << ": old=" << mappings[index].old_id
               << " new=" << mappings[index].new_id << '\n';
    }
    if (mappings.size() > preview_count) {
        output << "  " << label << "_omitted: " << (mappings.size() - preview_count) << '\n';
    }
}

std::string_view dry_run_script_type_name(std::size_t type)
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

} // namespace

int parse_stats(const std::filesystem::path& input)
{
    return parse_stats(ParseStatsOptions{input, {}});
}

int parse_stats(const ParseStatsOptions& options)
{
    const auto ext = lowercase_extension(options.input);
    const auto text = read_text_file(options.input);

    std::cout << "file: " << options.input.string() << '\n';
    std::cout << "bytes: " << text.size() << '\n';

    if (ext == ".txt") {
        auto parsed = parse_text_map(text);
        if (!parsed) {
            std::cout << "kind: map txt\n";
            std::cout << "status: parse failed\n";
            std::cout << "error: " << parsed.error().message
                      << " at offset " << parsed.error().offset << '\n';
            return 2;
        }

        std::cout << format_text_map_stats(text, parsed.value());
        return 0;
    }

    if (ext == ".map") {
        const auto bytes = read_binary_file(options.input);
        if (options.proto_root.empty()) {
            std::cout << "kind: binary map\n";
            std::cout << "status: parse failed\n";
            std::cout << "error: binary map object parsing requires --proto-root\n";
            return 2;
        }
        auto loaded = load_prototype_database(options.proto_root);
        if (!loaded) {
            std::cout << "kind: binary map\n";
            std::cout << "status: parse failed\n";
            std::cout << "error: " << loaded.error().message
                      << " at offset " << loaded.error().offset << '\n';
            return 2;
        }
        auto prototypes = std::move(loaded.value());
        std::cout << "prototype_metadata: loaded " << prototypes.size() << " records\n";

        auto header = parse_binary_map_header(bytes);
        if (!header) {
            std::cout << "kind: binary map\n";
            std::cout << "status: parse failed\n";
            std::cout << "error: " << header.error().message
                      << " at offset " << header.error().offset << '\n';
            return 2;
        }
        auto variables = parse_binary_map_variables(bytes, header.value());
        if (!variables) {
            std::cout << "kind: binary map\n";
            std::cout << "status: parse failed\n";
            std::cout << "error: " << variables.error().message
                      << " at offset " << variables.error().offset << '\n';
            return 2;
        }
        auto tiles = parse_binary_map_tiles(bytes, header.value());
        if (!tiles) {
            std::cout << "kind: binary map\n";
            std::cout << "status: parse failed\n";
            std::cout << "error: " << tiles.error().message
                      << " at offset " << tiles.error().offset << '\n';
            return 2;
        }
        auto scripts = parse_binary_map_scripts(bytes, header.value());
        if (!scripts) {
            std::cout << "kind: binary map\n";
            std::cout << "status: parse failed\n";
            std::cout << "error: " << scripts.error().message
                      << " at offset " << scripts.error().offset << '\n';
            return 2;
        }
        auto objects = parse_binary_map_object_counts(bytes, scripts.value().end_offset, header.value());
        if (!objects) {
            std::cout << "kind: binary map\n";
            std::cout << "status: parse failed\n";
            std::cout << "error: " << objects.error().message
                      << " at offset " << objects.error().offset << '\n';
            return 2;
        }
        auto first_object = parse_first_binary_object_prefix(bytes, scripts.value().end_offset, header.value());
        if (!first_object) {
            std::cout << "kind: binary map\n";
            std::cout << "status: parse failed\n";
            std::cout << "error: " << first_object.error().message
                      << " at offset " << first_object.error().offset << '\n';
            return 2;
        }
        std::optional<BinaryObjectRecord> first_record;
        std::optional<std::size_t> parsed_object_records_count;
        std::optional<std::size_t> parsed_object_records_total_count;
        std::optional<Error> object_records_error;
        std::span<const Error> object_record_diagnostics;
        auto object_records = parse_binary_map_object_records(
            bytes,
            scripts.value().end_offset,
            header.value(),
            prototypes
        );
        if (object_records) {
            parsed_object_records_count = object_records.value().records.size();
            parsed_object_records_total_count = count_object_records_including_inventory(object_records.value());
            object_record_diagnostics = object_records.value().diagnostics;
            if (!object_records.value().records.empty()) {
                first_record = object_records.value().records.front();
            }
        } else {
            object_records_error = object_records.error();
        }

        std::cout << format_binary_map_stats(
            header.value(),
            variables.value(),
            tiles.value(),
            scripts.value(),
            objects.value(),
            first_object.value(),
            first_record,
            bytes,
            parsed_object_records_count,
            parsed_object_records_total_count,
            object_records_error,
            object_record_diagnostics
        );
        return 0;
    }

    std::cout << "kind: unknown\n";
    std::cout << "status: unsupported extension\n";
    return 2;
}

int extract_elevation(const ExtractOptions& options)
{
    auto input = read_text_map(options.input);
    auto plan = single_elevation_plan(options.elevation);
    const ParsedTextSource empty_right{input.text, input.map};
    auto exported = export_text_map(source_from(input), empty_right, plan);
    if (!exported) {
        throw std::runtime_error(exported.error().message);
    }

    write_output_file(options.output, exported.value(), options.force);
    return 0;
}

int split_elevations(const SplitOptions& options)
{
    auto input = read_text_map(options.input);
    for (int elevation = 0; elevation < elevation_count; ++elevation) {
        if (!input.map.elevations[elevation]) {
            continue;
        }
        auto plan = single_elevation_plan(elevation);
        const ParsedTextSource empty_right{input.text, input.map};
        auto exported = export_text_map(source_from(input), empty_right, plan);
        if (!exported) {
            throw std::runtime_error(exported.error().message);
        }
        write_output_file(
            split_output_path(options.output_dir, options.input, elevation),
            exported.value(),
            options.force
        );
    }
    return 0;
}

int combine_maps(const CombineOptions& options)
{
    auto left = read_text_map(options.left);
    auto right = read_text_map(options.right);
    TextMapExportPlan plan;
    if (options.header == 0) {
        plan.header_side = MapSide::left;
    } else if (options.header == 1) {
        plan.header_side = MapSide::right;
    } else {
        throw std::runtime_error("header must be 0 for left or 1 for right");
    }

    for (const auto& spec : options.selection_specs) {
        apply_selection(plan, spec);
    }

    auto exported = export_text_map(source_from(left), source_from(right), plan);
    if (!exported) {
        throw std::runtime_error(exported.error().message);
    }
    write_output_file(options.output, exported.value(), options.force);
    return 0;
}

std::string format_replace_elevation_plan(
    const ReplaceElevationOptions& options,
    const BinaryReplaceElevationPlan& plan
)
{
    std::ostringstream output;
    output << "kind: binary replace-elevation\n";
    output << "status: planned\n";
    output << "dry_run: true\n";
    output << "source: " << options.source.string() << '\n';
    output << "destination: " << options.destination.string() << '\n';
    output << "output: " << options.output.string() << '\n';
    output << "proto_root: " << options.proto_root.string() << '\n';
    output << "source_elevation: " << plan.source_elevation << '\n';
    output << "destination_elevation: " << plan.destination_elevation << '\n';
    output << "destination_was_present: " << (plan.destination_was_present ? "true" : "false") << '\n';
    output << "tiles:\n";
    output << "  source_tile_bytes: " << plan.source_tile_bytes << '\n';
    output << "  destination_tile_bytes: " << plan.destination_tile_bytes << '\n';
    output << "delete:\n";
    output << "  top_level_objects: " << plan.deleted_top_level_objects << '\n';
    output << "  objects_with_inventory: " << plan.deleted_objects_including_inventory << '\n';
    output << "  spatial_scripts: " << plan.deleted_spatial_scripts << '\n';
    output << "  attached_scripts: " << plan.deleted_attached_scripts << '\n';
    output << "copy:\n";
    output << "  top_level_objects: " << plan.copied_top_level_objects << '\n';
    output << "  objects_with_inventory: " << plan.copied_objects_including_inventory << '\n';
    output << "  spatial_scripts: " << plan.copied_spatial_scripts << '\n';
    output << "  attached_scripts: " << plan.copied_attached_scripts << '\n';
    output << "result:\n";
    output << "  object_total_before: " << plan.destination_total_objects_before << '\n';
    output << "  object_total_after: " << plan.destination_total_objects_after << '\n';
    for (std::size_t elevation = 0; elevation < plan.destination_object_counts_after.size(); ++elevation) {
        output << "  elevation_" << elevation
               << "_objects_before: " << plan.destination_object_counts_before[elevation]
               << '\n';
        output << "  elevation_" << elevation
               << "_objects_after: " << plan.destination_object_counts_after[elevation]
               << '\n';
    }
    output << "  scripts_before_after:\n";
    for (std::size_t type = 0; type < plan.destination_script_counts_after.size(); ++type) {
        output << "    " << dry_run_script_type_name(type)
               << ": before=" << plan.destination_script_counts_before[type]
               << " after=" << plan.destination_script_counts_after[type]
               << '\n';
    }
    output << "reassign:\n";
    output << "  object_ids: " << plan.object_id_mappings.size() << '\n';
    output << "  script_ids: " << plan.script_id_mappings.size() << '\n';
    append_id_mapping_preview(output, "object_id_mapping", plan.object_id_mappings);
    append_id_mapping_preview(output, "script_id_mapping", plan.script_id_mappings);
    output << "external_links:\n";
    output << "  exit_grids: " << plan.preserved_exit_grids.size() << '\n';
    for (const auto& exit_grid : plan.preserved_exit_grids) {
        output << "  exit_grid: object_id=" << exit_grid.object_id
               << " dest_map=" << exit_grid.dest_map
               << " dest_tile=" << exit_grid.dest_tile
               << " dest_elevation=" << exit_grid.dest_elevation
               << " dest_rotation=" << exit_grid.dest_rotation
               << '\n';
    }
    output << "command:\n";
    output << "  replace-elevation " << options.source.string()
           << ' ' << options.destination.string()
           << ' ' << options.output.string()
           << " --source-elevation " << options.source_elevation
           << " --dest-elevation " << options.destination_elevation
           << " --proto-root " << options.proto_root.string()
           << " --dry-run\n";
    return output.str();
}

int replace_elevation(const ReplaceElevationOptions& options)
{
    if (options.proto_root.empty()) {
        std::cout << "kind: binary replace-elevation\n";
        std::cout << "status: failed\n";
        std::cout << "error: replace-elevation requires --proto-root\n";
        return 2;
    }

    auto loaded = load_prototype_database(options.proto_root);
    if (!loaded) {
        std::cout << "kind: binary replace-elevation\n";
        std::cout << "status: failed\n";
        std::cout << "error: " << loaded.error().message
                  << " at offset " << loaded.error().offset << '\n';
        return 2;
    }

    const auto source_bytes = read_binary_file(options.source);
    auto source = parse_binary_map(source_bytes, loaded.value());
    if (!source) {
        std::cout << "kind: binary replace-elevation\n";
        std::cout << "status: failed\n";
        std::cout << "error: source parse failed: " << source.error().message
                  << " at offset " << source.error().offset << '\n';
        return 2;
    }

    const auto destination_bytes = read_binary_file(options.destination);
    auto destination = parse_binary_map(destination_bytes, loaded.value());
    if (!destination) {
        std::cout << "kind: binary replace-elevation\n";
        std::cout << "status: failed\n";
        std::cout << "error: destination parse failed: " << destination.error().message
                  << " at offset " << destination.error().offset << '\n';
        return 2;
    }

    auto planned = plan_binary_replace_elevation(
        source_bytes,
        destination_bytes,
        source.value(),
        destination.value(),
        loaded.value(),
        BinaryReplaceElevationRequest{
            options.source_elevation,
            options.destination_elevation,
        }
    );
    if (!planned) {
        std::cout << "kind: binary replace-elevation\n";
        std::cout << "status: failed\n";
        std::cout << "error: " << planned.error().message
                  << " at offset " << planned.error().offset << '\n';
        return 2;
    }

    if (options.dry_run) {
        std::cout << format_replace_elevation_plan(options, planned.value());
        return 0;
    }

    auto written = write_binary_replace_elevation_patch({
        source_bytes,
        destination_bytes,
        planned.value(),
        destination.value().scripts.end_offset,
        destination.value().scripts.count_offsets,
    });
    if (!written) {
        std::cout << "kind: binary replace-elevation\n";
        std::cout << "status: failed\n";
        std::cout << "error: " << written.error().message
                  << " at offset " << written.error().offset << '\n';
        return 2;
    }

    write_binary_output_file(options.output, written.value(), false);
    return 0;
}

} // namespace qmap::cli
