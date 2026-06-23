#include "text_map_export.h"
#include "text_map_records.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace qmap {
namespace {

constexpr std::string_view scripts_header =
    ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\r\n\r\n\r\n"
    "SCRS:\r\n";
constexpr std::string_view objects_header = ">>>>>>>>>>: OBJECTS <<<<<<<<<<\r\n\r\n";
constexpr std::uint32_t script_type_bits = 0xFF000000u;
constexpr std::uint32_t script_local_id_bits = 0x00FFFFFFu;
constexpr std::uint32_t no_script_id = 0xFFFFFFFFu;
constexpr std::uint32_t base_tile_mask = 0x0FFFFFFFu;
constexpr std::uint32_t elevation_one_tile_flag = 0x20000000u;
constexpr std::uint32_t elevation_two_tile_flag = 0x40000000u;
constexpr int tiles_per_elevation = 40000;
constexpr std::string_view object_begin = "[OBJECT BEGIN]";
constexpr std::string_view object_end = "[OBJECT END]";

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

std::optional<int> source_destination(const TextMapExportPlan& plan, MapSide side, int elevation)
{
    for (int destination = 0; destination < elevation_count; ++destination) {
        if (plan.elevations[destination]
            && plan.elevations[destination]->side == side
            && plan.elevations[destination]->elevation.value == elevation) {
            return destination;
        }
    }

    return std::nullopt;
}

std::optional<int> spatial_elevation(int tile)
{
    if (tile >= 0 && tile < tiles_per_elevation) {
        return 0;
    }
    const auto encoded_tile = static_cast<std::uint32_t>(tile);
    if (encoded_tile & elevation_one_tile_flag) {
        return 1;
    }
    if (encoded_tile & elevation_two_tile_flag) {
        return 2;
    }
    return std::nullopt;
}

std::string replace_line_value(std::string_view raw, std::string_view field, std::string_view value)
{
    std::string output;
    std::optional<std::size_t> field_offset;
    std::size_t line_start = 0;
    while (line_start < raw.size()) {
        const auto line_end = raw.find_first_of("\r\n", line_start);
        const auto line = line_end == std::string_view::npos
            ? raw.substr(line_start)
            : raw.substr(line_start, line_end - line_start);

        auto candidate = line_start;
        const auto line_stop = line_start + line.size();
        while (candidate < line_stop && (raw[candidate] == ' ' || raw[candidate] == '\t')) {
            ++candidate;
        }
        const auto trimmed_line = raw.substr(candidate, line.size() - (candidate - line_start));
        if (trimmed_line.starts_with(field)) {
            field_offset = candidate;
            break;
        }

        if (line_end == std::string_view::npos) {
            break;
        }
        line_start = line_end + (raw[line_end] == '\r' && line_end + 1 < raw.size()
            && raw[line_end + 1] == '\n' ? 2 : 1);
    }

    if (!field_offset) {
        append_crlf_normalized(output, raw);
        return output;
    }

    const auto offset = *field_offset;
    append_crlf_normalized(output, raw.substr(0, offset + field.size()));
    output += ' ';
    output += value;

    const auto line_end = raw.find_first_of("\r\n", offset + field.size());
    if (line_end != std::string_view::npos) {
        append_crlf_normalized(output, raw.substr(line_end));
    }

    return output;
}

std::string replace_object_line_value(std::string_view raw, std::string_view field, std::string_view value)
{
    std::string output;
    std::optional<std::size_t> field_offset;
    int depth = 0;
    std::size_t line_start = 0;
    while (line_start < raw.size()) {
        const auto line_end = raw.find_first_of("\r\n", line_start);
        const auto line = line_end == std::string_view::npos
            ? raw.substr(line_start)
            : raw.substr(line_start, line_end - line_start);

        auto candidate = line_start;
        const auto line_stop = line_start + line.size();
        while (candidate < line_stop && (raw[candidate] == ' ' || raw[candidate] == '\t')) {
            ++candidate;
        }
        const auto trimmed_line = raw.substr(candidate, line.size() - (candidate - line_start));

        if (trimmed_line.starts_with(object_begin)) {
            ++depth;
        } else if (trimmed_line.starts_with(object_end)) {
            --depth;
        } else if (depth == 1 && trimmed_line.starts_with(field)) {
            field_offset = candidate;
            break;
        }

        if (line_end == std::string_view::npos) {
            break;
        }
        line_start = line_end + (raw[line_end] == '\r' && line_end + 1 < raw.size()
            && raw[line_end + 1] == '\n' ? 2 : 1);
    }

    if (!field_offset) {
        append_crlf_normalized(output, raw);
        return output;
    }

    const auto offset = *field_offset;
    append_crlf_normalized(output, raw.substr(0, offset + field.size()));
    output += ' ';
    output += value;

    const auto line_end = raw.find_first_of("\r\n", offset + field.size());
    if (line_end != std::string_view::npos) {
        append_crlf_normalized(output, raw.substr(line_end));
    }

    return output;
}

std::uint32_t destination_tile(std::uint32_t source_tile, int destination_elevation)
{
    auto tile = source_tile & base_tile_mask;
    if (destination_elevation == 1) {
        tile |= elevation_one_tile_flag;
    } else if (destination_elevation == 2) {
        tile |= elevation_two_tile_flag;
    }
    return tile;
}

std::string serialize_object(
    std::string_view raw,
    int destination_elevation,
    std::optional<std::uint32_t> rewritten_script_id
)
{
    auto serialized = replace_object_line_value(
        raw,
        "obj_elev:",
        std::string_view{&"012"[destination_elevation], 1}
    );
    if (rewritten_script_id) {
        serialized = replace_object_line_value(serialized, "obj_sid:", std::to_string(*rewritten_script_id));
    }

    return serialized;
}

std::string serialize_script(
    std::string_view raw,
    std::optional<std::uint32_t> rewritten_script_id,
    std::optional<std::uint32_t> rewritten_spatial_tile
)
{
    std::string serialized;
    append_crlf_normalized(serialized, raw);

    if (rewritten_script_id) {
        serialized = replace_line_value(serialized, "scr_id:", std::to_string(*rewritten_script_id));
    }
    if (rewritten_spatial_tile) {
        serialized = replace_line_value(
            serialized,
            "scr_udata.sp.built_tile:",
            std::to_string(*rewritten_spatial_tile)
        );
    }

    return serialized;
}

struct TransformBuilder {
    TextMapTransform transform;
    std::unordered_set<std::uint32_t> used_script_ids;

