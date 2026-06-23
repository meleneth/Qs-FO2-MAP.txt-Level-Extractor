#include "cli_file_io.h"
#include "map_weird_scan.h"
#include "prototype_metadata.h"

#include <CLI/CLI.hpp>

#include <filesystem>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv)
{
    CLI::App app{"Scan a Fallout 2 .map for suspicious object/script/inventory/exit data"};
    std::filesystem::path input;
    std::filesystem::path proto_root;

    app.add_option("input", input, "Input .map file")->required();
    app.add_option(
        "--proto-root",
        proto_root,
        "Extracted Fallout 2 proto root, e.g. .local_fallout2_data/proto"
    )->required();

    CLI11_PARSE(app, argc, argv);

    try {
        const auto bytes = qmap::cli::read_binary_file(input);
        auto loaded = qmap::load_prototype_database(proto_root);
        if (!loaded) {
            std::cout << "kind: binary map weird scan\n";
            std::cout << "status: parse failed\n";
            std::cout << "error: " << loaded.error().message
                      << " at offset " << loaded.error().offset << '\n';
            return 2;
        }

        auto parsed = qmap::parse_binary_map(bytes, loaded.value());
        if (!parsed) {
            std::cout << "kind: binary map weird scan\n";
            std::cout << "file: " << input.string() << '\n';
            std::cout << "status: parse failed\n";
            std::cout << "error: " << parsed.error().message
                      << " at offset " << parsed.error().offset << '\n';
            return 2;
        }

        const auto report = qmap::scan_weird_binary_map(parsed.value(), bytes);
        std::cout << qmap::format_weird_map_scan_report(report, input.string());
        return report.issues.empty() ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
