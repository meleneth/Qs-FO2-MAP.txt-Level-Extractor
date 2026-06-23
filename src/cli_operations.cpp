#include "cli_operations.h"

#include "cli_file_io.h"
#include "cli_selection.h"
#include "cli_stats.h"
#include "text_map_parser.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>

namespace qmap::cli {
namespace {

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

} // namespace

int parse_stats(const std::filesystem::path& input)
{
    const auto ext = lowercase_extension(input);
    const auto text = read_text_file(input);

    std::cout << "file: " << input.string() << '\n';
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
        const auto bytes = read_binary_file(input);
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
        auto object_records = parse_binary_map_object_records(bytes, scripts.value().end_offset, header.value());
        if (object_records) {
            parsed_object_records_count = object_records.value().records.size();
            parsed_object_records_total_count = count_object_records_including_inventory(object_records.value());
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
            object_records_error
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

} // namespace qmap::cli
