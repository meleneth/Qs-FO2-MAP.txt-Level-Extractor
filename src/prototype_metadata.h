#pragma once

#include "binary_map_parser.h"
#include "qmap_result.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace qmap {

struct PrototypeListEntry {
    int index = 0;
    std::string filename;
};

struct PrototypeRecord {
    std::int32_t pid = 0;
    BinaryObjectType object_type = BinaryObjectType::item;
    std::int32_t subtype = 0;
};

// Numeric subtype values are stored in item/scenery .pro records at offset
// 0x20; these names mirror Fallout engine prototype subtype enums.
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

std::optional<std::size_t> object_tail_size_from_prototype(
    const PrototypeRecord& prototype,
    int map_version
);

bool prototype_is_elevation_linking_scenery(const PrototypeRecord& prototype);

class PrototypeDatabase {
public:
    void add(PrototypeRecord record);
    std::optional<PrototypeRecord> find(std::int32_t pid) const;
    std::size_t size() const;

private:
    std::unordered_map<std::int32_t, PrototypeRecord> records_;
};

Result<std::vector<PrototypeListEntry>> parse_prototype_list(std::string_view text);

Result<PrototypeRecord> parse_prototype_record(
    std::span<const std::byte> bytes,
    BinaryObjectType object_type
);

Result<PrototypeDatabase> load_prototype_database(const std::filesystem::path& proto_root);

} // namespace qmap
