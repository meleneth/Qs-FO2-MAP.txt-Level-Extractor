#include "cli_operations.h"

#include "text_map_parser.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <iterator>
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

std::string lowercase_extension(const std::filesystem::path& path)
{
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return ext;
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

void apply_selection(TextMapExportPlan& plan, std::string_view spec)
{
    // Format: destination=side:source, for example 2=R:0.
    const auto equals = spec.find('=');
    const auto colon = spec.find(':');
    if (equals == std::string_view::npos || colon == std::string_view::npos || colon <= equals + 1) {
        throw std::runtime_error("selection must use DEST=SIDE:SOURCE, for example 2=R:0");
    }

    const auto destination = std::stoi(std::string(spec.substr(0, equals)));
    const auto source = std::stoi(std::string(spec.substr(colon + 1)));
    if (destination < 0 || destination >= elevation_count || source < 0 || source >= elevation_count) {
        throw std::runtime_error("selection elevations must be 0, 1, or 2");
    }

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
        std::cout << "kind: binary map\n";
        std::cout << "status: detailed binary stats not implemented yet\n";
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
