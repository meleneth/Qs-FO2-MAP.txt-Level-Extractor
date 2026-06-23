#include "cli_operations.h"

#include <CLI/CLI.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <iostream>
#include <stdexcept>
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
    } else if (options.log_format == "human") {
        spdlog::set_pattern("[%l] %v");
    } else {
        throw std::runtime_error("unknown log format: " + options.log_format);
    }
    spdlog::set_level(level);
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
    app.add_option("--log-format", options.log_format, "human|plain");

    std::filesystem::path parse_stats_input;
    auto* parse_stats_command = app.add_subcommand("parse-stats", "Parse a map file and print a stats breakdown");
    parse_stats_command->add_option("input", parse_stats_input, "Input .txt or .map file")->required();

    qmap::cli::ExtractOptions extract_options;
    auto* extract_command = app.add_subcommand("extract", "Extract one elevation from a .txt map");
    extract_command->add_option("input", extract_options.input, "Input .txt map")->required();
    extract_command->add_option("output", extract_options.output, "Output .txt map")->required();
    extract_command->add_option("--elevation", extract_options.elevation, "Elevation 0, 1, or 2")->required();
    extract_command->add_flag("-f,--force", extract_options.force, "Overwrite output if it exists");

    qmap::cli::SplitOptions split_options;
    auto* split_command = app.add_subcommand("split", "Split present elevations into separate .txt maps");
    split_command->add_option("input", split_options.input, "Input .txt map")->required();
    split_command->add_option("output-dir", split_options.output_dir, "Output directory")->required();
    split_command->add_flag("-f,--force", split_options.force, "Overwrite outputs if they exist");

    qmap::cli::CombineOptions combine_options;
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
            return qmap::cli::parse_stats(parse_stats_input);
        }
        if (*extract_command) {
            return qmap::cli::extract_elevation(extract_options);
        }
        if (*split_command) {
            return qmap::cli::split_elevations(split_options);
        }
        if (*combine_command) {
            return qmap::cli::combine_maps(combine_options);
        }

        std::cout << app.help();
        return 0;
    } catch (const std::exception& error) {
        spdlog::error("{}", error.what());
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
