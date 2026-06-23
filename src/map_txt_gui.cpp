#include <imgui_internal.h>
#include "binary_map_parser.h"
#include "gui_session.h"
#include "map_txt_gui.h"
#include "map_txt_parser.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace {

constexpr int left_column = 0;
constexpr int middle_column = 1;
constexpr int right_column = 2;

void clear_loaded_map(map_lvls& map)
{
    map.map_type = qmap::MapFileKind::empty;
    map.file_path_storage.clear();
    map.map_name_storage.clear();
    map.parse_error.clear();
    map.owned_data.clear();
    map.file_str = nullptr;
    map.file_siz = 0;
    map.map_name = nullptr;
    map.data = nullptr;
    map.header_size = 0;
    for (int elevation = 0; elevation < qmap::elevation_count; ++elevation) {
        map.lvl_sizes[elevation] = 0;
        map.level[elevation] = nullptr;
    }
    map.scripts = nullptr;
    map.objects = nullptr;
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
    for (int level = 0; level < qmap::binary_map_elevation_count; ++level) {
        map.level[level] = nullptr;
        map.lvl_sizes[level] = 0;
    }

    if (!map.data || map.file_siz <= 0) {
        return;
    }

    const auto bytes = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(map.data),
        static_cast<std::size_t>(map.file_siz)
    );
    const auto header = qmap::parse_binary_map_header(bytes);
    if (!header) {
        map.parse_error = header.error().message;
        return;
    }

    map.header_size = static_cast<int>(qmap::binary_map_header_size);
    for (int level = 0; level < qmap::binary_map_elevation_count; ++level) {
        if (header.value().has_elevation(level)) {
            map.level[level] = map.label[level];
        }
    }
}

} // namespace

qmap::GuiSession session;

const char* map_type_name(qmap::MapFileKind map_type)
{
    switch (map_type) {
    case qmap::MapFileKind::text:
        return ".txt";
    case qmap::MapFileKind::binary:
        return ".map";
    case qmap::MapFileKind::empty:
        return "empty";
    }

    return "empty";
}

bool map_parse_succeeded(const map_lvls& map)
{
    if (!map.data || map.map_type == qmap::MapFileKind::empty) {
        return false;
    }
    if (map.map_type == qmap::MapFileKind::text) {
        return map.scripts != nullptr && map.objects != nullptr;
    }
    if (map.map_type == qmap::MapFileKind::binary) {
        return map.header_size == static_cast<int>(qmap::binary_map_header_size);
    }

    return false;
}

void show_map_status(const char* side, const map_lvls& map)
{
    if (!map.data) {
        ImGui::Text("%s: empty", side);
        return;
    }

    ImGui::Text(
        "%s: %s %s",
        side,
        map_parse_succeeded(map) ? "file parsed" : "parse failed",
        map_type_name(map.map_type)
    );
    if (map.map_type == qmap::MapFileKind::binary) {
        ImGui::SameLine();
        ImGui::TextDisabled("export not implemented");
    }
    if (!map_parse_succeeded(map) && !map.parse_error.empty()) {
        ImGui::TextDisabled("%s: %s", side, map.parse_error.c_str());
    }
}

void update_labels(map_lvls* map, int target_list_box)
{
    if (target_list_box == -1) {
        return;
    }

    const auto side = target_list_box == left_column ? qmap::MapSide::left : qmap::MapSide::right;
    qmap::update_loaded_map_labels(session, *map, side);
}


