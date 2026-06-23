#include <catch2/catch_test_macros.hpp>

#include "binary_map_patch_writer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace {

void write_i32_be(std::vector<std::byte>& bytes, std::size_t offset, std::int32_t value)
{
    const auto unsigned_value = static_cast<std::uint32_t>(value);
    bytes[offset] = static_cast<std::byte>((unsigned_value >> 24) & 0xFF);
    bytes[offset + 1] = static_cast<std::byte>((unsigned_value >> 16) & 0xFF);
    bytes[offset + 2] = static_cast<std::byte>((unsigned_value >> 8) & 0xFF);
    bytes[offset + 3] = static_cast<std::byte>(unsigned_value & 0xFF);
}

} // namespace

TEST_CASE("replace_binary_range replaces a same-size byte span", "[map][binary][patch]")
{
    const std::vector<std::byte> bytes{
        std::byte{0x10},
        std::byte{0x11},
        std::byte{0x12},
        std::byte{0x13},
    };
    const std::vector<std::byte> replacement{
        std::byte{0xA0},
        std::byte{0xA1},
    };

    const auto replaced = qmap::replace_binary_range(bytes, qmap::Range{1, 2}, replacement);

    REQUIRE(replaced);
    CHECK(replaced.value().size() == 4);
    CHECK(replaced.value()[0] == std::byte{0x10});
    CHECK(replaced.value()[1] == std::byte{0xA0});
    CHECK(replaced.value()[2] == std::byte{0xA1});
    CHECK(replaced.value()[3] == std::byte{0x13});
}

TEST_CASE("replace_binary_range can shrink and grow buffers", "[map][binary][patch]")
{
    const std::vector<std::byte> bytes{
        std::byte{0x10},
        std::byte{0x11},
        std::byte{0x12},
        std::byte{0x13},
    };
    const std::vector<std::byte> small_replacement{std::byte{0xA0}};
    const std::vector<std::byte> large_replacement{
        std::byte{0xB0},
        std::byte{0xB1},
        std::byte{0xB2},
    };

    const auto shrunk = qmap::replace_binary_range(bytes, qmap::Range{1, 2}, small_replacement);
    REQUIRE(shrunk);
    CHECK(shrunk.value().size() == 3);
    CHECK(shrunk.value()[1] == std::byte{0xA0});
    CHECK(shrunk.value()[2] == std::byte{0x13});

    const auto grown = qmap::replace_binary_range(bytes, qmap::Range{1, 1}, large_replacement);
    REQUIRE(grown);
    CHECK(grown.value().size() == 6);
    CHECK(grown.value()[1] == std::byte{0xB0});
    CHECK(grown.value()[3] == std::byte{0xB2});
    CHECK(grown.value()[4] == std::byte{0x12});
}

TEST_CASE("replace_binary_range inserts at end with an empty range", "[map][binary][patch]")
{
    const std::vector<std::byte> bytes{std::byte{0x10}, std::byte{0x11}};
    const std::vector<std::byte> replacement{std::byte{0xA0}, std::byte{0xA1}};

    const auto replaced = qmap::replace_binary_range(bytes, qmap::Range{2, 0}, replacement);

    REQUIRE(replaced);
    CHECK(replaced.value().size() == 4);
    CHECK(replaced.value()[2] == std::byte{0xA0});
    CHECK(replaced.value()[3] == std::byte{0xA1});
}

TEST_CASE("replace_binary_range rejects ranges outside the byte buffer", "[map][binary][patch]")
{
    const std::vector<std::byte> bytes(4);
    const std::vector<std::byte> replacement{std::byte{0xA0}};

    const auto replaced = qmap::replace_binary_range(bytes, qmap::Range{3, 2}, replacement);

    REQUIRE_FALSE(replaced);
    CHECK(replaced.error().message == "replacement range is outside the byte buffer");
    CHECK(replaced.error().offset == 3);
}

TEST_CASE("remove_binary_ranges removes multiple ranges independent of input order", "[map][binary][patch]")
{
    const std::vector<std::byte> bytes{
        std::byte{0x10},
        std::byte{0x11},
        std::byte{0x12},
        std::byte{0x13},
        std::byte{0x14},
        std::byte{0x15},
        std::byte{0x16},
    };
    const std::vector<qmap::Range> ranges{
        qmap::Range{5, 1},
        qmap::Range{1, 2},
    };

    const auto removed = qmap::remove_binary_ranges(bytes, ranges);

    REQUIRE(removed);
    CHECK(removed.value().size() == 4);
    CHECK(removed.value()[0] == std::byte{0x10});
    CHECK(removed.value()[1] == std::byte{0x13});
    CHECK(removed.value()[2] == std::byte{0x14});
    CHECK(removed.value()[3] == std::byte{0x16});
}

TEST_CASE("remove_binary_ranges rejects ranges outside the original byte buffer", "[map][binary][patch]")
{
    const std::vector<std::byte> bytes(4);
    const std::vector<qmap::Range> ranges{qmap::Range{3, 2}};

    const auto removed = qmap::remove_binary_ranges(bytes, ranges);

    REQUIRE_FALSE(removed);
    CHECK(removed.error().message == "removal range is outside the byte buffer");
    CHECK(removed.error().offset == 3);
}

TEST_CASE("remove_binary_ranges rejects overlapping ranges", "[map][binary][patch]")
{
    const std::vector<std::byte> bytes(8);
    const std::vector<qmap::Range> ranges{
        qmap::Range{1, 3},
        qmap::Range{3, 2},
    };

    const auto removed = qmap::remove_binary_ranges(bytes, ranges);

    REQUIRE_FALSE(removed);
    CHECK(removed.error().message == "removal ranges overlap");
    CHECK(removed.error().offset == 3);
}

