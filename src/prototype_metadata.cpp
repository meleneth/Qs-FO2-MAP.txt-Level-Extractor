#include "prototype_metadata.h"

#include <array>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>

namespace qmap {
namespace {

constexpr std::size_t common_pid_offset = 0x00;
constexpr std::size_t common_subtype_offset = 0x20;
constexpr std::size_t minimum_typed_prototype_size = common_subtype_offset + sizeof(std::int32_t);
constexpr int fallout_1_map_version = 19;
constexpr std::int32_t first_exit_grid_pid = 0x05000010;
constexpr std::int32_t last_exit_grid_pid = 0x05000017;

enum ItemPrototypeSubtype : int {
    item_armor = 0,
    item_container = 1,
    item_drug = 2,
    item_weapon = 3,
    item_ammo = 4,
    item_misc = 5,
    item_key = 6,
};

enum SceneryPrototypeSubtype : int {
    scenery_door = 0,
    scenery_stairs = 1,
    scenery_elevator = 2,
    scenery_ladder_up = 3,
    scenery_ladder_down = 4,
    scenery_generic = 5,
};

struct PrototypeKindSource {
    BinaryObjectType type;
    const char* directory;
    const char* list_filename;
};

constexpr std::array prototype_kind_sources{
    PrototypeKindSource{BinaryObjectType::item, "items", "items.lst"},
    PrototypeKindSource{BinaryObjectType::critter, "critters", "critters.lst"},
    PrototypeKindSource{BinaryObjectType::scenery, "scenery", "scenery.lst"},
    PrototypeKindSource{BinaryObjectType::wall, "walls", "walls.lst"},
    PrototypeKindSource{BinaryObjectType::tile, "tiles", "tiles.lst"},
    PrototypeKindSource{BinaryObjectType::misc, "misc", "misc.lst"},
};

std::string trim_list_line(std::string_view line)
{
    const auto comment = line.find_first_of(" \t\r\n");
    if (comment != std::string_view::npos) {
        line = line.substr(0, comment);
    }
    return std::string(line);
}

std::uint8_t to_u8(std::byte value)
{
    return static_cast<std::uint8_t>(value);
}

Result<std::int32_t> read_i32_be_at(std::span<const std::byte> bytes, std::size_t offset)
{
    if (offset + sizeof(std::int32_t) > bytes.size()) {
        return Result<std::int32_t>::fail({"prototype field read past end", offset});
    }

    const auto value =
        (static_cast<std::uint32_t>(to_u8(bytes[offset])) << 24)
        | (static_cast<std::uint32_t>(to_u8(bytes[offset + 1])) << 16)
        | (static_cast<std::uint32_t>(to_u8(bytes[offset + 2])) << 8)
        | static_cast<std::uint32_t>(to_u8(bytes[offset + 3]));
    return Result<std::int32_t>::ok(static_cast<std::int32_t>(value));
}

bool prototype_type_has_subtype(BinaryObjectType type)
{
    return type == BinaryObjectType::item || type == BinaryObjectType::scenery;
}

std::string ascii_lower(std::string_view text)
{
    std::string lowered(text);
    std::transform(
        lowered.begin(),
        lowered.end(),
        lowered.begin(),
        [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        }
    );
    return lowered;
}

std::filesystem::path resolve_child_path(const std::filesystem::path& parent, std::string_view child)
{
    const auto exact = parent / std::string(child);
    if (std::filesystem::exists(exact)) {
        return exact;
    }

    const auto wanted = ascii_lower(child);
    if (!std::filesystem::exists(parent)) {
        return exact;
    }

    for (const auto& entry : std::filesystem::directory_iterator(parent)) {
        if (ascii_lower(entry.path().filename().string()) == wanted) {
            return entry.path();
        }
    }
    return exact;
}

Result<std::string> read_text_file(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Result<std::string>::fail({"could not open prototype list", 0});
    }

    std::ostringstream output;
    output << input.rdbuf();
    return Result<std::string>::ok(output.str());
}

Result<std::vector<std::byte>> read_binary_file(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Result<std::vector<std::byte>>::fail({"could not open prototype file", 0});
    }

    std::vector<char> chars(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>()
    );
    std::vector<std::byte> bytes;
    bytes.reserve(chars.size());
    for (const auto value : chars) {
        bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(value)));
    }
    return Result<std::vector<std::byte>>::ok(std::move(bytes));
}

