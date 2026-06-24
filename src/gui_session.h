#pragma once

#include "binary_map_parser.h"
#include "prototype_metadata.h"
#include "text_map_export.h"

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace qmap {

inline constexpr int gui_export_path_size = 4096;

struct GuiSession {
    struct MapSlot {
        MapFileKind map_type = MapFileKind::empty;
        std::string file_path;
        std::string map_name;
        std::string parse_error;
        std::vector<uint8_t> owned_data;
        int header_size = 0;
        std::array<std::optional<Range>, elevation_count> elevations = {};
        std::optional<ParsedTextMap> parsed_text;
        std::optional<BinaryMap> parsed_binary;
        std::array<std::string, elevation_count> labels = {"Level 1", "Level 2", "Level 3"};
        std::string heading;
    };

    MapSlot left{.heading = "empty##1"};
    MapSlot right{.heading = "empty##3"};
    std::array<std::string, elevation_count> output_labels = {"empty", "##1", "##2"};
    std::array<std::optional<ElevationSource>, elevation_count> output_selection = {};
    std::array<int, elevation_count> selected_elevations = {0, 1, 2};
    std::string middle_head = "empty##2";
    int header = -1;
    char export_path[gui_export_path_size] = "/path/to/some/folder/with/long/mapname.txt";
    char proto_root_path[gui_export_path_size] = ".local_fallout2_data/proto";
    std::string current_error;
    bool open_error_popup = false;
    bool is_hovering_drop_target = false;
    std::optional<MapSide> drop_target = std::nullopt;
};

enum class GuiExportAction {
    none,
    export_text,
};

std::array<std::string, elevation_count>& labels_for_side(GuiSession& session, MapSide side);
const std::array<std::string, elevation_count>& labels_for_side(const GuiSession& session, MapSide side);

const char* map_type_name(MapFileKind map_type);
bool map_parse_succeeded(const GuiSession::MapSlot& slot);
void reset_output_selection(GuiSession& session);
void update_loaded_map_labels(GuiSession& session, MapSide side);
void select_output_elevation(
    GuiSession& session,
    int destination,
    const GuiSession::MapSlot& source_slot,
    const std::string& source_label,
    MapSide side,
    int source_elevation
);
void clear_output_elevation(GuiSession& session, int destination);
void choose_output_header(GuiSession& session, MapSide side);
void clear_output_header(GuiSession& session);
TextMapExportPlan make_text_export_plan(const GuiSession& session);
GuiExportAction prepare_export(GuiSession& session);
void export_session_map(GuiSession& session, char* path);
bool load_dropped_file(GuiSession& session, const std::filesystem::path& file_path);

} // namespace qmap