TEST_CASE("adjust_binary_offset_after_removing_ranges subtracts prior removed byte counts", "[map][binary][patch]")
{
    const std::vector<qmap::Range> ranges{
        qmap::Range{30, 5},
        qmap::Range{10, 4},
    };

    const auto adjusted = qmap::adjust_binary_offset_after_removing_ranges(40, ranges);

    REQUIRE(adjusted);
    CHECK(adjusted.value() == 31);
}

TEST_CASE("adjust_binary_offset_after_removing_ranges leaves offsets before removals unchanged", "[map][binary][patch]")
{
    const std::vector<qmap::Range> ranges{
        qmap::Range{10, 4},
        qmap::Range{30, 5},
    };

    const auto adjusted = qmap::adjust_binary_offset_after_removing_ranges(8, ranges);

    REQUIRE(adjusted);
    CHECK(adjusted.value() == 8);
}

TEST_CASE("adjust_binary_offset_after_removing_ranges treats range ends as stable anchors", "[map][binary][patch]")
{
    const std::vector<qmap::Range> ranges{
        qmap::Range{10, 4},
    };

    const auto adjusted = qmap::adjust_binary_offset_after_removing_ranges(14, ranges);

    REQUIRE(adjusted);
    CHECK(adjusted.value() == 10);
}

TEST_CASE("adjust_binary_offset_after_removing_ranges rejects offsets inside removed ranges", "[map][binary][patch]")
{
    const std::vector<qmap::Range> ranges{
        qmap::Range{10, 4},
    };

    const auto adjusted = qmap::adjust_binary_offset_after_removing_ranges(12, ranges);

    REQUIRE_FALSE(adjusted);
    CHECK(adjusted.error().message == "offset falls inside a removed range");
    CHECK(adjusted.error().offset == 12);
}

TEST_CASE("insert_binary_ranges inserts source ranges in supplied order", "[map][binary][patch]")
{
    const std::vector<std::byte> bytes{
        std::byte{0x10},
        std::byte{0x11},
        std::byte{0x12},
    };
    const std::vector<std::byte> source_bytes{
        std::byte{0xA0},
        std::byte{0xA1},
        std::byte{0xA2},
        std::byte{0xA3},
        std::byte{0xA4},
    };
    const std::vector<qmap::Range> ranges{
        qmap::Range{3, 2},
        qmap::Range{0, 2},
    };

    const auto inserted = qmap::insert_binary_ranges(bytes, 1, source_bytes, ranges);

    REQUIRE(inserted);
    CHECK(inserted.value().size() == 7);
    CHECK(inserted.value()[0] == std::byte{0x10});
    CHECK(inserted.value()[1] == std::byte{0xA3});
    CHECK(inserted.value()[2] == std::byte{0xA4});
    CHECK(inserted.value()[3] == std::byte{0xA0});
    CHECK(inserted.value()[4] == std::byte{0xA1});
    CHECK(inserted.value()[5] == std::byte{0x11});
    CHECK(inserted.value()[6] == std::byte{0x12});
}

TEST_CASE("insert_binary_ranges appends at end", "[map][binary][patch]")
{
    const std::vector<std::byte> bytes{std::byte{0x10}};
    const std::vector<std::byte> source_bytes{std::byte{0xA0}, std::byte{0xA1}};
    const std::vector<qmap::Range> ranges{qmap::Range{0, 2}};

    const auto inserted = qmap::insert_binary_ranges(bytes, 1, source_bytes, ranges);

    REQUIRE(inserted);
    CHECK(inserted.value().size() == 3);
    CHECK(inserted.value()[0] == std::byte{0x10});
    CHECK(inserted.value()[1] == std::byte{0xA0});
    CHECK(inserted.value()[2] == std::byte{0xA1});
}

TEST_CASE("insert_binary_ranges rejects invalid destination offset", "[map][binary][patch]")
{
    const std::vector<std::byte> bytes(2);
    const std::vector<std::byte> source_bytes(2);
    const std::vector<qmap::Range> ranges{qmap::Range{0, 1}};

    const auto inserted = qmap::insert_binary_ranges(bytes, 3, source_bytes, ranges);

    REQUIRE_FALSE(inserted);
    CHECK(inserted.error().message == "insertion offset is outside the byte buffer");
    CHECK(inserted.error().offset == 3);
}

TEST_CASE("insert_binary_ranges rejects invalid source ranges", "[map][binary][patch]")
{
    const std::vector<std::byte> bytes(2);
    const std::vector<std::byte> source_bytes(2);
    const std::vector<qmap::Range> ranges{qmap::Range{1, 2}};

    const auto inserted = qmap::insert_binary_ranges(bytes, 1, source_bytes, ranges);

    REQUIRE_FALSE(inserted);
    CHECK(inserted.error().message == "source insertion range is outside the source byte buffer");
    CHECK(inserted.error().offset == 1);
}

TEST_CASE("copy_binary_ranges_with_i32_patches copies ranges and translates source-offset patches", "[map][binary][patch]")
{
    const std::vector<std::byte> source_bytes{
        std::byte{0x10},
        std::byte{0x11},
        std::byte{0x12},
        std::byte{0x13},
        std::byte{0x20},
        std::byte{0x21},
        std::byte{0x22},
        std::byte{0x23},
        std::byte{0x30},
        std::byte{0x31},
        std::byte{0x32},
        std::byte{0x33},
    };
    const std::vector<qmap::Range> ranges{
        qmap::Range{4, 4},
        qmap::Range{0, 4},
    };
    const std::vector<qmap::BinaryI32Patch> patches{
        qmap::BinaryI32Patch{0, 0x01020304},
        qmap::BinaryI32Patch{4, 0x05060708},
    };

    const auto copied = qmap::copy_binary_ranges_with_i32_patches(
        source_bytes,
        ranges,
        patches
    );

    REQUIRE(copied);
    REQUIRE(copied.value().size() == 8);
    CHECK(copied.value()[0] == std::byte{0x05});
    CHECK(copied.value()[1] == std::byte{0x06});
    CHECK(copied.value()[2] == std::byte{0x07});
    CHECK(copied.value()[3] == std::byte{0x08});
    CHECK(copied.value()[4] == std::byte{0x01});
    CHECK(copied.value()[5] == std::byte{0x02});
    CHECK(copied.value()[6] == std::byte{0x03});
    CHECK(copied.value()[7] == std::byte{0x04});
}

