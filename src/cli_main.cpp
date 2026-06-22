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

    CLI11_PARSE(app, argc, argv);

    try {
        configure_logging(options);
        if (*parse_stats_command) {
            return parse_stats(parse_stats_input);
        }

        std::cout << app.help();
        return 0;
    } catch (const std::exception& error) {
        spdlog::error("{}", error.what());
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
