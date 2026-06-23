#include <imgui_internal.h>
#include "gui_session.h"
#include "map_txt_gui.h"

#include <filesystem>

namespace {

constexpr int left_column = 0;
constexpr int middle_column = 1;
constexpr int right_column = 2;

} // namespace

qmap::GuiSession session;

void show_map_status(const char* side, const map_lvls& map)
{
    if (!map.data) {
        ImGui::Text("%s: empty", side);
        return;
    }

    ImGui::Text(
        "%s: %s %s",
        side,
        qmap::map_parse_succeeded(map) ? "file parsed" : "parse failed",
        qmap::map_type_name(map.map_type)
    );
    if (map.map_type == qmap::MapFileKind::binary) {
        ImGui::SameLine();
        ImGui::TextDisabled("export not implemented");
    }
    if (!qmap::map_parse_succeeded(map) && !map.parse_error.empty()) {
        ImGui::TextDisabled("%s: %s", side, map.parse_error.c_str());
    }
}

void file_drop_callback(const char* full_path)
{
    qmap::load_dropped_file(session, std::filesystem::path{full_path});
}

void drag_file(ImVec2 pos)
{
    ImGui::TeleportMousePos(pos);
    ImGuiIO& io = ImGui::GetIO();
    io.MouseDown[0] = true;

    session.is_hovering_drop_target = true;
}
void drag_dropped()
{
    //QTODO: some cleanup might be necessary here
    //       this seems to be called multiple times from multiple places
    session.is_hovering_drop_target = false;

    ImGuiIO& io = ImGui::GetIO();
    io.MouseDown[0] = false;
    // session.drop_target = std::nullopt;
}

// kind of dumb, but...
// if mouse enters the window but is not over previous item
// then draw highlight border around the previous item
// (in this case it's always one of the two map lists)
// and return false
// if mouse enters the boundary of the previous item
// then return true
// the return value is used to determine which list item
// to use when storing the map.txt information
bool hover_box()
{
    if (session.is_hovering_drop_target) {
        ImVec2 list_min  = ImGui::GetItemRectMin();
        ImVec2 list_max  = ImGui::GetItemRectMax();
        ImRect rect = ImRect{list_min.x,list_min.y,list_max.x,list_max.y};

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRect(list_min, list_max, ImColor(232,232,2,255), 0, 0, 5.0f);

        ImVec2 m_pos = ImGui::GetMousePos();
        if ((m_pos.x > list_min.x)
        &&  (m_pos.y > list_min.y)
        &&  (m_pos.x < list_max.x)
        &&  (m_pos.y < list_max.y)) {
            return true;
        }
    }
    return false;
}

void error_popup()
{
    ImGui::Text("%s", session.current_error.c_str());

    if (ImGui::Button("Close")) {
        ImGui::CloseCurrentPopup();
    }
}
void overwrite_popup(char* path_buff)
{
    ImGui::Text("Don't save over the original files\n"
                "for now, I'm not sure they would\n"
                "be recoverable.");
    ImGui::Text("File already exists, overwrite?");
    if (ImGui::Button("Overwrite")) {
        qmap::export_session_map(session, path_buff);
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
    }
}

