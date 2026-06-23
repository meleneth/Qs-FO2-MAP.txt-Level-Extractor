#include "map_txt_parser.h"

#include "text_map_export.h"
#include "text_map_parser.h"

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
    if (!map || map->map_type == qmap::MapFileKind::empty || map->owned_data.empty()) {
        auto parsed = qmap::parse_text_map(empty_text_map);
        if (!parsed) {
            return std::nullopt;
        }
        return LegacyTextSource{empty_text_map, parsed.value()};
    }

    std::string_view text{
        reinterpret_cast<const char*>(map->owned_data.data()),
        map->owned_data.size(),
    };
    auto parsed = qmap::parse_text_map(text);
    if (!parsed) {
        return std::nullopt;
    }
    return LegacyTextSource{text, parsed.value()};
}

} // namespace

void parse_map_txt(std::span<uint8_t> map_data, map_lvls* map)
{
    if (!map) {
        return;
    }

    map->header_size = 0;
    map->elevations = {};
    map->scripts = std::nullopt;
    map->objects = std::nullopt;
    map->parse_error.clear();

    auto parsed = qmap::parse_text_map(
        std::string_view{
            reinterpret_cast<const char*>(map_data.data()),
            map_data.size(),
        }
    );
    if (!parsed) {
        map->parse_error = parsed.error().message;
        return;
    }
    map->header_size = static_cast<int>(parsed.value().header.size);
    for (int elevation = 0; elevation < qmap::elevation_count; ++elevation) {
        if (!parsed.value().elevations[elevation]) {
            continue;
        }

        map->elevations[elevation] = parsed.value().elevations[elevation];
    }
    map->scripts = parsed.value().scripts;
    map->objects = parsed.value().objects;
}

void export_map_txt(
    const qmap::TextMapExportPlan& plan,
    map_lvls* map_L,
    map_lvls* map_R,
    char* path
)
{
    if (!map_L || !map_R || !path) {
        return;
    }

    auto left = parse_source(map_L);
    auto right = parse_source(map_R);
    if (!left || !right) {
        return;
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
