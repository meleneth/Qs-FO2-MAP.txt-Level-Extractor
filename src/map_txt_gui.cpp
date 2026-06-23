#include <imgui_internal.h>
#include "binary_map_parser.h"
#include "map_txt_gui.h"
#include "map_txt_parser.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

bool is_hovering     = false;
int list_box         = -1;
constexpr int error_text_length = 256;
char error_text[error_text_length] = {};
bool open_err_popup = false;

namespace {

constexpr int left_column = 0;
constexpr int middle_column = 1;
constexpr int right_column = 2;
constexpr int path_size = 4096;

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
    for (int elevation = 0; elevation < qmap::binary_map_elevation_count; ++elevation) {
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

map_lvls map_L;
map_lvls map_R;
char label_M[3][16] = {"empty", "##1", "##2"};
std::array<std::optional<qmap::ElevationSource>, qmap::elevation_count> output_selection = {};
char head_L[NAME_LENGTH] = {"empty##1"};
char head_M[NAME_LENGTH] = {"empty##2"};
char head_R[NAME_LENGTH] = {"empty##3"};

void reset_output_labels()
{
    output_selection = {};
    snprintf(label_M[0], NAME_LENGTH, "empty");
    snprintf(label_M[1], NAME_LENGTH, "##1");
    snprintf(label_M[2], NAME_LENGTH, "##2");
}

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

void update_labels(map_lvls* map, int list_box)
{
    if (list_box == -1) {
        return;
    }

    snprintf((list_box == 0) ? head_L : head_R, NAME_LENGTH, "%s", map->map_name);
    reset_output_labels();

    for (size_t i = 0; i < 3; i++) {
        if (map->level[i]) {
            snprintf(map->label[i], NAME_LENGTH, "%d:%s", i, map->map_name);
        } else {
            snprintf(map->label[i], NAME_LENGTH, "empty");
        }
    }
}


void file_drop_callback(const char* full_path)
{
    // not hovering over one of the boxes
    if (list_box == -1) {
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
        snprintf(error_text, error_text_length,
        "Wrong file type.\n"
        "Should be Fallout 2 'map.txt'.\n"
        "You can export a single map.txt\n"
        "from the Fallout 2 Mapper\n"
        "by opening the map you want\n"
        "to export and pressing 'Alt + P'."
        );
        open_err_popup = true;
        return;
    }

    map_lvls* map_ptr = nullptr;
    if (list_box == 0) {
        map_ptr   = &map_L;
    } else
    if (list_box == 1) {
        map_ptr   = &map_R;
    }

    std::vector<uint8_t> bytes;
    if (!load_file_bytes(file_path, bytes)) {
        snprintf(error_text, error_text_length, "Unable to read file:\n%s", full_path);
        open_err_popup = true;
        return;
    }

    clear_loaded_map(*map_ptr);
    if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        snprintf(error_text, error_text_length, "File is too large:\n%s", full_path);
        open_err_popup = true;
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
    update_labels(map_ptr, list_box);
    //QTODO: is this necessary? why did I mark it in the debugger?
    list_box = -1;
}

void drag_file(ImVec2 pos)
{
    ImGui::TeleportMousePos(pos);
    ImGuiIO& io = ImGui::GetIO();
    io.MouseDown[0] = true;

    is_hovering = true;
}
void drag_dropped()
{
    //QTODO: some cleanup might be necessary here
    //       this seems to be called multiple times from multiple places
    is_hovering = false;

    ImGuiIO& io = ImGui::GetIO();
    io.MouseDown[0] = false;
    // list_box = -1;
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
    if (is_hovering) {
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

qmap::TextMapExportPlan make_text_export_plan(int header)
{
    qmap::TextMapExportPlan plan;
    if (header == left_column) {
        plan.header_side = qmap::MapSide::left;
    } else if (header == 1) {
        plan.header_side = qmap::MapSide::right;
    } else {
        plan.header_side = std::nullopt;
    }
    plan.elevations = output_selection;
    return plan;
}

void select_output_elevation(
    int destination,
    const map_lvls& source_map,
    qmap::MapSide side,
    int source_elevation
)
{
    if (destination < 0 || destination >= qmap::elevation_count
        || source_elevation < 0 || source_elevation >= qmap::elevation_count
        || !source_map.level[source_elevation]) {
        return;
    }

    snprintf(label_M[destination], NAME_LENGTH, "%s", source_map.label[source_elevation]);
    output_selection[destination] = qmap::ElevationSource{side, source_elevation};
}

void clear_output_elevation(int destination)
{
    if (destination < 0 || destination >= qmap::elevation_count) {
        return;
    }

    snprintf(label_M[destination], NAME_LENGTH, "##%d", destination);
    output_selection[destination] = std::nullopt;
}

void export_map(int header, char* path_buff)
{
    if (map_L.data == nullptr && map_R.data == nullptr) {
        return;
    }
    if (map_L.map_type == qmap::MapFileKind::empty && map_R.map_type == qmap::MapFileKind::empty) {
        return;
    }

    // .map file extension for both maps or one .map and one empty
    if ((map_L.map_type == qmap::MapFileKind::binary || map_L.map_type == qmap::MapFileKind::empty)
    &&  (map_R.map_type == qmap::MapFileKind::binary || map_R.map_type == qmap::MapFileKind::empty)) {
        open_err_popup = true;
        snprintf(
            error_text,
            error_text_length,
            ".MAP export is not implemented yet.\n"
            "The file can be parsed, but binary export\n"
            "is disabled until the full format is modeled."
        );
    } else
    // .txt file extension for both maps or one .txt and one empty
    if ((map_L.map_type == qmap::MapFileKind::text || map_L.map_type == qmap::MapFileKind::empty)
    &&  (map_R.map_type == qmap::MapFileKind::text || map_R.map_type == qmap::MapFileKind::empty)) {
        export_map_txt(make_text_export_plan(header), &map_L, &map_R, path_buff);
    } else {
        open_err_popup = true;
        snprintf(error_text, error_text_length,
            "Sorry, can't mix .MAP and .TXT yet.\n"
            "It's just a pain in the butt to\n"
            "combine these two filetypes,\n"
            "so I'm leaving this out for now.\n"
            "Let me know if you want this!"
        );
    }
}


void error_popup()
{
    ImGui::Text("%s", error_text);

    if (ImGui::Button("Close")) {
        ImGui::CloseCurrentPopup();
    }
}
void overwrite_popup(int header, char* path_buff)
{
    ImGui::Text("Don't save over the original files\n"
                "for now, I'm not sure they would\n"
                "be recoverable.");
    ImGui::Text("File already exists, overwrite?");
    if (ImGui::Button("Overwrite")) {
        //QTODO: this needs to work for both types
        export_map(header, path_buff);
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
    static int header = -1;
    static char path_buff[path_size] = "/path/to/some/folder/with/long/mapname.txt";

    ImGui::Text("Map Names:");
    show_map_status("Left", map_L);
    show_map_status("Right", map_R);


    ImVec2 posA = ImGui::GetCursorPos();
    if (ImGui::Button(head_L, ImVec2{size.x,0})) {
        if (map_L.data) {
            snprintf(head_M, NAME_LENGTH, "%s##", map_L.map_name);
            snprintf(path_buff, path_size, "%s.Q.txt", map_L.file_str);
            header = 0;
        } else {
            snprintf(head_M, NAME_LENGTH, "HeaderL##");
        }
    }
    ImGui::SetCursorPos(ImVec2{posA.x+size.x   + 60, posA.y});
    if (ImGui::Button(head_M, ImVec2{size.x,0})) {
        header = -1;
        snprintf(head_M, NAME_LENGTH, "empty");
    }
    ImGui::SetCursorPos(ImVec2{posA.x+size.x*2 + 120, posA.y});
    if (ImGui::Button(head_R, ImVec2{size.x,0})) {
        if (map_R.data) {
            snprintf(head_M, NAME_LENGTH, "%s##", map_R.map_name);
            snprintf(path_buff, path_size, "%s.Q.txt", map_R.file_str);
            header = 1;
        } else {
            snprintf(head_M, NAME_LENGTH, "HeaderR##");
        }
    }


    ImVec2 posB = ImGui::GetCursorPos();
    static int selection[3] = { 0, 1, 2 };
    const char* left_labels[] = {map_L.label[0], map_L.label[1], map_L.label[2]};
    const char* right_labels[] = {map_R.label[0], map_R.label[1], map_R.label[2]};


    // left third
    ImGui::ListBox("##L", &selection[left_column], left_labels, IM_COUNTOF(left_labels));
    if (hover_box()) {
        list_box = 0;
    }

    ImGui::SetCursorPos(ImVec2{posB.x+size.x   +  5, posB.y});
    if (ImGui::Button(">##L->M", ImVec2{50,ImGui::GetItemRectSize().y})) {
        select_output_elevation(
            selection[middle_column],
            map_L,
            qmap::MapSide::left,
            selection[left_column]
        );
    }

    // middle third - output listbox
    // this uses listbox internals so I can clear an entry on double-click
    char* label_ptr_M[] = {label_M[0],label_M[1],label_M[2]};
    ImGui::SetCursorPos(ImVec2{posB.x+size.x+20   + 40, posB.y});
    if (ImGui::BeginListBox("##M", {size.x,ImGui::GetItemRectSize().y}))
    {
        for (int n = 0; n < IM_COUNTOF(label_ptr_M); n++)
        {
            const bool item_selected = (n == selection[middle_column]);
            if (ImGui::Selectable(label_M[n], item_selected))
                selection[middle_column] = n;

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                clear_output_elevation(selection[middle_column]);
            }
        }
        ImGui::EndListBox();
    }

    ImGui::SetCursorPos(ImVec2{posB.x+size.x*2+20 + 45, posB.y});
    if (ImGui::Button("<##R->M", ImVec2{50,ImGui::GetItemRectSize().y})) {
        select_output_elevation(
            selection[middle_column],
            map_R,
            qmap::MapSide::right,
            selection[right_column]
        );
    }

    // right third
    ImGui::SetCursorPos(ImVec2{posB.x+size.x*2 + 120, posB.y});
    ImGui::ListBox("##R", &selection[right_column], right_labels, IM_COUNTOF(right_labels));
    if (hover_box()) {
        list_box = 1;
    }

    ImGui::PopItemWidth();

    if (open_err_popup) {
        ImGui::OpenPopup("Error");
        open_err_popup = false;
    }

    if (ImGui::BeginPopup("Error")) {
        error_popup();
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("Overwrite?")) {
        overwrite_popup(header, path_buff);
        ImGui::EndPopup();
    }

    if (ImGui::Button("Export")) {
        if (std::filesystem::exists(path_buff)) {
            ImGui::OpenPopup("Overwrite?");
        } else {
            export_map(header, path_buff);
        }
    }
    if (header == -1) {
        ImGui::SameLine();
        ImGui::Text("Pick a header first");
    }

    ImGui::InputText("Path", path_buff, IM_COUNTOF(path_buff), ImGuiInputTextFlags_ElideLeft);


    return false;
}
