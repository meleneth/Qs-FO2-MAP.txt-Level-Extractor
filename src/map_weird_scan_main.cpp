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
        auto bytes_result = qmap::cli::read_binary_file_result(input);
        if (!bytes_result) {
            std::cout << qmap::format_weird_map_scan_failure(
                input.string(),
                "input read failed: " + bytes_result.error().message,
                bytes_result.error().offset
            );
            return 2;
        }
        const auto& bytes = bytes_result.value();
        auto loaded = qmap::load_prototype_database(proto_root);
        if (!loaded) {
            std::cout << qmap::format_weird_map_scan_failure(
                input.string(),
                loaded.error().message,
                loaded.error().offset
            );
            return 2;
        }

        auto parsed = qmap::parse_binary_map(bytes, loaded.value());
        if (!parsed) {
            std::cout << qmap::format_weird_map_scan_failure(
                input.string(),
                parsed.error().message,
                parsed.error().offset
            );
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
