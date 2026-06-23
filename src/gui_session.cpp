#include "gui_session.h"

#include "binary_map_parser.h"
#include "text_map_parser.h"

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

constexpr std::string_view empty_text_map =
    ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
    ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n";

GuiSession::MapSlot& slot_for_side(GuiSession& session, MapSide side)
{
    return side == MapSide::left ? session.left : session.right;
}

const GuiSession::MapSlot& slot_for_side(const GuiSession& session, MapSide side)
{
    return side == MapSide::left ? session.left : session.right;
}

void clear_loaded_slot(GuiSession::MapSlot& slot)
{
    slot.map_type = MapFileKind::empty;
    slot.file_path.clear();
    slot.map_name.clear();
    slot.parse_error.clear();
    slot.owned_data.clear();
    slot.header_size = 0;
    slot.elevations = {};
    slot.parsed_text = std::nullopt;
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

void parse_binary_map_for_gui(GuiSession::MapSlot& slot)
{
    slot.header_size = 0;
    for (int level = 0; level < binary_map_elevation_count; ++level) {
        slot.elevations[level] = std::nullopt;
    }

    if (slot.owned_data.empty()) {
        return;
    }

    const auto bytes = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(slot.owned_data.data()),
        slot.owned_data.size()
    );
    const auto header = parse_binary_map_header(bytes);
    if (!header) {
        slot.parse_error = header.error().message;
        return;
    }

    slot.header_size = static_cast<int>(binary_map_header_size);
    for (int level = 0; level < binary_map_elevation_count; ++level) {
        if (header.value().has_elevation(level)) {
            slot.elevations[level] = Range{0, slot.owned_data.size()};
        }
    }
}

std::optional<ParsedTextSource> text_source_for_slot(const GuiSession::MapSlot& slot)
{
    if (slot.map_type == MapFileKind::empty || slot.owned_data.empty()) {
        auto parsed = parse_text_map(empty_text_map);
        if (!parsed) {
            return std::nullopt;
        }
        return ParsedTextSource{empty_text_map, parsed.value()};
    }

    if (!slot.parsed_text) {
        return std::nullopt;
    }

    const auto text = std::string_view{
        reinterpret_cast<const char*>(slot.owned_data.data()),
        slot.owned_data.size(),
    };
    return ParsedTextSource{text, *slot.parsed_text};
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

bool map_parse_succeeded(const GuiSession::MapSlot& slot)
{
    if (slot.owned_data.empty() || slot.map_type == MapFileKind::empty) {
        return false;
    }
    if (slot.map_type == MapFileKind::text) {
        return slot.parsed_text.has_value();
    }
    if (slot.map_type == MapFileKind::binary) {
        return slot.header_size == static_cast<int>(binary_map_header_size);
    }

    return false;
}

void reset_output_selection(GuiSession& session)
{
    session.output_selection = {};
    session.output_labels = {"empty", "##1", "##2"};
}

void update_loaded_map_labels(GuiSession& session, MapSide side)
{
    auto& slot = slot_for_side(session, side);
    slot.heading = slot.map_name;
    reset_output_selection(session);
    auto& labels = labels_for_side(session, side);
    for (int elevation = 0; elevation < elevation_count; ++elevation) {
        labels[elevation] = slot.elevations[elevation]
            ? std::to_string(elevation) + ":" + slot.map_name
            : "empty";
    }
}

void select_output_elevation(
    GuiSession& session,
    int destination,
    const GuiSession::MapSlot& source_slot,
    const std::string& source_label,
    MapSide side,
    int source_elevation
)
{
    if (!is_valid_elevation(destination)
        || !is_valid_elevation(source_elevation)
        || !source_slot.elevations[source_elevation]) {
        return;
    }

    session.output_labels[destination] = source_label;
    session.output_selection[destination] = ElevationSource{side, source_elevation};
}

void clear_output_elevation(GuiSession& session, int destination)
{
    if (!is_valid_elevation(destination)) {
        return;
    }

    session.output_labels[destination] = "##" + std::to_string(destination);
    session.output_selection[destination] = std::nullopt;
}

void choose_output_header(GuiSession& session, MapSide side)
{
    const auto& slot = slot_for_side(session, side);
    if (slot.owned_data.empty()) {
        session.middle_head = side == MapSide::left ? "HeaderL##" : "HeaderR##";
        return;
    }

    session.middle_head = slot.map_name + "##";
    std::snprintf(session.export_path, gui_export_path_size, "%s.Q.txt", slot.file_path.c_str());
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
    if (session.left.owned_data.empty() && session.right.owned_data.empty()) {
        return GuiExportAction::none;
    }
    if (session.left.map_type == MapFileKind::empty && session.right.map_type == MapFileKind::empty) {
        return GuiExportAction::none;
    }

    if ((session.left.map_type == MapFileKind::binary || session.left.map_type == MapFileKind::empty)
        && (session.right.map_type == MapFileKind::binary || session.right.map_type == MapFileKind::empty)) {
        session.open_error_popup = true;
        session.current_error =
            ".MAP export is not implemented yet.\n"
            "The file can be parsed, but binary export\n"
            "is disabled until the full format is modeled.";
        return GuiExportAction::none;
    }

    if ((session.left.map_type == MapFileKind::text || session.left.map_type == MapFileKind::empty)
        && (session.right.map_type == MapFileKind::text || session.right.map_type == MapFileKind::empty)) {
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
        auto left = text_source_for_slot(session.left);
        auto right = text_source_for_slot(session.right);
        if (!left || !right) {
            return;
        }

        auto exported = export_text_map(*left, *right, make_text_export_plan(session));
        if (!exported) {
            return;
        }

        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output) {
            return;
        }
        output.write(exported.value().data(), static_cast<std::streamsize>(exported.value().size()));
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

    auto& slot = slot_for_side(session, *session.drop_target);
    clear_loaded_slot(slot);
    if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        session.current_error = std::string{"File is too large:\n"} + file_path.string();
        session.open_error_popup = true;
        return false;
    }

    slot.file_path = file_path.string();
    slot.map_name = file_path.filename().string();
    slot.owned_data = std::move(bytes);
    slot.map_type = map_type;

    if (slot.map_type == MapFileKind::binary) {
        parse_binary_map_for_gui(slot);
    } else if (slot.map_type == MapFileKind::text) {
        const auto text = std::string_view{
            reinterpret_cast<const char*>(slot.owned_data.data()),
            slot.owned_data.size(),
        };
        auto parsed = parse_text_map(text);
        if (!parsed) {
            slot.parse_error = parsed.error().message;
        } else {
            slot.parsed_text = parsed.value();
            slot.header_size = static_cast<int>(slot.parsed_text->header.size);
            slot.elevations = slot.parsed_text->elevations;
        }
    }

    update_loaded_map_labels(session, *session.drop_target);
    session.drop_target = std::nullopt;
    return true;
}

} // namespace qmap
