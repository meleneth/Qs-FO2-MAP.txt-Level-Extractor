#pragma once

#include "binary_map_parser.h"
#include "text_map_parser.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace qmap::cli {

std::string format_text_map_stats(std::string_view text, const ParsedTextMap& map);
std::string format_binary_map_stats(
    const BinaryMapHeader& header,
    const BinaryMapVariables& variables,
    const BinaryMapTiles& tiles,
    const BinaryMapScripts& scripts,
    const BinaryMapObjectCounts& objects,
    const std::optional<BinaryObjectPrefix>& first_object,
    std::optional<BinaryObjectRecord> first_record = std::nullopt,
    std::span<const std::byte> bytes = {},
    std::optional<std::size_t> parsed_object_records_count = std::nullopt,
    std::optional<std::size_t> parsed_object_records_total_count = std::nullopt,
    std::optional<Error> object_records_error = std::nullopt,
    std::span<const Error> object_record_diagnostics = {}
);

} // namespace qmap::cli