TEST_CASE("copy_binary_ranges_with_i32_patches rejects invalid copy ranges", "[map][binary][patch]")
{
    const std::vector<std::byte> source_bytes(4);
    const std::vector<qmap::Range> ranges{qmap::Range{3, 2}};
    const std::vector<qmap::BinaryI32Patch> patches;

    const auto copied = qmap::copy_binary_ranges_with_i32_patches(
        source_bytes,
        ranges,
        patches
    );

    REQUIRE_FALSE(copied);
    CHECK(copied.error().message == "source copy range is outside the source byte buffer");
    CHECK(copied.error().offset == 3);
}

TEST_CASE("copy_binary_ranges_with_i32_patches rejects patches outside copied ranges", "[map][binary][patch]")
{
    const std::vector<std::byte> source_bytes(12);
    const std::vector<qmap::Range> ranges{qmap::Range{0, 4}};
    const std::vector<qmap::BinaryI32Patch> patches{
        qmap::BinaryI32Patch{8, 1},
    };

    const auto copied = qmap::copy_binary_ranges_with_i32_patches(
        source_bytes,
        ranges,
        patches
    );

    REQUIRE_FALSE(copied);
    CHECK(copied.error().message == "rewrite patch is outside copied source ranges");
    CHECK(copied.error().offset == 8);
}

TEST_CASE("patch_binary_i32_be writes a big-endian int32 without changing surrounding bytes", "[map][binary][patch]")
{
    const std::vector<std::byte> bytes(8, std::byte{0xAA});

    const auto patched = qmap::patch_binary_i32_be(bytes, 2, 0x01020304);

    REQUIRE(patched);
    CHECK(patched.value()[0] == std::byte{0xAA});
    CHECK(patched.value()[1] == std::byte{0xAA});
    CHECK(patched.value()[2] == std::byte{0x01});
    CHECK(patched.value()[3] == std::byte{0x02});
    CHECK(patched.value()[4] == std::byte{0x03});
    CHECK(patched.value()[5] == std::byte{0x04});
    CHECK(patched.value()[6] == std::byte{0xAA});
    CHECK(patched.value()[7] == std::byte{0xAA});
}

TEST_CASE("patch_binary_i32_be preserves two's-complement negative values", "[map][binary][patch]")
{
    const std::vector<std::byte> bytes(4);

    const auto patched = qmap::patch_binary_i32_be(bytes, 0, -2);

    REQUIRE(patched);
    CHECK(patched.value()[0] == std::byte{0xFF});
    CHECK(patched.value()[1] == std::byte{0xFF});
    CHECK(patched.value()[2] == std::byte{0xFF});
    CHECK(patched.value()[3] == std::byte{0xFE});
}

TEST_CASE("patch_binary_i32_be accepts the last complete int32 slot", "[map][binary][patch]")
{
    const std::vector<std::byte> bytes(6, std::byte{0xAA});

    const auto patched = qmap::patch_binary_i32_be(bytes, 2, 0x01020304);

    REQUIRE(patched);
    CHECK(patched.value()[2] == std::byte{0x01});
    CHECK(patched.value()[5] == std::byte{0x04});
}

TEST_CASE("patch_binary_i32_be rejects short writes", "[map][binary][patch]")
{
    const std::vector<std::byte> bytes(6);

    const auto patched = qmap::patch_binary_i32_be(bytes, 3, 0x01020304);

    REQUIRE_FALSE(patched);
    CHECK(patched.error().message == "int32 patch offset is outside the byte buffer");
    CHECK(patched.error().offset == 3);
}

TEST_CASE("patch_binary_i32_be_all applies unordered non-overlapping patches", "[map][binary][patch]")
{
    const std::vector<std::byte> bytes(12, std::byte{0xAA});
    const std::vector<qmap::BinaryI32Patch> patches{
        qmap::BinaryI32Patch{8, -2},
        qmap::BinaryI32Patch{0, 0x01020304},
    };

    const auto patched = qmap::patch_binary_i32_be_all(bytes, patches);

    REQUIRE(patched);
    CHECK(patched.value()[0] == std::byte{0x01});
    CHECK(patched.value()[3] == std::byte{0x04});
    CHECK(patched.value()[4] == std::byte{0xAA});
    CHECK(patched.value()[7] == std::byte{0xAA});
    CHECK(patched.value()[8] == std::byte{0xFF});
    CHECK(patched.value()[11] == std::byte{0xFE});
}

TEST_CASE("patch_binary_i32_be_all allows duplicate identical patches", "[map][binary][patch]")
{
    const std::vector<std::byte> bytes(8, std::byte{0xAA});
    const std::vector<qmap::BinaryI32Patch> patches{
        qmap::BinaryI32Patch{2, 0x01020304},
        qmap::BinaryI32Patch{2, 0x01020304},
    };

    const auto patched = qmap::patch_binary_i32_be_all(bytes, patches);

    REQUIRE(patched);
    CHECK(patched.value()[1] == std::byte{0xAA});
    CHECK(patched.value()[2] == std::byte{0x01});
    CHECK(patched.value()[5] == std::byte{0x04});
    CHECK(patched.value()[6] == std::byte{0xAA});
}