Result<void> load_kind(
    PrototypeDatabase& database,
    const std::filesystem::path& proto_root,
    PrototypeKindSource source
)
{
    const auto directory = resolve_child_path(proto_root, source.directory);
    const auto list_text = read_text_file(resolve_child_path(directory, source.list_filename));
    if (!list_text) {
        return Result<void>::fail(list_text.error());
    }

    auto entries = parse_prototype_list(list_text.value());
    if (!entries) {
        return Result<void>::fail(entries.error());
    }

    for (const auto& entry : entries.value()) {
        auto bytes = read_binary_file(resolve_child_path(directory, entry.filename));
        if (!bytes) {
            return Result<void>::fail(bytes.error());
        }

        auto record = parse_prototype_record(bytes.value(), source.type);
        if (!record) {
            return Result<void>::fail(record.error());
        }
        database.add(record.value());
    }

    return Result<void>::ok();
}

} // namespace

void PrototypeDatabase::add(PrototypeRecord record)
{
    records_[record.pid] = record;
}

std::optional<PrototypeRecord> PrototypeDatabase::find(std::int32_t pid) const
{
    const auto found = records_.find(pid);
    if (found == records_.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::size_t PrototypeDatabase::size() const
{
    return records_.size();
}

std::optional<std::size_t> object_tail_size_from_prototype(
    const PrototypeRecord& prototype,
    int map_version
)
{
    switch (prototype.object_type) {
    case BinaryObjectType::item:
        switch (prototype.subtype) {
        case item_weapon:
            return 8;
        case item_ammo:
        case item_misc:
        case item_key:
            return 4;
        case item_armor:
        case item_container:
        case item_drug:
            return 0;
        default:
            return std::nullopt;
        }
    case BinaryObjectType::scenery:
        switch (prototype.subtype) {
        case scenery_door:
            return 4;
        case scenery_stairs:
        case scenery_elevator:
            return 8;
        case scenery_ladder_up:
        case scenery_ladder_down:
            return map_version == fallout_1_map_version ? 4 : 8;
        case scenery_generic:
            return 0;
        default:
            return std::nullopt;
        }
    case BinaryObjectType::misc:
        if (prototype.pid >= first_exit_grid_pid && prototype.pid <= last_exit_grid_pid) {
            return 16;
        }
        return 0;
    default:
        return std::nullopt;
    }
}

bool prototype_is_elevation_linking_scenery(const PrototypeRecord& prototype)
{
    if (prototype.object_type != BinaryObjectType::scenery) {
        return false;
    }

    return prototype.subtype == scenery_stairs
        || prototype.subtype == scenery_elevator
        || prototype.subtype == scenery_ladder_up
        || prototype.subtype == scenery_ladder_down;
}

Result<std::vector<PrototypeListEntry>> parse_prototype_list(std::string_view text)
{
    std::vector<PrototypeListEntry> entries;
    int index = 1;
    while (!text.empty()) {
        auto line_end = text.find('\n');
        auto line = text.substr(0, line_end);
        if (line_end == std::string_view::npos) {
            text = {};
        } else {
            text.remove_prefix(line_end + 1);
        }

        auto filename = trim_list_line(line);
        if (!filename.empty()) {
            entries.push_back(PrototypeListEntry{index, std::move(filename)});
        }
        ++index;
    }

    return Result<std::vector<PrototypeListEntry>>::ok(std::move(entries));
}

Result<PrototypeRecord> parse_prototype_record(
    std::span<const std::byte> bytes,
    BinaryObjectType object_type
)
{
    if (prototype_type_has_subtype(object_type) && bytes.size() < minimum_typed_prototype_size) {
        return Result<PrototypeRecord>::fail({"prototype file too short for subtype", bytes.size()});
    }
    if (bytes.size() < common_pid_offset + sizeof(std::int32_t)) {
        return Result<PrototypeRecord>::fail({"prototype file too short", bytes.size()});
    }

    auto pid = read_i32_be_at(bytes, common_pid_offset);
    if (!pid) {
        return Result<PrototypeRecord>::fail(pid.error());
    }

    PrototypeRecord record;
    record.pid = pid.value();
    record.object_type = object_type;

    if (prototype_type_has_subtype(object_type)) {
        auto subtype = read_i32_be_at(bytes, common_subtype_offset);
        if (!subtype) {
            return Result<PrototypeRecord>::fail(subtype.error());
        }
        record.subtype = subtype.value();
    }

    return Result<PrototypeRecord>::ok(record);
}

Result<PrototypeDatabase> load_prototype_database(const std::filesystem::path& proto_root)
{
    PrototypeDatabase database;
    for (const auto source : prototype_kind_sources) {
        auto loaded = load_kind(database, proto_root, source);
        if (!loaded) {
            return Result<PrototypeDatabase>::fail(loaded.error());
        }
    }
    return Result<PrototypeDatabase>::ok(std::move(database));
}

} // namespace qmap