// gui interface for the whole map_txt editor
// divided into thirds, a left map, a right map,
// and the new map in the middle
bool map_txt_gui()
{
    ImVec2 size = ImGui::CalcTextSize("AAAAAAAAA");
    ImGui::PushItemWidth(size.x);

    ImGui::Text("Map Names:");
    show_map_status("Left", session.left);
    show_map_status("Right", session.right);


    ImVec2 posA = ImGui::GetCursorPos();
    if (ImGui::Button(session.left_head.c_str(), ImVec2{size.x,0})) {
        qmap::choose_output_header(session, qmap::MapSide::left);
    }
    ImGui::SetCursorPos(ImVec2{posA.x+size.x   + 60, posA.y});
    if (ImGui::Button(session.middle_head.c_str(), ImVec2{size.x,0})) {
        qmap::clear_output_header(session);
    }
    ImGui::SetCursorPos(ImVec2{posA.x+size.x*2 + 120, posA.y});
    if (ImGui::Button(session.right_head.c_str(), ImVec2{size.x,0})) {
        qmap::choose_output_header(session, qmap::MapSide::right);
    }


    ImVec2 posB = ImGui::GetCursorPos();
    const char* left_labels[] = {
        session.left_labels[0].c_str(),
        session.left_labels[1].c_str(),
        session.left_labels[2].c_str(),
    };
    const char* right_labels[] = {
        session.right_labels[0].c_str(),
        session.right_labels[1].c_str(),
        session.right_labels[2].c_str(),
    };


    // left third
    ImGui::ListBox("##L", &session.selected_elevations[left_column], left_labels, IM_COUNTOF(left_labels));
    if (hover_box()) {
        session.drop_target = qmap::MapSide::left;
    }

    ImGui::SetCursorPos(ImVec2{posB.x+size.x   +  5, posB.y});
    if (ImGui::Button(">##L->M", ImVec2{50,ImGui::GetItemRectSize().y})) {
        qmap::select_output_elevation(
            session,
            session.selected_elevations[middle_column],
            session.left,
            session.left_labels[session.selected_elevations[left_column]],
            qmap::MapSide::left,
            session.selected_elevations[left_column]
        );
    }

    // middle third - output listbox
    // this uses listbox internals so I can clear an entry on double-click
    const char* output_labels[] = {
        session.output_labels[0].c_str(),
        session.output_labels[1].c_str(),
        session.output_labels[2].c_str(),
    };
    ImGui::SetCursorPos(ImVec2{posB.x+size.x+20   + 40, posB.y});
    if (ImGui::BeginListBox("##M", {size.x,ImGui::GetItemRectSize().y}))
    {
        for (int n = 0; n < IM_COUNTOF(output_labels); n++)
        {
            const bool item_selected = (n == session.selected_elevations[middle_column]);
            if (ImGui::Selectable(output_labels[n], item_selected))
                session.selected_elevations[middle_column] = n;

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                qmap::clear_output_elevation(session, session.selected_elevations[middle_column]);
            }
        }
        ImGui::EndListBox();
    }

    ImGui::SetCursorPos(ImVec2{posB.x+size.x*2+20 + 45, posB.y});
    if (ImGui::Button("<##R->M", ImVec2{50,ImGui::GetItemRectSize().y})) {
        qmap::select_output_elevation(
            session,
            session.selected_elevations[middle_column],
            session.right,
            session.right_labels[session.selected_elevations[right_column]],
            qmap::MapSide::right,
            session.selected_elevations[right_column]
        );
    }

    // right third
    ImGui::SetCursorPos(ImVec2{posB.x+size.x*2 + 120, posB.y});
    ImGui::ListBox("##R", &session.selected_elevations[right_column], right_labels, IM_COUNTOF(right_labels));
    if (hover_box()) {
        session.drop_target = qmap::MapSide::right;
    }

    ImGui::PopItemWidth();

    if (session.open_error_popup) {
        ImGui::OpenPopup("Error");
        session.open_error_popup = false;
    }

    if (ImGui::BeginPopup("Error")) {
        error_popup();
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("Overwrite?")) {
        overwrite_popup(session.export_path);
        ImGui::EndPopup();
    }

    if (ImGui::Button("Export")) {
        if (std::filesystem::exists(session.export_path)) {
            ImGui::OpenPopup("Overwrite?");
        } else {
            qmap::export_session_map(session, session.export_path);
        }
    }
    if (session.header == -1) {
        ImGui::SameLine();
        ImGui::Text("Pick a header first");
    }

    ImGui::InputText("Path", session.export_path, IM_COUNTOF(session.export_path), ImGuiInputTextFlags_ElideLeft);


    return false;
}