TEST_CASE("patch_binary_i32_be_all rejects conflicting patches at the same offset", "[map][binary][patch]")
{
    const std::vector<std::byte> bytes(8);
    const std::vector<qmap::BinaryI32Patch> patches{
        qmap::BinaryI32Patch{2, 1},
        qmap::BinaryI32Patch{2, 2},
    };

    const auto patched = qmap::patch_binary_i32_be_all(bytes, patches);

    REQUIRE_FALSE(patched);
    CHECK(patched.error().message == "conflicting int32 patches target the same offset");
    CHECK(patched.error().offset == 2);
}

TEST_CASE("patch_binary_i32_be_all rejects overlapping patches", "[map][binary][patch]")
{
    const std::vector<std::byte> bytes(8);
    const std::vector<qmap::BinaryI32Patch> patches{
        qmap::BinaryI32Patch{1, 1},
        qmap::BinaryI32Patch{4, 2},
    };

    const auto patched = qmap::patch_binary_i32_be_all(bytes, patches);

    REQUIRE_FALSE(patched);
    CHECK(patched.error().message == "int32 patches overlap");
    CHECK(patched.error().offset == 4);
}

TEST_CASE("patch_binary_i32_be_all rejects out-of-range patch targets", "[map][binary][patch]")
{
    const std::vector<std::byte> bytes(8);
    const std::vector<qmap::BinaryI32Patch> patches{
        qmap::BinaryI32Patch{5, 1},
    };

    const auto patched = qmap::patch_binary_i32_be_all(bytes, patches);

    REQUIRE_FALSE(patched);
    CHECK(patched.error().message == "int32 patch offset is outside the byte buffer");
    CHECK(patched.error().offset == 5);
}

TEST_CASE("build_binary_replace_elevation_source_rewrite_patches rewrites copied object and script fields", "[map][binary][patch]")
{
    qmap::BinaryReplaceElevationPlan plan;
    plan.source_elevation = 0;
    plan.destination_elevation = 2;
    plan.object_id_mappings.push_back({10, 100});
    plan.object_id_mappings.push_back({11, 101});
    plan.script_id_mappings.push_back({0x01000005, 0x01000040});
    plan.script_id_mappings.push_back({0x03000010, 0x03000040});

    qmap::BinaryPlannedObjectCopy top_level;
    top_level.object_id = 10;
    top_level.elevation = 0;
    top_level.script_id = 0x03000010;
    top_level.raw = qmap::Range{100, 88};
    top_level.offsets.obj_id = 100;
    top_level.offsets.elevation = 140;
    top_level.offsets.script_id = 164;
    plan.copied_objects.push_back(top_level);

    qmap::BinaryPlannedObjectCopy inventory_child;
    inventory_child.object_id = 11;
    inventory_child.elevation = -1;
    inventory_child.script_id = -1;
    inventory_child.raw = qmap::Range{188, 88};
    inventory_child.offsets.obj_id = 188;
    inventory_child.offsets.elevation = 228;
    inventory_child.offsets.script_id = 252;
    plan.copied_objects.push_back(inventory_child);

    qmap::BinaryPlannedScriptCopy spatial;
    spatial.script_id = 0x01000005;
    spatial.script_type = qmap::BinaryScriptType::spatial;
    spatial.spatial_tile = 123;
    spatial.raw = qmap::Range{300, 72};
    spatial.offsets.scr_id = 300;
    spatial.offsets.spatial_tile = 308;
    spatial.offsets.scr_obj_id = 328;
    plan.copied_scripts.push_back(spatial);

    qmap::BinaryPlannedScriptCopy object_script;
    object_script.script_id = 0x03000010;
    object_script.script_type = qmap::BinaryScriptType::object;
    object_script.object_id = 10;
    object_script.raw = qmap::Range{400, 64};
    object_script.offsets.scr_id = 400;
    object_script.offsets.scr_obj_id = 420;
    plan.copied_scripts.push_back(object_script);

    const auto patches = qmap::build_binary_replace_elevation_source_rewrite_patches(plan);

    REQUIRE(patches);
    REQUIRE(patches.value().size() == 8);
    CHECK(patches.value()[0].offset == 100);
    CHECK(patches.value()[0].value == 100);
    CHECK(patches.value()[1].offset == 140);
    CHECK(patches.value()[1].value == 2);
    CHECK(patches.value()[2].offset == 164);
    CHECK(patches.value()[2].value == 0x03000040);
    CHECK(patches.value()[3].offset == 188);
    CHECK(patches.value()[3].value == 101);
    CHECK(patches.value()[4].offset == 300);
    CHECK(patches.value()[4].value == 0x01000040);
    CHECK(patches.value()[5].offset == 308);
    CHECK(patches.value()[5].value == 0x4000007B);
    CHECK(patches.value()[6].offset == 400);
    CHECK(patches.value()[6].value == 0x03000040);
    CHECK(patches.value()[7].offset == 420);
    CHECK(patches.value()[7].value == 100);
}

TEST_CASE("build_binary_replace_elevation_source_rewrite_patches rejects missing object mappings", "[map][binary][patch]")
{
    qmap::BinaryReplaceElevationPlan plan;
    plan.source_elevation = 0;
    plan.destination_elevation = 1;
    qmap::BinaryPlannedObjectCopy copied_object;
    copied_object.object_id = 10;
    copied_object.raw = qmap::Range{20, 88};
    plan.copied_objects.push_back(copied_object);

    const auto patches = qmap::build_binary_replace_elevation_source_rewrite_patches(plan);

    REQUIRE_FALSE(patches);
    CHECK(patches.error().message == "missing object ID mapping for copied object 10");
    CHECK(patches.error().offset == 20);
}