    Result<std::uint32_t> reserve_script_id(std::uint32_t preferred)
    {
        const auto type = preferred & script_type_bits;
        const auto preferred_local_id = preferred & script_local_id_bits;

        for (auto local_id = preferred_local_id; local_id <= script_local_id_bits; ++local_id) {
            const auto candidate = type | local_id;
            if (!used_script_ids.contains(candidate)) {
                used_script_ids.insert(candidate);
                return Result<std::uint32_t>::ok(candidate);
            }
            if (local_id == script_local_id_bits) {
                break;
            }
        }

        for (std::uint32_t local_id = 0; local_id < preferred_local_id; ++local_id) {
            const auto candidate = type | local_id;
            if (!used_script_ids.contains(candidate)) {
                used_script_ids.insert(candidate);
                return Result<std::uint32_t>::ok(candidate);
            }
        }

        return Result<std::uint32_t>::fail({"no available script ids for script type", 0});
    }
};

Result<void> append_selected_records(
    TransformBuilder& builder,
    const ParsedTextSource& source,
    MapSide side,
    const TextMapExportPlan& plan
)
{
    const auto objects_view = source.map.objects_view(source.text);
    const auto scripts_view = source.map.scripts_view(source.text);
    if (!objects_view || !scripts_view) {
        return Result<void>::fail({"invalid source section range", 0});
    }

    auto objects = parse_text_objects(*objects_view);
    if (!objects) {
        return Result<void>::fail(objects.error());
    }
    auto scripts = parse_text_scripts(*scripts_view);
    if (!scripts) {
        return Result<void>::fail(scripts.error());
    }

    std::unordered_map<std::uint32_t, std::uint32_t> copied_object_script_ids;
    for (const auto& object : objects.value()) {
        if (!object.elevation) {
            continue;
        }
        const auto destination = source_destination(plan, side, *object.elevation);
        if (!destination) {
            continue;
        }

        std::optional<std::uint32_t> rewritten_script_id;
        if (object.script_id && *object.script_id != no_script_id) {
            auto reserved = builder.reserve_script_id(*object.script_id);
            if (!reserved) {
                return Result<void>::fail(reserved.error());
            }
            rewritten_script_id = reserved.value();
            copied_object_script_ids[*object.script_id] = *rewritten_script_id;
        }

        const auto raw = view_range(*objects_view, object.raw);
        if (!raw) {
            return Result<void>::fail({"invalid object record range", object.raw.offset});
        }
        builder.transform.objects.push_back(serialize_object(*raw, *destination, rewritten_script_id));
    }

    for (const auto& script : scripts.value()) {
        bool copy = false;
        std::optional<int> destination;
        std::optional<std::uint32_t> rewritten_script_id;
        std::optional<std::uint32_t> rewritten_spatial_tile;

        if (script.script_type == ScriptType::spatial && script.spatial_tile) {
            const auto elevation = spatial_elevation(*script.spatial_tile);
            if (elevation) {
                destination = source_destination(plan, side, *elevation);
                copy = destination.has_value();
            }
            if (copy) {
                auto reserved = builder.reserve_script_id(script.script_id);
                if (!reserved) {
                    return Result<void>::fail(reserved.error());
                }
                rewritten_script_id = reserved.value();
                rewritten_spatial_tile = destination_tile(
                    static_cast<std::uint32_t>(*script.spatial_tile),
                    *destination
                );
            }
        } else if ((script.script_type == ScriptType::object || script.script_type == ScriptType::critter)
            && copied_object_script_ids.contains(script.script_id)) {
            copy = true;
            rewritten_script_id = copied_object_script_ids.at(script.script_id);
        }

        if (!copy) {
            continue;
        }

        const auto raw = view_range(*scripts_view, script.raw);
        if (!raw) {
            return Result<void>::fail({"invalid script record range", script.raw.offset});
        }
        builder.transform.scripts[script_type_index(script.script_type)].push_back(
            serialize_script(*raw, rewritten_script_id, rewritten_spatial_tile)
        );
    }

    return Result<void>::ok();
}

void append_scripts(std::string& output, const TextMapTransform& transform)
{
    output += scripts_header;
    for (int type = 0; type < script_type_count; ++type) {
        output += "scr_num: ";
        output += std::to_string(transform.scripts[type].size());
        output += "\r\n";
        for (const auto& script : transform.scripts[type]) {
            output += "\r\n";
            output += script;
            if (!output.ends_with("\r\n")) {
                output += "\r\n";
            }
        }
    }
}

void append_objects(std::string& output, const TextMapTransform& transform)
{
    output += objects_header;
    output += "[[OBJECTS BEGIN]]\r\n";
    for (const auto& object : transform.objects) {
        output += "\r\n";
        output += object;
        if (!output.ends_with("\r\n")) {
            output += "\r\n";
        }
    }
    output += "[[OBJECTS END]]\r\n";
}

} // namespace

