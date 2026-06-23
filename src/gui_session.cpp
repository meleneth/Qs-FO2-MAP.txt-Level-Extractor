#include "gui_session.h"

#include "binary_map_parser.h"
#include "map_txt_parser.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
#include <span>
#include <vector>

namespace qmap {
namespace {

GuiSession::MapSlot& slot_for_side(GuiSession& session, MapSide side)
{
    return side == MapSide::left ? session.left : session.right;
}

const GuiSession::MapSlot& slot_for_side(const GuiSession& session, MapSide side)
{
    return side == MapSide::left ? session.left : session.right;
}

map_lvls& map_for_side(GuiSession& session, MapSide side)
{
    return slot_for_side(session, side).map;
}

void clear_loaded_map(map_lvls& map)
{
    map.map_type = MapFileKind::empty;
    map.file_path_storage.clear();
    map.map_name_storage.clear();
    map.parse_error.clear();
    map.owned_data.clear();
    map.data_size = 0;
    map.data = nullptr;
    map.header_size = 0;
    map.elevations = {};
    map.scripts = std::nullopt;
    map.objects = std::nullopt;
}

bool load_file_bytes(const std::filesystem::path& path, std::vector<uint8_t>& bytes)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    bytes.assign(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
    return file.good() || file.eof();
}

std::string lower_extension(const std::filesystem::path& path)
{
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return extension;
}

void parse_binary_map_for_gui(map_lvls& map)
{
    map.header_size = 0;
    for (int level = 0; level < binary_map_elevation_count; ++level) {
        map.elevations[level] = std::nullopt;
    }

    if (!map.data || map.data_size == 0) {
        return;
    }

    const auto bytes = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(map.data),
        map.data_size
    );
    const auto header = parse_binary_map_header(bytes);
    if (!header) {
        map.parse_error = header.error().message;
        return;
    }

    map.header_size = static_cast<int>(binary_map_header_size);
    for (int level = 0; level < binary_map_elevation_count; ++level) {
        if (header.value().has_elevation(level)) {
            map.elevations[level] = Range{0, map.data_size};
        }
    }
}

} // namespace

std::array<std::string, elevation_count>& labels_for_side(GuiSession& session, MapSide side)
{
    return slot_for_side(session, side).labels;
}

const std::array<std::string, elevation_count>& labels_for_side(const GuiSession& session, MapSide side)
{
    return slot_for_side(session, side).labels;
}

const char* map_type_name(MapFileKind map_type)
{
    switch (map_type) {
    case MapFileKind::text:
        return ".txt";
    case MapFileKind::binary:
        return ".map";
    case MapFileKind::empty:
        return "empty";
    }

    return "empty";
}

bool map_parse_succeeded(const map_lvls& map)
{
    if (!map.data || map.map_type == MapFileKind::empty) {
        return false;
    }
    if (map.map_type == MapFileKind::text) {
        return map.scripts.has_value() && map.objects.has_value();
    }
    if (map.map_type == MapFileKind::binary) {
        return map.header_size == static_cast<int>(binary_map_header_size);
    }

    return false;
}

void reset_output_selection(GuiSession& session)
{
    session.output_selection = {};
    session.output_labels = {"empty", "##1", "##2"};
}

void update_loaded_map_labels(GuiSession& session, const map_lvls& map, MapSide side)
{
    slot_for_side(session, side).heading = map.map_name_storage;
    reset_output_selection(session);
    auto& labels = labels_for_side(session, side);
    for (int elevation = 0; elevation < elevation_count; ++elevation) {
        labels[elevation] = map.elevations[elevation]
            ? std::to_string(elevation) + ":" + map.map_name_storage
            : "empty";
    }
}

void select_output_elevation(
    GuiSession& session,
    int destination,
    const map_lvls& source_map,
    const std::string& source_label,
    MapSide side,
    int source_elevation
)
{
    if (destination < 0 || destination >= elevation_count
        || source_elevation < 0 || source_elevation >= elevation_count
        || !source_map.elevations[source_elevation]) {
        return;
    }

    session.output_labels[destination] = source_label;
    session.output_selection[destination] = ElevationSource{side, source_elevation};
}

void clear_output_elevation(GuiSession& session, int destination)
{
    if (destination < 0 || destination >= elevation_count) {
        return;
    }

    session.output_labels[destination] = "##" + std::to_string(destination);
    session.output_selection[destination] = std::nullopt;
}