TEST_CASE("build_binary_replace_elevation_source_rewrite_patches rejects missing script mappings", "[map][binary][patch]")
{
    qmap::BinaryReplaceElevationPlan plan;
    plan.source_elevation = 0;
    plan.destination_elevation = 1;
    plan.object_id_mappings.push_back({10, 100});
    qmap::BinaryPlannedObjectCopy copied_object;
    copied_object.object_id = 10;
    copied_object.script_id = 0x03000010;
    copied_object.raw = qmap::Range{20, 88};
    plan.copied_objects.push_back(copied_object);

    const auto patches = qmap::build_binary_replace_elevation_source_rewrite_patches(plan);

    REQUIRE_FALSE(patches);
    CHECK(patches.error().message == "missing script ID mapping for copied object script 50331664");
    CHECK(patches.error().offset == 20);
}

TEST_CASE("build_binary_replace_elevation_source_rewrite_patches rejects spatial scripts without tile offsets", "[map][binary][patch]")
{
    qmap::BinaryReplaceElevationPlan plan;
    plan.source_elevation = 0;
    plan.destination_elevation = 1;
    plan.script_id_mappings.push_back({0x01000005, 0x01000040});
    qmap::BinaryPlannedScriptCopy spatial;
    spatial.script_id = 0x01000005;
    spatial.script_type = qmap::BinaryScriptType::spatial;
    spatial.raw = qmap::Range{40, 72};
    spatial.offsets.scr_id = 40;
    plan.copied_scripts.push_back(spatial);

    const auto patches = qmap::build_binary_replace_elevation_source_rewrite_patches(plan);

    REQUIRE_FALSE(patches);
    CHECK(patches.error().message == "copied spatial script is missing a spatial tile offset");
    CHECK(patches.error().offset == 40);
}

TEST_CASE("patch_binary_replace_elevation_object_counts writes total and per-elevation counts", "[map][binary][patch]")
{
    const std::vector<std::byte> bytes(32, std::byte{0xAA});
    qmap::BinaryReplaceElevationPlan plan;
    plan.destination_total_objects_after = 12;
    plan.destination_object_counts_after = {3, 4, 5};

    const auto patched = qmap::patch_binary_replace_elevation_object_counts(bytes, 8, plan);

    REQUIRE(patched);
    CHECK(patched.value()[7] == std::byte{0xAA});
    CHECK(patched.value()[8] == std::byte{0x00});
    CHECK(patched.value()[11] == std::byte{0x0C});
    CHECK(patched.value()[12] == std::byte{0x00});
    CHECK(patched.value()[15] == std::byte{0x03});
    CHECK(patched.value()[16] == std::byte{0x00});
    CHECK(patched.value()[19] == std::byte{0x04});
    CHECK(patched.value()[20] == std::byte{0x00});
    CHECK(patched.value()[23] == std::byte{0x05});
    CHECK(patched.value()[24] == std::byte{0xAA});
}

TEST_CASE("patch_binary_replace_elevation_object_counts rejects mismatched totals", "[map][binary][patch]")
{
    const std::vector<std::byte> bytes(32);
    qmap::BinaryReplaceElevationPlan plan;
    plan.destination_total_objects_after = 99;
    plan.destination_object_counts_after = {3, 4, 5};

    const auto patched = qmap::patch_binary_replace_elevation_object_counts(bytes, 8, plan);

    REQUIRE_FALSE(patched);
    CHECK(patched.error().message == "planned destination object counts do not match total");
    CHECK(patched.error().offset == 8);
}

TEST_CASE("patch_binary_replace_elevation_object_counts rejects short object count headers", "[map][binary][patch]")
{
    const std::vector<std::byte> bytes(20);
    qmap::BinaryReplaceElevationPlan plan;
    plan.destination_total_objects_after = 12;
    plan.destination_object_counts_after = {3, 4, 5};

    const auto patched = qmap::patch_binary_replace_elevation_object_counts(bytes, 8, plan);

    REQUIRE_FALSE(patched);
    CHECK(patched.error().message == "int32 patch offset is outside the byte buffer");
    CHECK(patched.error().offset == 20);
}

TEST_CASE("patch_binary_replace_elevation_script_counts writes counts at parsed offsets", "[map][binary][patch]")
{
    const std::vector<std::byte> bytes(48, std::byte{0xAA});
    qmap::BinaryReplaceElevationPlan plan;
    plan.destination_script_counts_after = {1, 2, 3, 4, 5};
    const std::array<std::size_t, qmap::binary_script_type_count> offsets{4, 12, 20, 28, 36};

    const auto patched = qmap::patch_binary_replace_elevation_script_counts(bytes, offsets, plan);

    REQUIRE(patched);
    CHECK(patched.value()[3] == std::byte{0xAA});
    CHECK(patched.value()[4] == std::byte{0x00});
    CHECK(patched.value()[7] == std::byte{0x01});
    CHECK(patched.value()[12] == std::byte{0x00});
    CHECK(patched.value()[15] == std::byte{0x02});
    CHECK(patched.value()[20] == std::byte{0x00});
    CHECK(patched.value()[23] == std::byte{0x03});
    CHECK(patched.value()[28] == std::byte{0x00});
    CHECK(patched.value()[31] == std::byte{0x04});
    CHECK(patched.value()[36] == std::byte{0x00});
    CHECK(patched.value()[39] == std::byte{0x05});
    CHECK(patched.value()[40] == std::byte{0xAA});
}

