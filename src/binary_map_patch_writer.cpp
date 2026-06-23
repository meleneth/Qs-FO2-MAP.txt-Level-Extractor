#include "binary_map_patch_writer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>

namespace qmap {

namespace {

constexpr std::size_t map_flags_offset = 40;
constexpr std::size_t map_flags_size = sizeof(std::uint32_t);
constexpr std::size_t object_count_word_size = sizeof(std::int32_t);
constexpr std::uint32_t elevation_one_tile_flag = 0x20000000u;
constexpr std::uint32_t elevation_two_tile_flag = 0x40000000u;
constexpr std::uint32_t tile_elevation_mask = elevation_one_tile_flag | elevation_two_tile_flag;
constexpr int serialized_script_block_capacity = 16;
constexpr int base_script_record_words = 16;
constexpr std::array<std::int32_t, binary_map_elevation_count> map_elevation_absent_flags = {
    0x2,
    0x4,
    0x8,
};

bool range_is_valid(std::span<const std::byte> bytes, Range range)
{
    return range.offset <= bytes.size() && range.size <= bytes.size() - range.offset;
}

std::int32_t read_i32_be(std::span<const std::byte> bytes, std::size_t offset)
{
    auto value = (std::to_integer<std::uint32_t>(bytes[offset]) << 24)
        | (std::to_integer<std::uint32_t>(bytes[offset + 1]) << 16)
        | (std::to_integer<std::uint32_t>(bytes[offset + 2]) << 8)
        | std::to_integer<std::uint32_t>(bytes[offset + 3]);
    return static_cast<std::int32_t>(value);
}

void write_i32_be(std::vector<std::byte>& bytes, std::size_t offset, std::int32_t value)
{
    const auto unsigned_value = static_cast<std::uint32_t>(value);
    bytes[offset] = static_cast<std::byte>((unsigned_value >> 24) & 0xFF);
    bytes[offset + 1] = static_cast<std::byte>((unsigned_value >> 16) & 0xFF);
    bytes[offset + 2] = static_cast<std::byte>((unsigned_value >> 8) & 0xFF);
    bytes[offset + 3] = static_cast<std::byte>(unsigned_value & 0xFF);
}

void append_i32_be(std::vector<std::byte>& bytes, std::int32_t value)
{
    const auto unsigned_value = static_cast<std::uint32_t>(value);
    bytes.push_back(static_cast<std::byte>((unsigned_value >> 24) & 0xFF));
    bytes.push_back(static_cast<std::byte>((unsigned_value >> 16) & 0xFF));
    bytes.push_back(static_cast<std::byte>((unsigned_value >> 8) & 0xFF));
    bytes.push_back(static_cast<std::byte>(unsigned_value & 0xFF));
}

bool range_contains(Range outer, Range inner)
{
    return outer.offset <= inner.offset && inner.end() <= outer.end();
}

std::vector<Range> remove_contained_ranges(std::vector<Range> ranges)
{
    std::sort(ranges.begin(), ranges.end(), [](Range left, Range right) {
        if (left.offset != right.offset) {
            return left.offset < right.offset;
        }
        return left.size > right.size;
    });

    std::vector<Range> output;
    for (const auto range : ranges) {
        if (!output.empty() && range_contains(output.back(), range)) {
            continue;
        }
        output.push_back(range);
    }
    return output;
}

std::unordered_map<std::int32_t, std::int32_t> id_mapping_lookup(
    const std::vector<BinaryIdMapping>& mappings
)
{
    std::unordered_map<std::int32_t, std::int32_t> lookup;
    for (const auto mapping : mappings) {
        lookup[mapping.old_id] = mapping.new_id;
    }
    return lookup;
}

std::optional<std::int32_t> mapped_id(
    const std::unordered_map<std::int32_t, std::int32_t>& lookup,
    std::int32_t old_id
)
{
    const auto found = lookup.find(old_id);
    if (found == lookup.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::uint32_t tile_elevation_flag(int elevation)
{
    if (elevation == 1) {
        return elevation_one_tile_flag;
    }
    if (elevation == 2) {
        return elevation_two_tile_flag;
    }
    return 0;
}

std::int32_t rewrite_encoded_tile_elevation(std::int32_t tile, int destination_elevation)
{
    auto encoded = static_cast<std::uint32_t>(tile);
    encoded &= ~tile_elevation_mask;
    encoded |= tile_elevation_flag(destination_elevation);
    return static_cast<std::int32_t>(encoded);
}

bool same_range(Range left, Range right)
{
    return left.offset == right.offset && left.size == right.size;
}

bool deleted_script_contains(const BinaryReplaceElevationPlan& plan, const BinaryScriptRecord& script)
{
    for (const auto& deleted : plan.deleted_scripts) {
        if (same_range(deleted.raw, script.raw)) {
            return true;
        }
    }
    return false;
}

bool deleted_object_contains(const BinaryReplaceElevationPlan& plan, const BinaryObjectRecord& object)
{
    for (const auto& deleted : plan.deleted_objects) {
        if (same_range(deleted.raw, object.raw)) {
            return true;
        }
    }
    return false;
}

int script_record_word_count(BinaryScriptType type)
{
    int words = base_script_record_words;
    if (type == BinaryScriptType::spatial) {
        words += 2;
    }
    if (type == BinaryScriptType::timed) {
        words += 1;
    }
    return words;
}

std::vector<std::byte> zero_script_padding_record(BinaryScriptType type)
{
    std::vector<std::byte> bytes(
        static_cast<std::size_t>(script_record_word_count(type)) * sizeof(std::int32_t),
        std::byte{0}
    );
    write_i32_be(bytes, 0, static_cast<std::int32_t>(type) << 24);
    return bytes;
}

bool destination_elevation_present_after(
    const BinaryReplaceElevationWriteRequest& request,
    int elevation
)
{
    return request.destination_header != nullptr
        && (request.destination_header->has_elevation(elevation)
            || elevation == request.plan.destination_elevation);
}

Result<void> validate_model_backed_write_request(const BinaryReplaceElevationWriteRequest& request)
{
    if (request.destination_header == nullptr
        || request.source_scripts == nullptr
        || request.destination_scripts == nullptr
        || request.source_objects == nullptr
        || request.destination_objects == nullptr) {
        return Result<void>::fail({
            "binary replace-elevation writer requires parsed source and destination map models",
            request.destination_object_section_offset,
        });
    }
    return Result<void>::ok();
}

} // namespace

Result<std::vector<std::byte>> replace_binary_range(
    std::span<const std::byte> bytes,
    Range range,
    std::span<const std::byte> replacement
)
{
    if (!range_is_valid(bytes, range)) {
        return Result<std::vector<std::byte>>::fail({
            "replacement range is outside the byte buffer",
            range.offset,
        });
    }

    std::vector<std::byte> output;
    output.reserve(bytes.size() - range.size + replacement.size());
    output.insert(output.end(), bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(range.offset));
    output.insert(output.end(), replacement.begin(), replacement.end());
    output.insert(
        output.end(),
        bytes.begin() + static_cast<std::ptrdiff_t>(range.end()),
        bytes.end()
    );
    return Result<std::vector<std::byte>>::ok(std::move(output));
}

Result<std::vector<std::byte>> remove_binary_ranges(
    std::span<const std::byte> bytes,
    std::span<const Range> ranges
)
{
    std::vector<Range> sorted(ranges.begin(), ranges.end());
    std::sort(sorted.begin(), sorted.end(), [](Range left, Range right) {
        return left.offset < right.offset;
    });

    for (const auto range : sorted) {
        if (!range_is_valid(bytes, range)) {
            return Result<std::vector<std::byte>>::fail({
                "removal range is outside the byte buffer",
                range.offset,
            });
        }
    }

    for (std::size_t index = 1; index < sorted.size(); ++index) {
        if (sorted[index].offset < sorted[index - 1].end()) {
            return Result<std::vector<std::byte>>::fail({
                "removal ranges overlap",
                sorted[index].offset,
            });
        }
    }

    std::vector<std::byte> output(bytes.begin(), bytes.end());
    for (auto range = sorted.rbegin(); range != sorted.rend(); ++range) {
        auto removed = replace_binary_range(output, *range, std::span<const std::byte>{});
        if (!removed) {
            return Result<std::vector<std::byte>>::fail(removed.error());
        }
        output = std::move(removed.value());
    }
    return Result<std::vector<std::byte>>::ok(std::move(output));
}

Result<std::vector<std::byte>> copy_binary_ranges_with_i32_patches(
    std::span<const std::byte> source_bytes,
    std::span<const Range> source_ranges,
    std::span<const BinaryI32Patch> patches
)
{
    struct CopiedRange {
        Range source;
        std::size_t destination_offset = 0;
    };

    std::vector<CopiedRange> copied_ranges;
    copied_ranges.reserve(source_ranges.size());

    std::vector<std::byte> copied;
    std::size_t copied_size = 0;
    for (const auto range : source_ranges) {
        if (!range_is_valid(source_bytes, range)) {
            return Result<std::vector<std::byte>>::fail({
                "source copy range is outside the source byte buffer",
                range.offset,
            });
        }
        copied_ranges.push_back({range, copied_size});
        copied_size += range.size;
    }

    copied.reserve(copied_size);
    for (const auto range : source_ranges) {
        copied.insert(
            copied.end(),
            source_bytes.begin() + static_cast<std::ptrdiff_t>(range.offset),
            source_bytes.begin() + static_cast<std::ptrdiff_t>(range.end())
        );
    }

    std::vector<BinaryI32Patch> translated_patches;
    translated_patches.reserve(patches.size());
    for (const auto patch : patches) {
        const Range patch_range{patch.offset, sizeof(std::int32_t)};
        std::optional<std::size_t> copied_offset;
        for (const auto copied_range : copied_ranges) {
            if (range_contains(copied_range.source, patch_range)) {
                copied_offset = copied_range.destination_offset
                    + patch.offset
                    - copied_range.source.offset;
                break;
            }
        }
        if (!copied_offset) {
            return Result<std::vector<std::byte>>::fail({
                "rewrite patch is outside copied source ranges",
                patch.offset,
            });
        }
        translated_patches.push_back({*copied_offset, patch.value});
    }

    return patch_binary_i32_be_all(copied, translated_patches);
}

std::vector<BinaryI32Patch> i32_patches_inside_ranges(
    std::span<const BinaryI32Patch> patches,
    std::span<const Range> ranges
)
{
    std::vector<BinaryI32Patch> filtered;
    for (const auto patch : patches) {
        const Range patch_range{patch.offset, sizeof(std::int32_t)};
        for (const auto range : ranges) {
            if (range_contains(range, patch_range)) {
                filtered.push_back(patch);
                break;
            }
        }
    }
    return filtered;
}

Result<std::vector<std::byte>> patch_binary_i32_be(
    std::span<const std::byte> bytes,
    std::size_t offset,
    std::int32_t value
)
{
    if (offset > bytes.size() || sizeof(std::int32_t) > bytes.size() - offset) {
        return Result<std::vector<std::byte>>::fail({
            "int32 patch offset is outside the byte buffer",
            offset,
        });
    }

    std::vector<std::byte> output(bytes.begin(), bytes.end());
    write_i32_be(output, offset, value);
    return Result<std::vector<std::byte>>::ok(std::move(output));
}

Result<std::vector<std::byte>> patch_binary_i32_be_all(
    std::span<const std::byte> bytes,
    std::span<const BinaryI32Patch> patches
)
{
    std::vector<BinaryI32Patch> sorted(patches.begin(), patches.end());
    std::sort(sorted.begin(), sorted.end(), [](BinaryI32Patch left, BinaryI32Patch right) {
        if (left.offset != right.offset) {
            return left.offset < right.offset;
        }
        return left.value < right.value;
    });

    std::vector<BinaryI32Patch> unique_patches;
    unique_patches.reserve(sorted.size());
    for (const auto patch : sorted) {
        if (patch.offset > bytes.size() || sizeof(std::int32_t) > bytes.size() - patch.offset) {
            return Result<std::vector<std::byte>>::fail({
                "int32 patch offset is outside the byte buffer",
                patch.offset,
            });
        }
        if (!unique_patches.empty() && unique_patches.back().offset == patch.offset) {
            if (unique_patches.back().value != patch.value) {
                return Result<std::vector<std::byte>>::fail({
                    "conflicting int32 patches target the same offset",
                    patch.offset,
                });
            }
            continue;
        }
        if (!unique_patches.empty()
            && patch.offset < unique_patches.back().offset + sizeof(std::int32_t)) {
            return Result<std::vector<std::byte>>::fail({
                "int32 patches overlap",
                patch.offset,
            });
        }
        unique_patches.push_back(patch);
    }

    std::vector<std::byte> output(bytes.begin(), bytes.end());
    for (const auto patch : unique_patches) {
        write_i32_be(output, patch.offset, patch.value);
    }
    return Result<std::vector<std::byte>>::ok(std::move(output));
}

Result<std::vector<BinaryI32Patch>> build_binary_replace_elevation_source_rewrite_patches(
    const BinaryReplaceElevationPlan& plan
)
{
    if (!is_valid_elevation(plan.destination_elevation)) {
        return Result<std::vector<BinaryI32Patch>>::fail({"invalid destination elevation", 0});
    }

    const auto object_ids = id_mapping_lookup(plan.object_id_mappings);
    const auto script_ids = id_mapping_lookup(plan.script_id_mappings);
    std::vector<BinaryI32Patch> patches;
    patches.reserve(
        plan.copied_objects.size() * 3
        + plan.copied_scripts.size() * 3
    );

    for (const auto& object : plan.copied_objects) {
        const auto new_object_id = mapped_id(object_ids, object.object_id);
        if (!new_object_id) {
            return Result<std::vector<BinaryI32Patch>>::fail({
                std::string{"missing object ID mapping for copied object "}
                    + std::to_string(object.object_id),
                object.raw.offset,
            });
        }
        patches.push_back({object.offsets.obj_id, *new_object_id});

        if (object.elevation == plan.source_elevation) {
            patches.push_back({object.offsets.elevation, plan.destination_elevation});
        }

        if (object.script_id != -1 && object.script_id != 0) {
            const auto new_script_id = mapped_id(script_ids, object.script_id);
            if (!new_script_id) {
                return Result<std::vector<BinaryI32Patch>>::fail({
                    std::string{"missing script ID mapping for copied object script "}
                        + std::to_string(object.script_id),
                    object.raw.offset,
                });
            }
            patches.push_back({object.offsets.script_id, *new_script_id});
        }
    }

    for (const auto& script : plan.copied_scripts) {
        const auto new_script_id = mapped_id(script_ids, script.script_id);
        if (!new_script_id) {
            return Result<std::vector<BinaryI32Patch>>::fail({
                std::string{"missing script ID mapping for copied script "}
                    + std::to_string(script.script_id),
                script.raw.offset,
            });
        }
        patches.push_back({script.offsets.scr_id, *new_script_id});

        if (script.script_type == BinaryScriptType::spatial) {
            if (!script.offsets.spatial_tile) {
                return Result<std::vector<BinaryI32Patch>>::fail({
                    "copied spatial script is missing a spatial tile offset",
                    script.raw.offset,
                });
            }
            patches.push_back({
                *script.offsets.spatial_tile,
                rewrite_encoded_tile_elevation(script.spatial_tile, plan.destination_elevation),
            });
        }

        if (script.object_id != 0) {
            const auto new_object_id = mapped_id(
                object_ids,
                static_cast<std::int32_t>(script.object_id)
            );
            if (!new_object_id) {
                return Result<std::vector<BinaryI32Patch>>::fail({
                    std::string{"missing object ID mapping for copied script object "}
                        + std::to_string(script.object_id),
                    script.raw.offset,
                });
            }
            patches.push_back({script.offsets.scr_obj_id, *new_object_id});
        }
    }

    return Result<std::vector<BinaryI32Patch>>::ok(std::move(patches));
}

Result<std::vector<std::byte>> patch_binary_replace_elevation_object_counts(
    std::span<const std::byte> bytes,
    std::size_t object_section_offset,
    const BinaryReplaceElevationPlan& plan
)
{
    std::int64_t summed_counts = 0;
    for (const auto count : plan.destination_object_counts_after) {
        if (count < 0) {
            return Result<std::vector<std::byte>>::fail({
                "planned destination object count is negative",
                object_section_offset,
            });
        }
        summed_counts += count;
    }
    if (plan.destination_total_objects_after < 0 || summed_counts != plan.destination_total_objects_after) {
        return Result<std::vector<std::byte>>::fail({
            "planned destination object counts do not match total",
            object_section_offset,
        });
    }

    auto output = patch_binary_i32_be(
        bytes,
        object_section_offset,
        plan.destination_total_objects_after
    );
    if (!output) {
        return Result<std::vector<std::byte>>::fail(output.error());
    }

    for (std::size_t elevation = 0; elevation < plan.destination_object_counts_after.size(); ++elevation) {
        output = patch_binary_i32_be(
            output.value(),
            object_section_offset + object_count_word_size * (elevation + 1),
            plan.destination_object_counts_after[elevation]
        );
        if (!output) {
            return Result<std::vector<std::byte>>::fail(output.error());
        }
    }
    return output;
}

Result<std::vector<std::byte>> patch_binary_replace_elevation_script_counts(
    std::span<const std::byte> bytes,
    std::array<std::size_t, binary_script_type_count> script_count_offsets,
    const BinaryReplaceElevationPlan& plan
)
{
    auto output = Result<std::vector<std::byte>>::ok(
        std::vector<std::byte>(bytes.begin(), bytes.end())
    );
    for (std::size_t type = 0; type < plan.destination_script_counts_after.size(); ++type) {
        const auto count = plan.destination_script_counts_after[type];
        if (count > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
            return Result<std::vector<std::byte>>::fail({
                "planned destination script count exceeds int32 range",
                script_count_offsets[type],
            });
        }

        output = patch_binary_i32_be(
            output.value(),
            script_count_offsets[type],
            static_cast<std::int32_t>(count)
        );
        if (!output) {
            return Result<std::vector<std::byte>>::fail(output.error());
        }
    }
    return output;
}

Result<std::vector<std::byte>> patch_binary_replace_elevation_header_flags(
    std::span<const std::byte> destination_bytes,
    const BinaryReplaceElevationPlan& plan
)
{
    if (!is_valid_elevation(plan.destination_elevation)) {
        return Result<std::vector<std::byte>>::fail({"invalid destination elevation", 0});
    }
    if (destination_bytes.size() < map_flags_offset + map_flags_size) {
        return Result<std::vector<std::byte>>::fail({
            "destination map header is too short for map flags",
            destination_bytes.size(),
        });
    }

    std::vector<std::byte> output(destination_bytes.begin(), destination_bytes.end());
    const auto current_flags = read_i32_be(destination_bytes, map_flags_offset);
    const auto absent_flag =
        map_elevation_absent_flags[static_cast<std::size_t>(plan.destination_elevation)];
    return patch_binary_i32_be(output, map_flags_offset, current_flags & ~absent_flag);
}

Result<std::vector<std::byte>> patch_binary_replace_elevation_tiles(
    std::span<const std::byte> source_bytes,
    std::span<const std::byte> destination_bytes,
    const BinaryReplaceElevationPlan& plan
)
{
    if (plan.source_tile_range.size != plan.destination_tile_range.size) {
        return Result<std::vector<std::byte>>::fail({
            "source and destination tile ranges differ in size",
            plan.destination_tile_range.offset,
        });
    }
    if (!range_is_valid(source_bytes, plan.source_tile_range)) {
        return Result<std::vector<std::byte>>::fail({
            "source tile range is outside the source map buffer",
            plan.source_tile_range.offset,
        });
    }
    if (!range_is_valid(destination_bytes, plan.destination_tile_range)) {
        return Result<std::vector<std::byte>>::fail({
            "destination tile range is outside the destination map buffer",
            plan.destination_tile_range.offset,
        });
    }

    auto replacement = source_bytes.subspan(
        plan.source_tile_range.offset,
        plan.source_tile_range.size
    );
    return replace_binary_range(destination_bytes, plan.destination_tile_range, replacement);
}

Result<std::vector<std::byte>> rebuild_binary_replace_elevation_script_section(
    const BinaryReplaceElevationWriteRequest& request,
    std::span<const BinaryI32Patch> rewrite_patches
)
{
    if (request.source_scripts == nullptr || request.destination_scripts == nullptr) {
        return Result<std::vector<std::byte>>::fail({
            "script section rebuild requires parsed source and destination scripts",
            request.destination_object_section_offset,
        });
    }
    if (request.destination_script_count_offsets[0] > request.destination_object_section_offset) {
        return Result<std::vector<std::byte>>::fail({
            "destination script section start is after the object section",
            request.destination_script_count_offsets[0],
        });
    }

    std::vector<std::byte> section;
    for (std::size_t type_index = 0; type_index < binary_script_type_count; ++type_index) {
        const auto type = static_cast<BinaryScriptType>(type_index);
        std::vector<std::vector<std::byte>> record_bytes;

        for (const auto& script : request.destination_scripts->by_type[type_index]) {
            if (deleted_script_contains(request.plan, script)) {
                continue;
            }
            if (!range_is_valid(request.destination_bytes, script.raw)) {
                return Result<std::vector<std::byte>>::fail({
                    "preserved destination script range is outside the destination map buffer",
                    script.raw.offset,
                });
            }
            record_bytes.emplace_back(
                request.destination_bytes.begin() + static_cast<std::ptrdiff_t>(script.raw.offset),
                request.destination_bytes.begin() + static_cast<std::ptrdiff_t>(script.raw.end())
            );
        }

        for (const auto& copied : request.plan.copied_scripts) {
            if (copied.script_type != type) {
                continue;
            }
            const std::vector<Range> copied_range{copied.raw};
            const auto script_patches = i32_patches_inside_ranges(rewrite_patches, copied_range);
            auto copied_bytes = copy_binary_ranges_with_i32_patches(
                request.source_bytes,
                copied_range,
                script_patches
            );
            if (!copied_bytes) {
                return Result<std::vector<std::byte>>::fail(copied_bytes.error());
            }
            record_bytes.push_back(std::move(copied_bytes.value()));
        }

        const auto expected_count = request.plan.destination_script_counts_after[type_index];
        if (record_bytes.size() != expected_count) {
            return Result<std::vector<std::byte>>::fail({
                "rebuilt script count does not match planned type count",
                request.destination_script_count_offsets[type_index],
            });
        }
        if (record_bytes.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
            return Result<std::vector<std::byte>>::fail({
                "rebuilt script count exceeds int32 range",
                request.destination_script_count_offsets[type_index],
            });
        }

        append_i32_be(section, static_cast<std::int32_t>(record_bytes.size()));
        std::size_t index = 0;
        while (index < record_bytes.size()) {
            const auto remaining = record_bytes.size() - index;
            const auto block_count = std::min<std::size_t>(
                remaining,
                serialized_script_block_capacity
            );
            for (std::size_t slot = 0; slot < block_count; ++slot) {
                section.insert(
                    section.end(),
                    record_bytes[index + slot].begin(),
                    record_bytes[index + slot].end()
                );
            }
            if (block_count < serialized_script_block_capacity) {
                const auto padding = zero_script_padding_record(type);
                for (std::size_t slot = block_count; slot < serialized_script_block_capacity; ++slot) {
                    section.insert(section.end(), padding.begin(), padding.end());
                }
            }
            append_i32_be(section, static_cast<std::int32_t>(block_count));
            append_i32_be(section, 0);
            index += block_count;
        }
    }

    return Result<std::vector<std::byte>>::ok(std::move(section));
}

Result<std::vector<std::byte>> rebuild_binary_replace_elevation_object_section(
    const BinaryReplaceElevationWriteRequest& request,
    std::span<const BinaryI32Patch> rewrite_patches
)
{
    if (request.destination_header == nullptr
        || request.source_objects == nullptr
        || request.destination_objects == nullptr) {
        return Result<std::vector<std::byte>>::fail({
            "object section rebuild requires parsed destination header and source/destination objects",
            request.destination_object_section_offset,
        });
    }

    std::vector<std::byte> section;
    append_i32_be(section, request.plan.destination_total_objects_after);

    for (int elevation = 0; elevation < binary_map_elevation_count; ++elevation) {
        const auto expected_count =
            request.plan.destination_object_counts_after[static_cast<std::size_t>(elevation)];
        if (!destination_elevation_present_after(request, elevation)) {
            if (expected_count != 0) {
                return Result<std::vector<std::byte>>::fail({
                    "absent rebuilt elevation has planned object records",
                    request.destination_object_section_offset,
                });
            }
            append_i32_be(section, 0);
            continue;
        }

        std::vector<std::vector<std::byte>> record_bytes;
        for (const auto& object : request.destination_objects->records) {
            if (object.prefix.elevation != elevation || deleted_object_contains(request.plan, object)) {
                continue;
            }
            if (!range_is_valid(request.destination_bytes, object.raw)) {
                return Result<std::vector<std::byte>>::fail({
                    "preserved destination object range is outside the destination map buffer",
                    object.raw.offset,
                });
            }
            record_bytes.emplace_back(
                request.destination_bytes.begin() + static_cast<std::ptrdiff_t>(object.raw.offset),
                request.destination_bytes.begin() + static_cast<std::ptrdiff_t>(object.raw.end())
            );
        }

        if (elevation == request.plan.destination_elevation) {
            std::vector<Range> copied_object_ranges;
            for (const auto& copied : request.plan.copied_objects) {
                if (copied.elevation == request.plan.source_elevation) {
                    copied_object_ranges.push_back(copied.raw);
                }
            }
            copied_object_ranges = remove_contained_ranges(std::move(copied_object_ranges));
            for (const auto range : copied_object_ranges) {
                const std::vector<Range> copied_range{range};
                const auto object_patches = i32_patches_inside_ranges(
                    rewrite_patches,
                    copied_range
                );
                auto copied_bytes = copy_binary_ranges_with_i32_patches(
                    request.source_bytes,
                    copied_range,
                    object_patches
                );
                if (!copied_bytes) {
                    return Result<std::vector<std::byte>>::fail(copied_bytes.error());
                }
                record_bytes.push_back(std::move(copied_bytes.value()));
            }
        }

        if (record_bytes.size() != static_cast<std::size_t>(expected_count)) {
            return Result<std::vector<std::byte>>::fail({
                "rebuilt object count does not match planned elevation count",
                request.destination_object_section_offset,
            });
        }

        append_i32_be(section, expected_count);
        for (const auto& record : record_bytes) {
            section.insert(section.end(), record.begin(), record.end());
        }
    }

    return Result<std::vector<std::byte>>::ok(std::move(section));
}

Result<std::vector<std::byte>> write_binary_replace_elevation_patch(
    const BinaryReplaceElevationWriteRequest& request
)
{
    auto header_patched = patch_binary_replace_elevation_header_flags(
        request.destination_bytes,
        request.plan
    );
    if (!header_patched) {
        return Result<std::vector<std::byte>>::fail(header_patched.error());
    }

    auto tiles_patched = patch_binary_replace_elevation_tiles(
        request.source_bytes,
        header_patched.value(),
        request.plan
    );
    if (!tiles_patched) {
        return Result<std::vector<std::byte>>::fail(tiles_patched.error());
    }

    auto valid_model = validate_model_backed_write_request(request);
    if (!valid_model) {
        return Result<std::vector<std::byte>>::fail(valid_model.error());
    }

    auto rewrite_patches = build_binary_replace_elevation_source_rewrite_patches(request.plan);
    if (!rewrite_patches) {
        return Result<std::vector<std::byte>>::fail(rewrite_patches.error());
    }

    auto rebuilt_scripts = rebuild_binary_replace_elevation_script_section(
        request,
        rewrite_patches.value()
    );
    if (!rebuilt_scripts) {
        return Result<std::vector<std::byte>>::fail(rebuilt_scripts.error());
    }

    const Range destination_script_section{
        request.destination_script_count_offsets[0],
        request.destination_object_section_offset - request.destination_script_count_offsets[0],
    };
    auto scripts_replaced = replace_binary_range(
        tiles_patched.value(),
        destination_script_section,
        rebuilt_scripts.value()
    );
    if (!scripts_replaced) {
        return Result<std::vector<std::byte>>::fail(scripts_replaced.error());
    }

    const auto shifted_object_section_offset =
        request.destination_script_count_offsets[0] + rebuilt_scripts.value().size();
    auto rebuilt_objects = rebuild_binary_replace_elevation_object_section(
        request,
        rewrite_patches.value()
    );
    if (!rebuilt_objects) {
        return Result<std::vector<std::byte>>::fail(rebuilt_objects.error());
    }

    return replace_binary_range(
        scripts_replaced.value(),
        Range{shifted_object_section_offset, scripts_replaced.value().size() - shifted_object_section_offset},
        rebuilt_objects.value()
    );
}

} // namespace qmap