Result<TextMapTransform> build_text_map_transform(
    const ParsedTextSource& left,
    const ParsedTextSource& right,
    const TextMapExportPlan& plan
)
{
    if (!plan.header_side) {
        return Result<TextMapTransform>::fail({"missing header selection", 0});
    }

    const auto& header_source = source_for_side(left, right, *plan.header_side);
    auto header = header_source.map.header_view(header_source.text);
    if (!header) {
        return Result<TextMapTransform>::fail({"invalid header range", 0});
    }

    TransformBuilder builder;
    builder.transform.header = std::string{*header};

    for (int destination = 0; destination < elevation_count; ++destination) {
        if (!plan.elevations[destination]) {
            continue;
        }

        const auto source = *plan.elevations[destination];
        if (!is_valid_elevation(source.elevation)) {
            return Result<TextMapTransform>::fail({
                "invalid source elevation",
                static_cast<std::size_t>(destination),
            });
        }

        const auto& map_source = source_for_side(left, right, source.side);
        const auto level = map_source.map.elevation_view(map_source.text, source.elevation.value);
        if (!level) {
            return Result<TextMapTransform>::fail({
                "selected source elevation is absent",
                static_cast<std::size_t>(destination),
            });
        }

        builder.transform.elevations[destination] = std::string{*level};
    }

    auto left_records = append_selected_records(builder, left, MapSide::left, plan);
    if (!left_records) {
        return Result<TextMapTransform>::fail(left_records.error());
    }
    auto right_records = append_selected_records(builder, right, MapSide::right, plan);
    if (!right_records) {
        return Result<TextMapTransform>::fail(right_records.error());
    }

    return Result<TextMapTransform>::ok(std::move(builder.transform));
}

Result<std::string> serialize_text_map_transform(const TextMapTransform& transform)
{
    std::string output;
    append_crlf_normalized(output, transform.header);

    for (int destination = 0; destination < elevation_count; ++destination) {
        if (!transform.elevations[destination]) {
            continue;
        }

        append_level_marker(output, destination);
        append_crlf_normalized(output, *transform.elevations[destination]);
    }

    append_scripts(output, transform);
    append_objects(output, transform);

    return Result<std::string>::ok(std::move(output));
}

Result<std::string> export_text_map(
    const ParsedTextSource& left,
    const ParsedTextSource& right,
    const TextMapExportPlan& plan
)
{
    auto transform = build_text_map_transform(left, right, plan);
    if (!transform) {
        return Result<std::string>::fail(transform.error());
    }

    return serialize_text_map_transform(transform.value());
}

} // namespace qmap
