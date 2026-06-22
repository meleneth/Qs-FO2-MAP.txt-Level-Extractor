#pragma once

#include "qmap_result.h"
#include "qmap_types.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace qmap {

struct TextObjectRecord {
    Range raw;
    std::optional<int> elevation;
    std::optional<std::uint32_t> script_id;
};

struct TextScriptRecord {
    Range raw;
    std::uint32_t script_id = 0;
    int script_type = 0;
    std::optional<std::uint32_t> object_id;
    std::optional<int> local_var_count;
    std::optional<int> spatial_tile;
    std::optional<int> spatial_radius;
};

Result<std::vector<TextObjectRecord>> parse_text_objects(std::string_view objects_section);
Result<std::vector<TextScriptRecord>> parse_text_scripts(std::string_view scripts_section);

} // namespace qmap
