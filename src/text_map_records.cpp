#include "text_map_records.h"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace qmap {
namespace {

constexpr std::string_view object_begin = "[OBJECT BEGIN]";
constexpr std::string_view object_end = "[OBJECT END]";
constexpr std::string_view scr_id_field = "scr_id:";
constexpr std::string_view scr_num_field = "scr_num:";

std::string_view trim_left(std::string_view value)
{
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }
    return value;
}

std::optional<std::uint32_t> parse_u32(std::string_view value)
{
    value = trim_left(value);
    std::uint32_t parsed = 0;
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{}) {
        return std::nullopt;
    }
    return parsed;
}

std::optional<int> parse_i32(std::string_view value)
{
    value = trim_left(value);
    int parsed = 0;
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{}) {
        return std::nullopt;
    }
    return parsed;
}

std::optional<std::string_view> field_value(std::string_view record, std::string_view field)
{
    auto offset = record.find(field);
    if (offset == std::string_view::npos) {
        return std::nullopt;
    }

    offset += field.size();
    const auto end = record.find_first_of("\r\n", offset);
    if (end == std::string_view::npos) {
        return record.substr(offset);
    }
    return record.substr(offset, end - offset);
}

std::size_t line_start_after(std::string_view text, std::size_t offset)
{
    const auto next = text.find('\n', offset);
    if (next == std::string_view::npos) {
        return text.size();
    }
    return next + 1;
}

bool is_script_record_start(std::string_view text, std::size_t offset)
{
    if (offset >= text.size()) {
        return false;
    }
    if (offset > 0 && text[offset - 1] != '\n') {
        return false;
    }
    return text.substr(offset).starts_with(scr_id_field);
}

std::size_t next_script_boundary(std::string_view text, std::size_t start)
{
    auto cursor = line_start_after(text, start);
    while (cursor < text.size()) {
        const auto rest = text.substr(cursor);
        if (rest.starts_with(scr_id_field) || rest.starts_with(scr_num_field)) {
            return cursor;
        }
        cursor = line_start_after(text, cursor);
    }
    return text.size();
}

} // namespace

std::optional<ScriptType> script_type_from_id(std::uint32_t script_id)
{
    const auto raw_type = static_cast<int>(script_id >> 24);
    if (raw_type < 0 || raw_type >= script_type_count) {
        return std::nullopt;
    }
    return static_cast<ScriptType>(raw_type);
}

int script_type_index(ScriptType type)
{
    return static_cast<int>(type);
}

Result<std::vector<TextObjectRecord>> parse_text_objects(std::string_view objects_section)
{
    std::vector<TextObjectRecord> records;
    std::size_t search_offset = 0;

    while (true) {
        const auto begin = objects_section.find(object_begin, search_offset);
        if (begin == std::string_view::npos) {
            break;
        }

        const auto end_marker = objects_section.find(object_end, begin + object_begin.size());
        if (end_marker == std::string_view::npos) {
            return Result<std::vector<TextObjectRecord>>::fail({
                "object record missing [OBJECT END]",
                begin,
            });
        }

        const auto end = end_marker + object_end.size();
        const auto raw = Range{begin, end - begin};
        const auto record_text = objects_section.substr(raw.offset, raw.size);

        TextObjectRecord record;
        record.raw = raw;
        if (const auto value = field_value(record_text, "obj_elev:")) {
            record.elevation = parse_i32(*value);
        }
        if (const auto value = field_value(record_text, "obj_sid:")) {
            record.script_id = parse_u32(*value);
        }

        records.push_back(record);
        search_offset = end;
    }

    return Result<std::vector<TextObjectRecord>>::ok(std::move(records));
}

Result<std::vector<TextScriptRecord>> parse_text_scripts(std::string_view scripts_section)
{
    std::vector<TextScriptRecord> records;
    std::size_t search_offset = 0;

    while (search_offset < scripts_section.size()) {
        const auto begin = scripts_section.find(scr_id_field, search_offset);
        if (begin == std::string_view::npos) {
            break;
        }
        if (!is_script_record_start(scripts_section, begin)) {
            search_offset = begin + scr_id_field.size();
            continue;
        }

        const auto end = next_script_boundary(scripts_section, begin);
        const auto raw = Range{begin, end - begin};
        const auto record_text = scripts_section.substr(raw.offset, raw.size);
        std::optional<std::uint32_t> script_id;
        if (const auto value = field_value(record_text, scr_id_field)) {
            script_id = parse_u32(*value);
        }
        if (!script_id) {
            return Result<std::vector<TextScriptRecord>>::fail({
                "script record has invalid scr_id",
                begin,
            });
        }

        TextScriptRecord record;
        record.raw = raw;
        record.script_id = *script_id;
        const auto script_type = script_type_from_id(record.script_id);
        if (!script_type) {
            return Result<std::vector<TextScriptRecord>>::fail({
                "script record has unsupported script type",
                begin,
            });
        }
        record.script_type = *script_type;
        if (const auto value = field_value(record_text, "scr_oid:")) {
            record.object_id = parse_u32(*value);
        }
        if (const auto value = field_value(record_text, "scr_num_local_vars:")) {
            record.local_var_count = parse_i32(*value);
        }
        if (const auto value = field_value(record_text, "scr_udata.sp.built_tile:")) {
            record.spatial_tile = parse_i32(*value);
        }
        if (const auto value = field_value(record_text, "scr_udata.sp.radius:")) {
            record.spatial_radius = parse_i32(*value);
        }

        records.push_back(record);
        search_offset = end;
    }

    return Result<std::vector<TextScriptRecord>>::ok(std::move(records));
}

} // namespace qmap
