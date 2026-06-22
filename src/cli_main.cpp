#include "text_map_export.h"
#include "text_map_parser.h"

#include <CLI/CLI.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace {

struct CliOptions {
    int verbosity = 0;
    bool quiet = false;
    std::string log_level = "info";
    std::string log_file;
    std::string log_format = "human";
};

struct TextInput {
    std::string text;
    qmap::ParsedTextMap map;
};

struct ExtractOptions {
    std::filesystem::path input;
    std::filesystem::path output;
    int elevation = 0;
    bool force = false;
};

struct SplitOptions {
    std::filesystem::path input;
    std::filesystem::path output_dir;
    bool force = false;
};

struct CombineOptions {
    std::filesystem::path left;
    std::filesystem::path right;
    std::filesystem::path output;
    int header = 0;
    std::vector<std::string> selection_specs;
    bool force = false;
};

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

TextInput read_text_map(const std::filesystem::path& path)
{
    auto text = read_text_file(path);
    auto parsed = qmap::parse_text_map(text);
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

std::string lowercase_extension(const std::filesystem::path& path)
{
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return ext;
}

spdlog::level::level_enum parse_level(std::string_view level)
{
    if (level == "trace") {
        return spdlog::level::trace;
    }
    if (level == "debug") {
        return spdlog::level::debug;
    }
    if (level == "info") {
        return spdlog::level::info;
    }
    if (level == "warn") {
        return spdlog::level::warn;
    }
    if (level == "error") {
        return spdlog::level::err;
    }
    if (level == "critical") {
        return spdlog::level::critical;
    }
    if (level == "off") {
        return spdlog::level::off;
    }
    throw std::runtime_error("unknown log level: " + std::string(level));
}

void configure_logging(const CliOptions& options)
{
    auto level = parse_level(options.log_level);
    if (options.verbosity == 1) {
        level = spdlog::level::debug;
    } else if (options.verbosity >= 2) {
        level = spdlog::level::trace;
    }
    if (options.quiet) {
        level = spdlog::level::err;
    }

    if (!options.log_file.empty()) {
        auto logger = spdlog::basic_logger_mt("qmap", options.log_file);
        spdlog::set_default_logger(logger);
    }

    if (options.log_format == "plain") {
        spdlog::set_pattern("%v");
    } else {
        spdlog::set_pattern("[%l] %v");
    }
    spdlog::set_level(level);
}

void print_range(const char* label, qmap::Range range)
{
    std::cout << "  " << label << ": offset=" << range.offset
              << " size=" << range.size
              << " end=" << range.end() << '\n';
}

qmap::ParsedTextSource source_from(const TextInput& input)
{
    return {input.text, input.map};
}

qmap::TextMapExportPlan single_elevation_plan(int elevation)
{
    if (elevation < 0 || elevation >= qmap::elevation_count) {
        throw std::runtime_error("elevation must be 0, 1, or 2");
    }

    qmap::TextMapExportPlan plan;
    plan.header_side = qmap::MapSide::left;
    plan.elevations[elevation] = qmap::ElevationSource{qmap::MapSide::left, elevation};
    return plan;
}

int extract_elevation(const ExtractOptions& options)
{
    auto input = read_text_map(options.input);
    auto plan = single_elevation_plan(options.elevation);
    const qmap::ParsedTextSource empty_right{input.text, input.map};
    auto exported = qmap::export_text_map(source_from(input), empty_right, plan);
    if (!exported) {
        throw std::runtime_error(exported.error().message);
    }

    write_output_file(options.output, exported.value(), options.force);
    return 0;
}

std::filesystem::path split_output_path(const std::filesystem::path& output_dir, const std::filesystem::path& input, int elevation)
{
    auto stem = input.stem().string();
    stem += "_elev";
    stem += std::to_string(elevation);
    stem += ".txt";
    return output_dir / stem;
}

int split_elevations(const SplitOptions& options)
{
    auto input = read_text_map(options.input);
    for (int elevation = 0; elevation < qmap::elevation_count; ++elevation) {
        if (!input.map.elevations[elevation]) {
            continue;
        }
        auto plan = single_elevation_plan(elevation);
        const qmap::ParsedTextSource empty_right{input.text, input.map};
        auto exported = qmap::export_text_map(source_from(input), empty_right, plan);
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

qmap::MapSide parse_side(char side)
{
    if (side == 'L' || side == 'l') {
        return qmap::MapSide::left;
    }
    if (side == 'R' || side == 'r') {
        return qmap::MapSide::right;
    }
    throw std::runtime_error("selection side must be L or R");
}

void apply_selection(qmap::TextMapExportPlan& plan, std::string_view spec)
{
    // Format: destination=side:source, for example 2=R:0.
    const auto equals = spec.find('=');
    const auto colon = spec.find(':');
    if (equals == std::string_view::npos || colon == std::string_view::npos || colon <= equals + 1) {
        throw std::runtime_error("selection must use DEST=SIDE:SOURCE, for example 2=R:0");
    }

    const auto destination = std::stoi(std::string(spec.substr(0, equals)));
    const auto source = std::stoi(std::string(spec.substr(colon + 1)));
    if (destination < 0 || destination >= qmap::elevation_count || source < 0 || source >= qmap::elevation_count) {
        throw std::runtime_error("selection elevations must be 0, 1, or 2");
    }

    plan.elevations[destination] = qmap::ElevationSource{parse_side(spec[equals + 1]), source};
}

int combine_maps(const CombineOptions& options)
{
    auto left = read_text_map(options.left);
    auto right = read_text_map(options.right);
    qmap::TextMapExportPlan plan;
    if (options.header == 0) {
        plan.header_side = qmap::MapSide::left;
    } else if (options.header == 1) {
        plan.header_side = qmap::MapSide::right;
    } else {
        throw std::runtime_error("header must be 0 for left or 1 for right");
    }

    for (const auto& spec : options.selection_specs) {
        apply_selection(plan, spec);
    }

    auto exported = qmap::export_text_map(source_from(left), source_from(right), plan);
    if (!exported) {
        throw std::runtime_error(exported.error().message);
    }
    write_output_file(options.output, exported.value(), options.force);
    return 0;
}

int parse_stats(const std::filesystem::path& input)
{
    const auto ext = lowercase_extension(input);
    const auto text = read_text_file(input);

    std::cout << "file: " << input.string() << '\n';
    std::cout << "bytes: " << text.size() << '\n';

    if (ext == ".txt") {
        auto parsed = qmap::parse_text_map(text);
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
        for (int elevation = 0; elevation < qmap::elevation_count; ++elevation) {
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
        spdlog::warn("binary parse-stats is waiting on the modeled .map parser");
        return 0;
    }

    std::cout << "kind: unknown\n";
    std::cout << "status: unsupported extension\n";
    return 2;
}

} // namespace

int main(int argc, char** argv)
{
    CLI::App app{"Q's FO2 MAP.txt Level Extractor command line tools"};
    CliOptions options;
    app.add_flag("-v,--verbose", options.verbosity, "Increase log verbosity; repeat for trace");
    app.add_flag("-q,--quiet", options.quiet, "Suppress non-error log output");
    app.add_option("--log-level", options.log_level, "trace|debug|info|warn|error|critical|off");
    app.add_option("--log-file", options.log_file, "Write logs to a file");
    app.add_option("--log-format", options.log_format, "human|plain|json");

    std::filesystem::path parse_stats_input;
    auto* parse_stats_command = app.add_subcommand("parse-stats", "Parse a map file and print a stats breakdown");
    parse_stats_command->add_option("input", parse_stats_input, "Input .txt or .map file")->required();

    ExtractOptions extract_options;
    auto* extract_command = app.add_subcommand("extract", "Extract one elevation from a .txt map");
    extract_command->add_option("input", extract_options.input, "Input .txt map")->required();
    extract_command->add_option("output", extract_options.output, "Output .txt map")->required();
    extract_command->add_option("--elevation", extract_options.elevation, "Elevation 0, 1, or 2")->required();
    extract_command->add_flag("-f,--force", extract_options.force, "Overwrite output if it exists");

    SplitOptions split_options;
    auto* split_command = app.add_subcommand("split", "Split present elevations into separate .txt maps");
    split_command->add_option("input", split_options.input, "Input .txt map")->required();
    split_command->add_option("output-dir", split_options.output_dir, "Output directory")->required();
    split_command->add_flag("-f,--force", split_options.force, "Overwrite outputs if they exist");

    CombineOptions combine_options;
    auto* combine_command = app.add_subcommand("combine", "Combine selected elevations from two .txt maps");
    combine_command->add_option("left", combine_options.left, "Left input .txt map")->required();
    combine_command->add_option("right", combine_options.right, "Right input .txt map")->required();
    combine_command->add_option("output", combine_options.output, "Output .txt map")->required();
    combine_command->add_option("--header", combine_options.header, "Header source: 0 for left, 1 for right")->default_val(0);
    combine_command->add_option("--select", combine_options.selection_specs, "DEST=SIDE:SOURCE, e.g. 0=L:1 or 2=R:0")->expected(1, -1);
    combine_command->add_flag("-f,--force", combine_options.force, "Overwrite output if it exists");

    CLI11_PARSE(app, argc, argv);

    try {
        configure_logging(options);
        if (*parse_stats_command) {
            return parse_stats(parse_stats_input);
        }
        if (*extract_command) {
            return extract_elevation(extract_options);
        }
        if (*split_command) {
            return split_elevations(split_options);
        }
        if (*combine_command) {
            return combine_maps(combine_options);
        }

        std::cout << app.help();
        return 0;
    } catch (const std::exception& error) {
        spdlog::error("{}", error.what());
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
