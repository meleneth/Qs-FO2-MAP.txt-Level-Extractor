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

std::optional<std::size_t> object_tail_size_from_prototype(
    const PrototypeRecord& prototype,
    int map_version
);

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
