#include "text_map_parser.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>

namespace qmap {
namespace {

constexpr std::string_view scripts_marker = ">>>>>>>>>>: SCRIPTS <<<<<<<<<<";
constexpr std::string_view objects_marker = ">>>>>>>>>>: OBJECTS <<<<<<<<<<";

struct MarkerMatch {
    std::size_t offset = 0;
    std::size_t size = 0;
};

std::string_view level_marker(int elevation, bool crlf)
{
    static constexpr std::array crlf_markers = {
        std::string_view{"square_elev: 0\r\n\r\n"},
        std::string_view{"square_elev: 1\r\n\r\n"},
        std::string_view{"square_elev: 2\r\n\r\n"},
    };
    static constexpr std::array lf_markers = {
        std::string_view{"square_elev: 0\n\n"},
        std::string_view{"square_elev: 1\n\n"},
        std::string_view{"square_elev: 2\n\n"},
    };

    return crlf ? crlf_markers[elevation] : lf_markers[elevation];
}

std::size_t find_line_start_marker(std::string_view text, std::string_view marker, std::size_t start = 0)
{
    auto offset = text.find(marker, start);
    while (offset != std::string_view::npos) {
        if (offset == 0 || text[offset - 1] == '\n') {
            return offset;
        }
        offset = text.find(marker, offset + 1);
    }
    return std::string_view::npos;
}

std::optional<MarkerMatch> find_level_marker(std::string_view text, int elevation)
{
    const auto crlf = level_marker(elevation, true);
    const auto lf = level_marker(elevation, false);
    const auto crlf_offset = find_line_start_marker(text, crlf);
    const auto lf_offset = find_line_start_marker(text, lf);

    if (crlf_offset == std::string_view::npos && lf_offset == std::string_view::npos) {
        return std::nullopt;
    }
    if (lf_offset == std::string_view::npos || crlf_offset < lf_offset) {
        return MarkerMatch{crlf_offset, crlf.size()};
    }

    return MarkerMatch{lf_offset, lf.size()};
}

bool has_duplicate_level_marker(std::string_view text, int elevation, MarkerMatch first)
{
    std::array offsets = {
        find_line_start_marker(text, level_marker(elevation, true), first.offset + 1),
        find_line_start_marker(text, level_marker(elevation, false), first.offset + 1),
    };

    for (const auto offset : offsets) {
        if (offset != std::string_view::npos) {
            return true;
        }
    }
    return false;
}

Result<std::size_t> find_required_marker(std::string_view text, std::string_view marker)
{
    const auto offset = find_line_start_marker(text, marker);
    if (offset == std::string_view::npos) {
        return Result<std::size_t>::fail({
            "required marker not found",
            text.size(),
        });
    }

    return Result<std::size_t>::ok(offset);
}

bool has_duplicate_marker(std::string_view text, std::string_view marker, std::size_t first_offset)
{
    return find_line_start_marker(text, marker, first_offset + marker.size()) != std::string_view::npos;
}

} // namespace

Result<ParsedTextMap> parse_text_map(std::string_view text)
{
    auto scripts_offset = find_required_marker(text, scripts_marker);
    if (!scripts_offset) {
        return Result<ParsedTextMap>::fail({
            "missing SCRIPTS section",
            scripts_offset.error().offset,
        });
    }

    auto objects_offset = find_required_marker(text, objects_marker);
    if (!objects_offset) {
        return Result<ParsedTextMap>::fail({
            "missing OBJECTS section",
            objects_offset.error().offset,
        });
    }

    if (objects_offset.value() < scripts_offset.value()) {
        return Result<ParsedTextMap>::fail({
            "OBJECTS section appears before SCRIPTS section",
            objects_offset.value(),
        });
    }
    if (has_duplicate_marker(text, scripts_marker, scripts_offset.value())) {
        return Result<ParsedTextMap>::fail({
            "duplicate SCRIPTS section",
            scripts_offset.value(),
        });
    }
    if (has_duplicate_marker(text, objects_marker, objects_offset.value())) {
        return Result<ParsedTextMap>::fail({
            "duplicate OBJECTS section",
            objects_offset.value(),
        });
    }

    ParsedTextMap map;
    map.scripts = Range{
        scripts_offset.value(),
        objects_offset.value() - scripts_offset.value(),
    };
    map.objects = Range{
        objects_offset.value(),
        text.size() - objects_offset.value(),
    };

    std::array<std::optional<MarkerMatch>, elevation_count> markers;
    for (int elevation = 0; elevation < elevation_count; ++elevation) {
        markers[elevation] = find_level_marker(text, elevation);
        if (markers[elevation] && markers[elevation]->offset > scripts_offset.value()) {
            return Result<ParsedTextMap>::fail({
                "elevation marker appears after SCRIPTS section",
                markers[elevation]->offset,
            });
        }
        if (markers[elevation] && has_duplicate_level_marker(text, elevation, *markers[elevation])) {
            return Result<ParsedTextMap>::fail({
                "duplicate elevation marker",
                markers[elevation]->offset,
            });
        }
    }

    std::optional<std::size_t> previous_marker_offset;
    for (const auto& marker : markers) {
        if (!marker) {
            continue;
        }
        if (previous_marker_offset && marker->offset < *previous_marker_offset) {
            return Result<ParsedTextMap>::fail({
                "elevation markers are out of order",
                marker->offset,
            });
        }
        previous_marker_offset = marker->offset;
    }

    auto first_level_offset = scripts_offset.value();
    for (const auto& marker : markers) {
        if (marker) {
            first_level_offset = std::min(first_level_offset, marker->offset);
        }
    }
    map.header = Range{0, first_level_offset};

    for (int elevation = 0; elevation < elevation_count; ++elevation) {
        if (!markers[elevation]) {
            continue;
        }

        auto next_offset = scripts_offset.value();
        for (int next = elevation + 1; next < elevation_count; ++next) {
            if (markers[next]) {
                next_offset = markers[next]->offset;
                break;
            }
        }

        const auto content_offset = markers[elevation]->offset + markers[elevation]->size;
        if (next_offset < content_offset) {
            return Result<ParsedTextMap>::fail({
                "elevation markers are out of order",
                markers[elevation]->offset,
            });
        }

        map.elevations[elevation] = Range{
            content_offset,
            next_offset - content_offset,
        };
    }

    return Result<ParsedTextMap>::ok(map);
}

} // namespace qmap