TEST_CASE("patch_binary_replace_elevation_script_counts rejects int32 overflow", "[map][binary][patch]")
{
    const std::vector<std::byte> bytes(48);
    qmap::BinaryReplaceElevationPlan plan;
    plan.destination_script_counts_after[3] =
        static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) + 1;
    const std::array<std::size_t, qmap::binary_script_type_count> offsets{4, 12, 20, 28, 36};

    const auto patched = qmap::patch_binary_replace_elevation_script_counts(bytes, offsets, plan);

    REQUIRE_FALSE(patched);
    CHECK(patched.error().message == "planned destination script count exceeds int32 range");
    CHECK(patched.error().offset == 28);
}

TEST_CASE("patch_binary_replace_elevation_script_counts rejects short count offsets", "[map][binary][patch]")
{
    const std::vector<std::byte> bytes(38);
    qmap::BinaryReplaceElevationPlan plan;
    plan.destination_script_counts_after = {1, 2, 3, 4, 5};
    const std::array<std::size_t, qmap::binary_script_type_count> offsets{4, 12, 20, 28, 36};

    const auto patched = qmap::patch_binary_replace_elevation_script_counts(bytes, offsets, plan);

    REQUIRE_FALSE(patched);
    CHECK(patched.error().message == "int32 patch offset is outside the byte buffer");
    CHECK(patched.error().offset == 36);
}

TEST_CASE("patch_binary_replace_elevation_header_flags marks destination elevation present", "[map][binary][patch]")
{
    constexpr std::size_t map_flags_offset = 40;
    std::vector<std::byte> destination_bytes(64, std::byte{0xAA});
    write_i32_be(destination_bytes, map_flags_offset, 0xE);
    qmap::BinaryReplaceElevationPlan plan;
    plan.destination_elevation = 1;

    const auto patched = qmap::patch_binary_replace_elevation_header_flags(
        destination_bytes,
        plan
    );

    REQUIRE(patched);
    CHECK(patched.value()[map_flags_offset] == std::byte{0x00});
    CHECK(patched.value()[map_flags_offset + 1] == std::byte{0x00});
    CHECK(patched.value()[map_flags_offset + 2] == std::byte{0x00});
    CHECK(patched.value()[map_flags_offset + 3] == std::byte{0x0A});
    CHECK(patched.value()[map_flags_offset - 1] == std::byte{0xAA});
    CHECK(patched.value()[map_flags_offset + 4] == std::byte{0xAA});
}

TEST_CASE("patch_binary_replace_elevation_header_flags preserves already-present destination flags", "[map][binary][patch]")
{
    constexpr std::size_t map_flags_offset = 40;
    std::vector<std::byte> destination_bytes(64, std::byte{0xAA});
    write_i32_be(destination_bytes, map_flags_offset, 0xA);
    qmap::BinaryReplaceElevationPlan plan;
    plan.destination_elevation = 1;

    const auto patched = qmap::patch_binary_replace_elevation_header_flags(
        destination_bytes,
        plan
    );

    REQUIRE(patched);
    CHECK(patched.value()[map_flags_offset + 3] == std::byte{0x0A});
}

TEST_CASE("patch_binary_replace_elevation_header_flags rejects short destination headers", "[map][binary][patch]")
{
    const std::vector<std::byte> destination_bytes(40);
    qmap::BinaryReplaceElevationPlan plan;
    plan.destination_elevation = 0;

    const auto patched = qmap::patch_binary_replace_elevation_header_flags(
        destination_bytes,
        plan
    );

    REQUIRE_FALSE(patched);
    CHECK(patched.error().message == "destination map header is too short for map flags");
    CHECK(patched.error().offset == 40);
}

TEST_CASE("patch_binary_replace_elevation_header_flags rejects invalid destination elevation", "[map][binary][patch]")
{
    const std::vector<std::byte> destination_bytes(64);
    qmap::BinaryReplaceElevationPlan plan;
    plan.destination_elevation = 3;

    const auto patched = qmap::patch_binary_replace_elevation_header_flags(
        destination_bytes,
        plan
    );

    REQUIRE_FALSE(patched);
    CHECK(patched.error().message == "invalid destination elevation");
}

TEST_CASE("patch_binary_replace_elevation_tiles copies source tile bytes into destination range", "[map][binary][patch]")
{
    const std::vector<std::byte> source_bytes{
        std::byte{0x10},
        std::byte{0x11},
        std::byte{0x12},
        std::byte{0x13},
        std::byte{0x14},
    };
    const std::vector<std::byte> destination_bytes{
        std::byte{0x20},
        std::byte{0x21},
        std::byte{0x22},
        std::byte{0x23},
        std::byte{0x24},
        std::byte{0x25},
    };
    qmap::BinaryReplaceElevationPlan plan;
    plan.source_tile_range = qmap::Range{1, 3};
    plan.destination_tile_range = qmap::Range{2, 3};

    const auto patched = qmap::patch_binary_replace_elevation_tiles(
        source_bytes,
        destination_bytes,
        plan
    );

    REQUIRE(patched);
    CHECK(patched.value().size() == destination_bytes.size());
    CHECK(patched.value()[0] == std::byte{0x20});
    CHECK(patched.value()[1] == std::byte{0x21});
    CHECK(patched.value()[2] == std::byte{0x11});
    CHECK(patched.value()[3] == std::byte{0x12});
    CHECK(patched.value()[4] == std::byte{0x13});
    CHECK(patched.value()[5] == std::byte{0x25});
}