void choose_output_header(GuiSession& session, MapSide side)
{
    const map_lvls& map = slot_for_side(session, side).map;
    if (!map.data) {
        session.middle_head = side == MapSide::left ? "HeaderL##" : "HeaderR##";
        return;
    }

    session.middle_head = map.map_name_storage + "##";
    std::snprintf(session.export_path, gui_export_path_size, "%s.Q.txt", map.file_path_storage.c_str());
    session.header = side == MapSide::left ? 0 : 1;
}

void clear_output_header(GuiSession& session)
{
    session.header = -1;
    session.middle_head = "empty";
}

TextMapExportPlan make_text_export_plan(const GuiSession& session)
{
    TextMapExportPlan plan;
    if (session.header == 0) {
        plan.header_side = MapSide::left;
    } else if (session.header == 1) {
        plan.header_side = MapSide::right;
    } else {
        plan.header_side = std::nullopt;
    }
    plan.elevations = session.output_selection;
    return plan;
}

GuiExportAction prepare_export(GuiSession& session)
{
    if (session.left.map.data == nullptr && session.right.map.data == nullptr) {
        return GuiExportAction::none;
    }
    if (session.left.map.map_type == MapFileKind::empty && session.right.map.map_type == MapFileKind::empty) {
        return GuiExportAction::none;
    }

    if ((session.left.map.map_type == MapFileKind::binary || session.left.map.map_type == MapFileKind::empty)
        && (session.right.map.map_type == MapFileKind::binary || session.right.map.map_type == MapFileKind::empty)) {
        session.open_error_popup = true;
        session.current_error =
            ".MAP export is not implemented yet.\n"
            "The file can be parsed, but binary export\n"
            "is disabled until the full format is modeled.";
        return GuiExportAction::none;
    }

    if ((session.left.map.map_type == MapFileKind::text || session.left.map.map_type == MapFileKind::empty)
        && (session.right.map.map_type == MapFileKind::text || session.right.map.map_type == MapFileKind::empty)) {
        return GuiExportAction::export_text;
    }

    session.open_error_popup = true;
    session.current_error =
        "Sorry, can't mix .MAP and .TXT yet.\n"
        "It's just a pain in the butt to\n"
        "combine these two filetypes,\n"
        "so I'm leaving this out for now.\n"
        "Let me know if you want this!";
    return GuiExportAction::none;
}

void export_session_map(GuiSession& session, char* path)
{
    if (prepare_export(session) == GuiExportAction::export_text) {
        export_map_txt(make_text_export_plan(session), &session.left.map, &session.right.map, path);
    }
}

bool load_dropped_file(GuiSession& session, const std::filesystem::path& file_path)
{
    if (!session.drop_target) {
        return false;
    }

    const std::string extension = lower_extension(file_path);
    MapFileKind map_type = MapFileKind::empty;
    if (extension == ".txt") {
        map_type = MapFileKind::text;
    } else if (extension == ".map") {
        map_type = MapFileKind::binary;
    } else {
        session.current_error =
            "Wrong file type.\n"
            "Should be Fallout 2 'map.txt'.\n"
            "You can export a single map.txt\n"
            "from the Fallout 2 Mapper\n"
            "by opening the map you want\n"
            "to export and pressing 'Alt + P'.";
        session.open_error_popup = true;
        return false;
    }

    std::vector<uint8_t> bytes;
    if (!load_file_bytes(file_path, bytes)) {
        session.current_error = std::string{"Unable to read file:\n"} + file_path.string();
        session.open_error_popup = true;
        return false;
    }

    map_lvls& map = map_for_side(session, *session.drop_target);
    clear_loaded_map(map);
    if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        session.current_error = std::string{"File is too large:\n"} + file_path.string();
        session.open_error_popup = true;
        return false;
    }

    map.file_path_storage = file_path.string();
    map.map_name_storage = file_path.filename().string();
    map.owned_data = std::move(bytes);
    map.data_size = map.owned_data.size();
    map.data = map.owned_data.data();
    map.map_type = map_type;

    if (map.map_type == MapFileKind::binary) {
        parse_binary_map_for_gui(map);
    } else if (map.map_type == MapFileKind::text) {
        parse_map_txt(std::span<uint8_t>{map.owned_data.data(), map.owned_data.size()}, &map);
    }

    update_loaded_map_labels(session, map, *session.drop_target);
    session.drop_target = std::nullopt;
    return true;
}

} // namespace qmap
