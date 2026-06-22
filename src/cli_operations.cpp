#include "cli_operations.h"

#include "text_map_parser.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <sstream>

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

void write_output_file(const std::filesystem::path& path, std::string_view content, bool force)
{
    if (!force && std::filesystem::exists(path)) {
        throw std::runtime_error("output file already exists: " + path.string());
    }

    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        throw std::runtime_error("unable to open output file: " + path.string());
    }

    file.write(content.data(), static_cast<std::streamsize>(content.size()));
}

ParsedTextSource source_from(const TextInput& input)
{
    return {input.text, input.map};
}

void print_range(const char* label, Range range)
{
    std::cout << "  " << label << ": offset=" << range.offset
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

std::string read_text_file(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("unable to open input file: " + path.string());
    }

    return {
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>(),
    };
}

std::vector<std::byte> read_binary_file(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("unable to open input file: " + path.string());
    }

    std::vector<std::byte> bytes;
    char ch = 0;
    while (file.get(ch)) {
        bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(ch)));
    }
    return bytes;
}

std::string lowercase_extension(const std::filesystem::path& path)
{
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return ext;
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
    } else {
        output << "  object_records_status: not_attempted\n";
    }
    if (first_object) {
        output << "  first_object:\n";
        output << "    pid: " << first_object->pid << '\n';
        const auto type = binary_object_type_from_pid(first_object->pid);
        if (type) {
            output << "    type: " << object_type_name(*type) << '\n';
        } else {
            output << "    type: unknown\n";
        }
        output << "    elevation: " << first_object->elevation << '\n';
        output << "    script_id: " << first_object->script_id << '\n';
        if (first_record && type) {
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
        }
    }
    return output.str();
}

TextMapExportPlan single_elevation_plan(int elevation)
{
    if (elevation < 0 || elevation >= elevation_count) {
        throw std::runtime_error("elevation must be 0, 1, or 2");
    }

    TextMapExportPlan plan;
    plan.header_side = MapSide::left;
    plan.elevations[elevation] = ElevationSource{MapSide::left, elevation};
    return plan;
}

std::filesystem::path split_output_path(
    const std::filesystem::path& output_dir,
    const std::filesystem::path& input,
    int elevation
)
{
    auto stem = input.stem().string();
    stem += "_elev";
    stem += std::to_string(elevation);
    stem += ".txt";
    return output_dir / stem;
}

MapSide parse_side(char side)
{
    if (side == 'L' || side == 'l') {
        return MapSide::left;
    }
    if (side == 'R' || side == 'r') {
        return MapSide::right;
    }
    throw std::runtime_error("selection side must be L or R");
}

int parse_selection_elevation(std::string_view value)
{
    if (value.empty()) {
        throw std::runtime_error("selection elevations must be 0, 1, or 2");
    }

    int parsed = 0;
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end) {
        throw std::runtime_error("selection elevations must be 0, 1, or 2");
    }
    if (parsed < 0 || parsed >= elevation_count) {
        throw std::runtime_error("selection elevations must be 0, 1, or 2");
    }

    return parsed;
}

void apply_selection(TextMapExportPlan& plan, std::string_view spec)
{
    // Format: destination=side:source, for example 2=R:0.
    const auto equals = spec.find('=');
    const auto colon = spec.find(':');
    if (equals == std::string_view::npos || colon == std::string_view::npos || colon != equals + 2) {
        throw std::runtime_error("selection must use DEST=SIDE:SOURCE, for example 2=R:0");
    }

    const auto destination = parse_selection_elevation(spec.substr(0, equals));
    const auto source = parse_selection_elevation(spec.substr(colon + 1));

    plan.elevations[destination] = ElevationSource{parse_side(spec[equals + 1]), source};
}

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

        std::cout << "kind: map txt\n";
        std::cout << "status: parsed\n";
        print_range("header", parsed.value().header);
        for (int elevation = 0; elevation < elevation_count; ++elevation) {
            const auto label = "elevation " + std::to_string(elevation);
            if (parsed.value().elevations[elevation]) {
                print_range(label.c_str(), *parsed.value().elevations[elevation]);
            } else {
                std::cout << "  " << label << ": absent\n";
            }
        }
        print_range("scripts", parsed.value().scripts);
        print_range("objects", parsed.value().objects);
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
        std::optional<Error> object_records_error;
        auto object_records = parse_binary_map_object_records(bytes, scripts.value().end_offset, header.value());
        if (object_records) {
            parsed_object_records_count = object_records.value().records.size();
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