void file_drop_callback(const char* full_path)
{
    // not hovering over one of the boxes
    if (session.drop_target == -1) {
        return;
    }
    const std::filesystem::path file_path(full_path);
    const std::string extension = lower_extension(file_path);

    qmap::MapFileKind map_type = qmap::MapFileKind::empty;
    if (extension == ".txt") {
        map_type = qmap::MapFileKind::text;
    } else
    if (extension == ".map") {
        map_type = qmap::MapFileKind::binary;
    } else {
        session.current_error =
            "Wrong file type.\n"
            "Should be Fallout 2 'map.txt'.\n"
            "You can export a single map.txt\n"
            "from the Fallout 2 Mapper\n"
            "by opening the map you want\n"
            "to export and pressing 'Alt + P'.";
        session.open_error_popup = true;
        return;
    }

    map_lvls* map_ptr = nullptr;
    if (session.drop_target == left_column) {
        map_ptr   = &session.left;
    } else
    if (session.drop_target == right_column) {
        map_ptr   = &session.right;
    }
    if (!map_ptr) {
        return;
    }

    std::vector<uint8_t> bytes;
    if (!load_file_bytes(file_path, bytes)) {
        session.current_error = std::string{"Unable to read file:\n"} + full_path;
        session.open_error_popup = true;
        return;
    }

    clear_loaded_map(*map_ptr);
    if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        session.current_error = std::string{"File is too large:\n"} + full_path;
        session.open_error_popup = true;
        return;
    }
    map_ptr->file_path_storage = file_path.string();
    map_ptr->map_name_storage = file_path.filename().string();
    map_ptr->owned_data = std::move(bytes);
    map_ptr->file_str = map_ptr->file_path_storage.data();
    map_ptr->file_siz = static_cast<int>(map_ptr->owned_data.size());
    map_ptr->data = map_ptr->owned_data.data();
    map_ptr->map_name = map_ptr->map_name_storage.data();
    map_ptr->map_type = map_type;


    if (map_ptr->map_type == qmap::MapFileKind::binary) {
        parse_binary_map_for_gui(*map_ptr);
    } else
    if (map_ptr->map_type == qmap::MapFileKind::text) {
        parse_map_txt(map_ptr->data, map_ptr);
    }
    update_labels(map_ptr, session.drop_target);
    //QTODO: is this necessary? why did I mark it in the debugger?
    session.drop_target = -1;
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
    // session.drop_target = -1;
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

void export_map(char* path_buff)
{
    if (qmap::prepare_export(session) == qmap::GuiExportAction::export_text) {
        export_map_txt(qmap::make_text_export_plan(session), &session.left, &session.right, path_buff);
    }
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
        //QTODO: this needs to work for both types
        export_map(path_buff);
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
    static int selection[3] = { 0, 1, 2 };
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
    ImGui::ListBox("##L", &selection[left_column], left_labels, IM_COUNTOF(left_labels));
    if (hover_box()) {
        session.drop_target = left_column;
    }

    ImGui::SetCursorPos(ImVec2{posB.x+size.x   +  5, posB.y});
    if (ImGui::Button(">##L->M", ImVec2{50,ImGui::GetItemRectSize().y})) {
        qmap::select_output_elevation(
            session,
            selection[middle_column],
            session.left,
            session.left_labels[selection[left_column]],
            qmap::MapSide::left,
            selection[left_column]
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
            const bool item_selected = (n == selection[middle_column]);
            if (ImGui::Selectable(output_labels[n], item_selected))
                selection[middle_column] = n;

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                qmap::clear_output_elevation(session, selection[middle_column]);
            }
        }
        ImGui::EndListBox();
    }

    ImGui::SetCursorPos(ImVec2{posB.x+size.x*2+20 + 45, posB.y});
    if (ImGui::Button("<##R->M", ImVec2{50,ImGui::GetItemRectSize().y})) {
        qmap::select_output_elevation(
            session,
            selection[middle_column],
            session.right,
            session.right_labels[selection[right_column]],
            qmap::MapSide::right,
            selection[right_column]
        );
    }

    // right third
    ImGui::SetCursorPos(ImVec2{posB.x+size.x*2 + 120, posB.y});
    ImGui::ListBox("##R", &selection[right_column], right_labels, IM_COUNTOF(right_labels));
    if (hover_box()) {
        session.drop_target = right_column;
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
            export_map(session.export_path);
        }
    }
    if (session.header == -1) {
        ImGui::SameLine();
        ImGui::Text("Pick a header first");
    }

    ImGui::InputText("Path", session.export_path, IM_COUNTOF(session.export_path), ImGuiInputTextFlags_ElideLeft);


    return false;
}