TEST_CASE("patch_binary_replace_elevation_tiles rejects mismatched tile range sizes", "[map][binary][patch]")
{
    const std::vector<std::byte> source_bytes(8);
    const std::vector<std::byte> destination_bytes(8);
    qmap::BinaryReplaceElevationPlan plan;
    plan.source_tile_range = qmap::Range{0, 4};
    plan.destination_tile_range = qmap::Range{0, 5};

    const auto patched = qmap::patch_binary_replace_elevation_tiles(
        source_bytes,
        destination_bytes,
        plan
    );

    REQUIRE_FALSE(patched);
    CHECK(patched.error().message == "source and destination tile ranges differ in size");
    CHECK(patched.error().offset == 0);
}

TEST_CASE("patch_binary_replace_elevation_tiles rejects unbacked source range", "[map][binary][patch]")
{
    const std::vector<std::byte> source_bytes(8);
    const std::vector<std::byte> destination_bytes(8);
    qmap::BinaryReplaceElevationPlan plan;
    plan.source_tile_range = qmap::Range{6, 3};
    plan.destination_tile_range = qmap::Range{0, 3};

    const auto patched = qmap::patch_binary_replace_elevation_tiles(
        source_bytes,
        destination_bytes,
        plan
    );

    REQUIRE_FALSE(patched);
    CHECK(patched.error().message == "source tile range is outside the source map buffer");
    CHECK(patched.error().offset == 6);
}

TEST_CASE("patch_binary_replace_elevation_tiles rejects unbacked destination range", "[map][binary][patch]")
{
    const std::vector<std::byte> source_bytes(8);
    const std::vector<std::byte> destination_bytes(8);
    qmap::BinaryReplaceElevationPlan plan;
    plan.source_tile_range = qmap::Range{0, 3};
    plan.destination_tile_range = qmap::Range{6, 3};

    const auto patched = qmap::patch_binary_replace_elevation_tiles(
        source_bytes,
        destination_bytes,
        plan
    );

    REQUIRE_FALSE(patched);
    CHECK(patched.error().message == "destination tile range is outside the destination map buffer");
    CHECK(patched.error().offset == 6);
}

TEST_CASE("write_binary_replace_elevation_patch validates planned deleted record ranges", "[map][binary][patch]")
{
    const std::vector<std::byte> source_bytes(4);
    const std::vector<std::byte> destination_bytes(64);
    qmap::BinaryReplaceElevationPlan plan;
    plan.source_tile_range = qmap::Range{0, 4};
    plan.destination_tile_range = qmap::Range{0, 4};
    plan.destination_elevation = 0;
    plan.deleted_scripts.push_back({
        0x01000001,
        qmap::BinaryScriptType::spatial,
        qmap::Range{63, 2},
    });

    const auto written = qmap::write_binary_replace_elevation_patch({
        source_bytes,
        destination_bytes,
        plan,
    });

    REQUIRE_FALSE(written);
    CHECK(written.error().message == "removal range is outside the byte buffer");
    CHECK(written.error().offset == 63);
}

TEST_CASE("write_binary_replace_elevation_patch tolerates contained inventory deletion ranges", "[map][binary][patch]")
{
    const std::vector<std::byte> source_bytes(4);
    const std::vector<std::byte> destination_bytes(64);
    qmap::BinaryReplaceElevationPlan plan;
    plan.source_tile_range = qmap::Range{0, 4};
    plan.destination_tile_range = qmap::Range{0, 4};
    plan.destination_elevation = 0;
    plan.deleted_objects.push_back({
        10,
        qmap::BinaryObjectType::item,
        qmap::Range{20, 20},
    });
    plan.deleted_objects.push_back({
        11,
        qmap::BinaryObjectType::item,
        qmap::Range{28, 4},
    });

    const auto written = qmap::write_binary_replace_elevation_patch({
        source_bytes,
        destination_bytes,
        plan,
    });

    REQUIRE_FALSE(written);
    CHECK(
        written.error().message
        == "binary map export not implemented; use --dry-run to inspect the plan"
    );
}

TEST_CASE("write_binary_replace_elevation_patch validates planned copied record ranges", "[map][binary][patch]")
{
    const std::vector<std::byte> source_bytes(4);
    const std::vector<std::byte> destination_bytes(64);
    qmap::BinaryReplaceElevationPlan plan;
    plan.source_tile_range = qmap::Range{0, 4};
    plan.destination_tile_range = qmap::Range{0, 4};
    plan.destination_elevation = 0;
    qmap::BinaryPlannedScriptCopy copied_script;
    copied_script.script_id = 0x01000001;
    copied_script.script_type = qmap::BinaryScriptType::spatial;
    copied_script.raw = qmap::Range{3, 2};
    plan.copied_scripts.push_back(copied_script);

    const auto written = qmap::write_binary_replace_elevation_patch({
        source_bytes,
        destination_bytes,
        plan,
    });

    REQUIRE_FALSE(written);
    CHECK(written.error().message == "source insertion range is outside the source byte buffer");
    CHECK(written.error().offset == 3);
}

TEST_CASE("write_binary_replace_elevation_patch validates copied record rewrite mappings", "[map][binary][patch]")
{
    const std::vector<std::byte> source_bytes(64);
    const std::vector<std::byte> destination_bytes(64);
    qmap::BinaryReplaceElevationPlan plan;
    plan.source_tile_range = qmap::Range{0, 4};
    plan.destination_tile_range = qmap::Range{0, 4};
    plan.destination_elevation = 0;
    qmap::BinaryPlannedObjectCopy copied_object;
    copied_object.object_id = 10;
    copied_object.raw = qmap::Range{20, 20};
    copied_object.offsets.obj_id = 20;
    plan.copied_objects.push_back(copied_object);

    const auto written = qmap::write_binary_replace_elevation_patch({
        source_bytes,
        destination_bytes,
        plan,
    });

    REQUIRE_FALSE(written);
    CHECK(written.error().message == "missing object ID mapping for copied object 10");
    CHECK(written.error().offset == 20);
}

