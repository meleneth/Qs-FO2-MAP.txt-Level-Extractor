#include "map_txt_parser.h"

#include "text_map_export.h"
#include "text_map_parser.h"

#include <cstring>
#include <fstream>
#include <optional>
#include <string_view>

namespace {

constexpr std::string_view empty_text_map =
    ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
    ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n";

struct LegacyTextSource {
    std::string_view text;
    qmap::ParsedTextMap parsed;
};

std::optional<LegacyTextSource> parse_source(const map_lvls* map)
{
    if (!map || map->map_type == qmap::MapFileKind::empty || !map->data || map->file_siz <= 0) {
        auto parsed = qmap::parse_text_map(empty_text_map);
        if (!parsed) {
            return std::nullopt;
        }
        return LegacyTextSource{empty_text_map, parsed.value()};
    }

    std::string_view text{
        reinterpret_cast<const char*>(map->data),
        static_cast<std::size_t>(map->file_siz),
    };
    auto parsed = qmap::parse_text_map(text);
    if (!parsed) {
        return std::nullopt;
    }
    return LegacyTextSource{text, parsed.value()};
}

bool label_is_empty(const char* label)
{
    return !label
        || label[0] == '\0'
        || std::strncmp(label, "empty", sizeof("empty") - 1) == 0
        || std::strncmp(label, "##", sizeof("##") - 1) == 0;
}

bool label_matches(const char* left, const char* right)
{
    return left && right && std::strncmp(left, right, NAME_LENGTH) == 0;
}

} // namespace

void parse_map_txt(uint8_t* map_data, map_lvls* map)
{
    if (!map_data || !map) {
        return;
    }

    map->data = map_data;
    map->header_size = 0;
    for (int elevation = 0; elevation < qmap::elevation_count; ++elevation) {
        map->level[elevation] = nullptr;
        map->lvl_sizes[elevation] = 0;
    }
    map->scripts = nullptr;
    map->objects = nullptr;

    if (map->file_siz < 0) {
        return;
    }

    auto parsed = qmap::parse_text_map(
        std::string_view{
            reinterpret_cast<const char*>(map_data),
            static_cast<std::size_t>(map->file_siz),
        }
    );
    if (!parsed) {
        return;
    }

    auto* base = reinterpret_cast<char*>(map_data);
    map->header_size = static_cast<int>(parsed.value().header.size);
    for (int elevation = 0; elevation < qmap::elevation_count; ++elevation) {
        if (!parsed.value().elevations[elevation]) {
            continue;
        }

        const auto range = *parsed.value().elevations[elevation];
        map->level[elevation] = base + range.offset;
        map->lvl_sizes[elevation] = static_cast<int>(range.size);
    }
    map->scripts = base + parsed.value().scripts.offset;
    map->objects = base + parsed.value().objects.offset;
}

void export_map_txt(char** label_ptr_M, map_lvls* map_L, map_lvls* map_R, int header, char* path)
{
    if (!label_ptr_M || !map_L || !map_R || !path) {
        return;
    }

    auto left = parse_source(map_L);
    auto right = parse_source(map_R);
    if (!left || !right) {
        return;
    }

    qmap::TextMapExportPlan plan;
    if (header == 0) {
        plan.header_side = qmap::MapSide::left;
    } else if (header == 1) {
        plan.header_side = qmap::MapSide::right;
    } else {
        plan.header_side = std::nullopt;
    }

    for (int destination = 0; destination < qmap::elevation_count; ++destination) {
        if (label_is_empty(label_ptr_M[destination])) {
            continue;
        }

        for (int source = 0; source < qmap::elevation_count; ++source) {
            if (label_matches(label_ptr_M[destination], map_L->label_ptr[source])) {
                plan.elevations[destination] = qmap::ElevationSource{qmap::MapSide::left, source};
                break;
            }
            if (label_matches(label_ptr_M[destination], map_R->label_ptr[source])) {
                plan.elevations[destination] = qmap::ElevationSource{qmap::MapSide::right, source};
                break;
            }
        }
    }

    const qmap::ParsedTextSource left_source{left->text, left->parsed};
    const qmap::ParsedTextSource right_source{right->text, right->parsed};
    auto exported = qmap::export_text_map(left_source, right_source, plan);
    if (!exported) {
        return;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return;
    }
    output.write(exported.value().data(), static_cast<std::streamsize>(exported.value().size()));
}
