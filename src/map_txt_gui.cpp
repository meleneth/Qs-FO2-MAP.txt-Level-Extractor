#include <imgui_internal.h>
#include "binary_map_parser.h"
#include "io_Platform.h"
#include "map_txt_gui.h"
#include "map_txt_parser.h"

#include <cstddef>
#include <span>

bool is_hovering     = false;
int list_box         = -1;
#define ERR_TXT_LEN     (256)
char error_text[ERR_TXT_LEN] = {};
bool open_err_popup = false;

namespace {

constexpr int left_column = 0;
constexpr int middle_column = 1;
constexpr int right_column = 2;

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
char head_L[NAME_LENGTH] = {"empty##1"};
char head_M[NAME_LENGTH] = {"empty##2"};
char head_R[NAME_LENGTH] = {"empty##3"};

const char* map_type_name(int map_type)
{
    switch (map_type) {
    case MAP_TXT:
        return ".txt";
    case MAP_MAP:
        return ".map";
    default:
        return "empty";
    }
}

bool map_parse_succeeded(const map_lvls& map)
{
    if (!map.data || map.map_type == EMPTY) {
        return false;
    }
    if (map.map_type == MAP_TXT) {
        return map.scripts != nullptr && map.objects != nullptr;
    }
    if (map.map_type == MAP_MAP) {
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
    if (map.map_type == MAP_MAP) {
        ImGui::SameLine();
        ImGui::TextDisabled("export not implemented");
    }
}

void update_labels(map_lvls* map, int list_box)
{
    if (list_box == -1) {
        return;
    }

    snprintf((list_box == 0) ? head_L : head_R, NAME_LENGTH, "%s", map->map_name);
    memset(label_M,0,sizeof(label_M));
    strncpy(label_M[0],"empty",sizeof("empty"));
    strncpy(label_M[1],"##1",  sizeof("##1"));
    strncpy(label_M[2],"##2",  sizeof("##2"));

    for (size_t i = 0; i < 3; i++) {
        map->label_ptr[i] = map->label[i];
        if (map->level[i]) {
            snprintf(map->label_ptr[i], NAME_LENGTH, "%d:%s", i, map->map_name);
        } else {
            snprintf(map->label_ptr[i], NAME_LENGTH, "empty");
        }
    }
}


void file_drop_callback(const char* full_path)
{
    // not hovering over one of the boxes
    if (list_box == -1) {
        return;
    }
    // make sure file type is .txt
    char* ext = io_get_file_extension(full_path);

    int map_type = EMPTY;
    if (io_strncasecmp(ext, "txt", 3) == 0) {
        map_type = MAP_TXT;
    } else
    if (io_strncasecmp(ext, "map", 3) == 0) {
        map_type = MAP_MAP;
    } else {
        snprintf(error_text, ERR_TXT_LEN,
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

    char* file_path   = NULL;
    int len = strlen(full_path) + 1;
    file_path = (char*)malloc(len);
    memcpy(file_path, full_path, len);


    map_lvls* map_ptr = NULL;
    if (list_box == 0) {
        map_ptr   = &map_L;
    } else
    if (list_box == 1) {
        map_ptr   = &map_R;
    }

    if (map_ptr->data) {
        free(map_ptr->data);
        free(map_ptr->file_str);
        memset(map_ptr,0,sizeof(*map_ptr));
    }
    file_info* file   = io_load_file(file_path);
    map_ptr->file_str = file_path;
    map_ptr->file_siz = file->size;
    map_ptr->data     = file->data;
    free(file);

    map_ptr->map_name = io_get_filename_from_path(file_path);
    map_ptr->map_type = map_type;


    if (map_ptr->map_type == MAP_MAP) {
        parse_binary_map_for_gui(*map_ptr);
    } else
    if (map_ptr->map_type == MAP_TXT) {
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

void export_map(char** label_ptr_M, int header, char* path_buff)
{
    if (map_L.data == nullptr && map_R.data == nullptr) {
        return;
    }
    if (map_L.map_type == EMPTY && map_R.map_type == EMPTY) {
        return;
    }

    // .map file extension for both maps or one .map and one empty
    if ((map_L.map_type == MAP_MAP || map_L.map_type == EMPTY)
    &&  (map_R.map_type == MAP_MAP || map_R.map_type == EMPTY)) {
        open_err_popup = true;
        snprintf(
            error_text,
            ERR_TXT_LEN,
            ".MAP export is not implemented yet.\n"
            "The file can be parsed, but binary export\n"
            "is disabled until the full format is modeled."
        );
    } else
    // .txt file extension for both maps or one .txt and one empty
    if ((map_L.map_type == MAP_TXT || map_L.map_type == EMPTY)
    &&  (map_R.map_type == MAP_TXT || map_R.map_type == EMPTY)) {
        export_map_txt(label_ptr_M, &map_L, &map_R, header, path_buff);
    } else {
        open_err_popup = true;
        snprintf(error_text, ERR_TXT_LEN,
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
void overwrite_popup(char** label_ptr_M, int header, char* path_buff)
{
    ImGui::Text("Don't save over the original files\n"
                "for now, I'm not sure they would\n"
                "be recoverable.");
    ImGui::Text("File already exists, overwrite?");
    if (ImGui::Button("Overwrite")) {
        //QTODO: this needs to work for both types
        export_map(label_ptr_M, header, path_buff);
        // export_map_txt(label_ptr_M, &map_L, &map_R, header, path_buff);
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
    #define PATH_SIZE           (MAX_PATH)
    static char path_buff[PATH_SIZE] = "/path/to/some/folder/with/long/mapname.txt";

    ImGui::Text("Map Names:");
    show_map_status("Left", map_L);
    show_map_status("Right", map_R);


    ImVec2 posA = ImGui::GetCursorPos();
    if (ImGui::Button(head_L, ImVec2{size.x,0})) {
        if (map_L.data) {
            snprintf(head_M, NAME_LENGTH, "%s##", map_L.map_name);
            snprintf(path_buff, PATH_SIZE, "%s.Q.txt", map_L.file_str);
            header = 0;
        } else {
            strncpy(head_M,"HeaderL##",sizeof("HeaderL##"));
        }
    }
    ImGui::SetCursorPos(ImVec2{posA.x+size.x   + 60, posA.y});
    if (ImGui::Button(head_M, ImVec2{size.x,0})) {
        header = -1;
        strncpy(head_M,"empty",sizeof("empty"));
    }
    ImGui::SetCursorPos(ImVec2{posA.x+size.x*2 + 120, posA.y});
    if (ImGui::Button(head_R, ImVec2{size.x,0})) {
        if (map_R.data) {
            snprintf(head_M, NAME_LENGTH, "%s##", map_R.map_name);
            snprintf(path_buff, PATH_SIZE, "%s.Q.txt", map_R.file_str);
            header = 1;
        } else {
            strncpy(head_M,"HeaderR##",sizeof("HeaderR##"));
        }
    }


    ImVec2 posB = ImGui::GetCursorPos();
    static int selection[3] = { 0, 1, 2 };


    // left third
    ImGui::ListBox("##L", &selection[left_column], map_L.label_ptr, IM_COUNTOF(map_L.label_ptr));
    if (hover_box()) {
        list_box = 0;
    }

    ImGui::SetCursorPos(ImVec2{posB.x+size.x   +  5, posB.y});
    if (ImGui::Button(">##L->M", ImVec2{50,ImGui::GetItemRectSize().y})) {
        // replace middle selection with selection on left
        strncpy(label_M[selection[middle_column]],map_L.label_ptr[selection[left_column]],NAME_LENGTH);
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
                snprintf(label_M[selection[middle_column]], NAME_LENGTH, "##%d", n);
            }
        }
        ImGui::EndListBox();
    }

    ImGui::SetCursorPos(ImVec2{posB.x+size.x*2+20 + 45, posB.y});
    if (ImGui::Button("<##R->M", ImVec2{50,ImGui::GetItemRectSize().y})) {
        // replace middle selection with selection on right
        strncpy(label_M[selection[middle_column]],map_R.label_ptr[selection[right_column]],NAME_LENGTH);
    }

    // right third
    ImGui::SetCursorPos(ImVec2{posB.x+size.x*2 + 120, posB.y});
    ImGui::ListBox("##R", &selection[right_column], map_R.label_ptr, IM_COUNTOF(map_R.label_ptr));
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
        overwrite_popup(label_ptr_M, header, path_buff);
        ImGui::EndPopup();
    }

    if (ImGui::Button("Export")) {
        if (io_file_exists(path_buff)) {
            ImGui::OpenPopup("Overwrite?");
        } else {
            export_map(label_ptr_M, header, path_buff);
        }
    }
    if (header == -1) {
        ImGui::SameLine();
        ImGui::Text("Pick a header first");
    }

    ImGui::InputText("Path", path_buff, IM_COUNTOF(path_buff), ImGuiInputTextFlags_ElideLeft);


    return false;
}