TEST_CASE("write_binary_replace_elevation_patch rejects rewrite patches outside copied source ranges", "[map][binary][patch]")
{
    const std::vector<std::byte> source_bytes(64);
    const std::vector<std::byte> destination_bytes(64);
    qmap::BinaryReplaceElevationPlan plan;
    plan.source_tile_range = qmap::Range{0, 4};
    plan.destination_tile_range = qmap::Range{0, 4};
    plan.destination_elevation = 0;
    plan.object_id_mappings.push_back({10, 100});
    qmap::BinaryPlannedObjectCopy copied_object;
    copied_object.object_id = 10;
    copied_object.raw = qmap::Range{20, 8};
    copied_object.offsets.obj_id = 40;
    plan.copied_objects.push_back(copied_object);

    const auto written = qmap::write_binary_replace_elevation_patch({
        source_bytes,
        destination_bytes,
        plan,
    });

    REQUIRE_FALSE(written);
    CHECK(written.error().message == "rewrite patch is outside copied source ranges");
    CHECK(written.error().offset == 40);
}

TEST_CASE("write_binary_replace_elevation_patch rejects rewrite patches that overrun copied source ranges", "[map][binary][patch]")
{
    const std::vector<std::byte> source_bytes(64);
    const std::vector<std::byte> destination_bytes(64);
    qmap::BinaryReplaceElevationPlan plan;
    plan.source_tile_range = qmap::Range{0, 4};
    plan.destination_tile_range = qmap::Range{0, 4};
    plan.destination_elevation = 0;
    plan.object_id_mappings.push_back({10, 100});
    qmap::BinaryPlannedObjectCopy copied_object;
    copied_object.object_id = 10;
    copied_object.raw = qmap::Range{20, 4};
    copied_object.offsets.obj_id = 22;
    plan.copied_objects.push_back(copied_object);

    const auto written = qmap::write_binary_replace_elevation_patch({
        source_bytes,
        destination_bytes,
        plan,
    });

    REQUIRE_FALSE(written);
    CHECK(written.error().message == "rewrite patch is outside copied source ranges");
    CHECK(written.error().offset == 22);
}

TEST_CASE("write_binary_replace_elevation_patch tolerates contained copied inventory ranges", "[map][binary][patch]")
{
    const std::vector<std::byte> source_bytes(64);
    const std::vector<std::byte> destination_bytes(64);
    qmap::BinaryReplaceElevationPlan plan;
    plan.source_tile_range = qmap::Range{0, 4};
    plan.destination_tile_range = qmap::Range{0, 4};
    plan.destination_elevation = 0;
    plan.object_id_mappings.push_back({10, 100});
    plan.object_id_mappings.push_back({11, 101});
    qmap::BinaryPlannedObjectCopy parent;
    parent.object_id = 10;
    parent.elevation = -1;
    parent.object_type = qmap::BinaryObjectType::item;
    parent.raw = qmap::Range{20, 20};
    parent.offsets.obj_id = 20;
    plan.copied_objects.push_back(parent);
    qmap::BinaryPlannedObjectCopy child;
    child.object_id = 11;
    child.elevation = -1;
    child.object_type = qmap::BinaryObjectType::item;
    child.raw = qmap::Range{28, 4};
    child.offsets.obj_id = 28;
    plan.copied_objects.push_back(child);

    const auto written = qmap::write_binary_replace_elevation_patch({
        source_bytes,
        destination_bytes,
        plan,
    });

    REQUIRE_FALSE(written);
    CHECK(
        written.error().message
        == "binary map export not implemented; use --dry-run to inspect the plan"
    );
}

TEST_CASE("write_binary_replace_elevation_patch inserts copied scripts at adjusted object section offset", "[map][binary][patch]")
{
    const std::vector<std::byte> source_bytes(64);
    const std::vector<std::byte> destination_bytes(96);
    qmap::BinaryReplaceElevationPlan plan;
    plan.source_tile_range = qmap::Range{0, 4};
    plan.destination_tile_range = qmap::Range{0, 4};
    plan.destination_elevation = 0;
    plan.deleted_scripts.push_back({
        0x03000001,
        qmap::BinaryScriptType::object,
        qmap::Range{20, 8},
    });
    plan.script_id_mappings.push_back({0x03000010, 0x03000040});

    qmap::BinaryPlannedScriptCopy copied_script;
    copied_script.script_id = 0x03000010;
    copied_script.script_type = qmap::BinaryScriptType::object;
    copied_script.raw = qmap::Range{10, 4};
    copied_script.offsets.scr_id = 10;
    plan.copied_scripts.push_back(copied_script);

    const auto written = qmap::write_binary_replace_elevation_patch({
        source_bytes,
        destination_bytes,
        plan,
        40,
    });

    REQUIRE_FALSE(written);
    CHECK(
        written.error().message
        == "binary map export not implemented; use --dry-run to inspect the plan"
    );
}

TEST_CASE("write_binary_replace_elevation_patch fails until binary serialization exists", "[map][binary][patch]")
{
    const std::vector<std::byte> source_bytes(4);
    const std::vector<std::byte> destination_bytes(64);
    qmap::BinaryReplaceElevationPlan plan;
    plan.source_tile_range = qmap::Range{0, 4};
    plan.destination_tile_range = qmap::Range{0, 4};
    plan.destination_elevation = 0;

    const auto written = qmap::write_binary_replace_elevation_patch({
        source_bytes,
        destination_bytes,
        plan,
    });

    REQUIRE_FALSE(written);
    CHECK(
        written.error().message
        == "binary map export not implemented; use --dry-run to inspect the plan"
    );
}
