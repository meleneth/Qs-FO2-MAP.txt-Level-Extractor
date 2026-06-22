#include "text_map_export.h"

#include <string>
#include <string_view>

namespace qmap {
namespace {

constexpr std::string_view scripts_header =
    ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\r\n\r\n\r\n"
    "SCRS:\r\n";
constexpr std::string_view empty_script_counts =
    "scr_num: 0\r\n"
    "scr_num: 0\r\n"
    "scr_num: 0\r\n"
    "scr_num: 0\r\n"
    "scr_num: 0\r\n";
constexpr std::string_view objects_header =
    ">>>>>>>>>>: OBJECTS <<<<<<<<<<\r\n\r\n"
    "[[OBJECTS BEGIN]]\r\n"
    "[[OBJECTS END]]\r\n";

const ParsedTextSource& source_for_side(
    const ParsedTextSource& left,
    const ParsedTextSource& right,
    MapSide side
)
{
    return side == MapSide::left ? left : right;
}

void append_crlf_normalized(std::string& output, std::string_view text)
{
    for (std::size_t index = 0; index < text.size(); ++index) {
        const char ch = text[index];
        if (ch == '\r') {
            if (index + 1 < text.size() && text[index + 1] == '\n') {
                output += "\r\n";
                ++index;
            } else {
                output += "\r\n";
            }
            continue;
        }
        if (ch == '\n') {
            output += "\r\n";
            continue;
        }

        output.push_back(ch);
    }
}

void append_level_marker(std::string& output, int elevation)
{
    output += "square_elev: ";
    output += static_cast<char>('0' + elevation);
    output += "\r\n\r\n";
}

} // namespace

Result<std::string> export_text_map(
    const ParsedTextSource& left,
    const ParsedTextSource& right,
    const TextMapExportPlan& plan
)
{
    const auto& header_source = source_for_side(left, right, plan.header_side);
    auto header = header_source.map.header_view(header_source.text);
    if (!header) {
        return Result<std::string>::fail({"invalid header range", 0});
    }

    std::string output;
    output.reserve(left.text.size() + right.text.size());
    append_crlf_normalized(output, *header);

    for (int destination = 0; destination < elevation_count; ++destination) {
        if (!plan.elevations[destination]) {
            continue;
        }

        const auto source = *plan.elevations[destination];
        if (source.elevation < 0 || source.elevation >= elevation_count) {
            return Result<std::string>::fail({
                "invalid source elevation",
                static_cast<std::size_t>(destination),
            });
        }

        const auto& map_source = source_for_side(left, right, source.side);
        const auto level = map_source.map.elevation_view(map_source.text, source.elevation);
        if (!level) {
            return Result<std::string>::fail({
                "selected source elevation is absent",
                static_cast<std::size_t>(destination),
            });
        }

        append_level_marker(output, destination);
        append_crlf_normalized(output, *level);
    }

    output += scripts_header;
    output += empty_script_counts;
    output += objects_header;

    return Result<std::string>::ok(std::move(output));
}

} // namespace qmap
