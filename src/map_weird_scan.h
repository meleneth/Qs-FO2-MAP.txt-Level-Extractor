#pragma once

#include "binary_map_parser.h"
#include "qmap_result.h"

#include <string>
#include <vector>

namespace qmap {

enum class WeirdMapIssueSeverity {
    info,
    warning,
    error,
};

struct WeirdMapIssue {
    WeirdMapIssueSeverity severity = WeirdMapIssueSeverity::warning;
    std::string category;
    std::string message;
    std::size_t offset = 0;
};

struct WeirdMapScanReport {
    std::vector<WeirdMapIssue> issues;
    std::size_t top_level_objects = 0;
    std::size_t objects_including_inventory = 0;
    std::size_t critters = 0;
    std::size_t objects_with_inventory = 0;
    std::size_t inventory_items = 0;
    std::size_t exit_grids = 0;
};

WeirdMapScanReport scan_weird_binary_map(
    const BinaryMap& map,
    std::span<const std::byte> bytes
);

std::string format_weird_map_scan_report(
    const WeirdMapScanReport& report,
    const std::string& input_name
);

} // namespace qmap
