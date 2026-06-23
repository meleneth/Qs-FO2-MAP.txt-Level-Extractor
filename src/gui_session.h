#pragma once

#include "map_structs.h"
#include "text_map_export.h"

#include <array>
#include <optional>
#include <string>

namespace qmap {

inline constexpr int gui_export_path_size = 4096;

struct GuiSession {
    map_lvls left;
    map_lvls right;
    std::array<std::string, elevation_count> left_labels = {"Level 1", "Level 2", "Level 3"};
    std::array<std::string, elevation_count> right_labels = {"Level 1", "Level 2", "Level 3"};
    std::array<std::string, elevation_count> output_labels = {"empty", "##1", "##2"};
    std::array<std::optional<ElevationSource>, elevation_count> output_selection = {};
    std::string left_head = "empty##1";
    std::string middle_head = "empty##2";
    std::string right_head = "empty##3";
    int header = -1;
    char export_path[gui_export_path_size] = "/path/to/some/folder/with/long/mapname.txt";
    std::string current_error;
    bool open_error_popup = false;
};

std::array<std::string, elevation_count>& labels_for_side(GuiSession& session, MapSide side);
const std::array<std::string, elevation_count>& labels_for_side(const GuiSession& session, MapSide side);

void reset_output_selection(GuiSession& session);
void update_loaded_map_labels(GuiSession& session, const map_lvls& map, MapSide side);
void select_output_elevation(
    GuiSession& session,
    int destination,
    const map_lvls& source_map,
    const std::string& source_label,
    MapSide side,
    int source_elevation
);
void clear_output_elevation(GuiSession& session, int destination);
void choose_output_header(GuiSession& session, MapSide side);
void clear_output_header(GuiSession& session);
TextMapExportPlan make_text_export_plan(const GuiSession& session);

} // namespace qmap
