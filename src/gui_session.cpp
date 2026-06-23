#include "gui_session.h"

#include <cstdio>

namespace qmap {

std::array<std::string, elevation_count>& labels_for_side(GuiSession& session, MapSide side)
{
    return side == MapSide::left ? session.left_labels : session.right_labels;
}

const std::array<std::string, elevation_count>& labels_for_side(const GuiSession& session, MapSide side)
{
    return side == MapSide::left ? session.left_labels : session.right_labels;
}

void reset_output_selection(GuiSession& session)
{
    session.output_selection = {};
    session.output_labels = {"empty", "##1", "##2"};
}

void update_loaded_map_labels(GuiSession& session, const map_lvls& map, MapSide side)
{
    if (side == MapSide::left) {
        session.left_head = map.map_name;
    } else {
        session.right_head = map.map_name;
    }

    reset_output_selection(session);
    auto& labels = labels_for_side(session, side);
    for (int elevation = 0; elevation < elevation_count; ++elevation) {
        labels[elevation] = map.level[elevation]
            ? std::to_string(elevation) + ":" + map.map_name
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
        || !source_map.level[source_elevation]) {
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
    const map_lvls& map = side == MapSide::left ? session.left : session.right;
    if (!map.data) {
        session.middle_head = side == MapSide::left ? "HeaderL##" : "HeaderR##";
        return;
    }

    session.middle_head = std::string{map.map_name} + "##";
    std::snprintf(session.export_path, gui_export_path_size, "%s.Q.txt", map.file_str);
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
    if (session.left.data == nullptr && session.right.data == nullptr) {
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

} // namespace qmap
