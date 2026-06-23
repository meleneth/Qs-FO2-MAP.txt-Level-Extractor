#include <catch2/catch_test_macros.hpp>

#include "map_weird_scan.h"

#include <cstdint>
#include <string>
#include <vector>

namespace {

void append_i32(std::vector<std::byte>& bytes, std::int32_t value)
{
    const auto unsigned_value = static_cast<std::uint32_t>(value);
    bytes.push_back(static_cast<std::byte>((unsigned_value >> 24) & 0xFF));
    bytes.push_back(static_cast<std::byte>((unsigned_value >> 16) & 0xFF));
    bytes.push_back(static_cast<std::byte>((unsigned_value >> 8) & 0xFF));
    bytes.push_back(static_cast<std::byte>(unsigned_value & 0xFF));
}

qmap::BinaryObjectRecord object_record(
    std::int32_t obj_id,
    qmap::BinaryObjectType type,
    std::int32_t pid,
    std::int32_t script_id = -1
)
{
    qmap::BinaryObjectRecord record;
    record.object_type = type;
    record.prefix.obj_id = obj_id;
    record.prefix.pid = pid;
    record.prefix.script_id = script_id;
    record.prefix.raw = qmap::Range{static_cast<std::size_t>(obj_id), 88};
    return record;
}

qmap::BinaryScriptRecord script_record(
    qmap::BinaryScriptType type,
    std::int32_t script_id,
    std::uint32_t object_id
)
{
    qmap::BinaryScriptRecord record;
    record.type = type;
    record.scr_id = script_id;
    record.scr_obj_id = object_id;
    record.raw = qmap::Range{static_cast<std::size_t>(script_id & 0xFFFF), 64};
    return record;
}

} // namespace

TEST_CASE("scan_weird_binary_map reports missing script and dangling script object links", "[map][binary][scan]")
{
    qmap::BinaryMap map;
    map.objects.records.push_back(object_record(
        10,
        qmap::BinaryObjectType::scenery,
        0x02000001,
        0x03000010
    ));
    map.scripts.by_type[3].push_back(script_record(
        qmap::BinaryScriptType::object,
        0x03000020,
        99
    ));

    const auto report = qmap::scan_weird_binary_map(map, {});

    CHECK(report.top_level_objects == 1);
    CHECK(report.objects_including_inventory == 1);
    REQUIRE(report.issues.size() == 2);
    CHECK(report.issues[0].category == "missing-script");
    CHECK(report.issues[0].message == "object 10 references missing script 50331664");
    CHECK(report.issues[1].category == "dangling-script-object");
    CHECK(report.issues[1].message == "script 50331680 references missing object 99");
}

TEST_CASE("scan_weird_binary_map summarizes critters and inventory and reports count mismatches", "[map][binary][scan]")
{
    qmap::BinaryMap map;
    auto critter = object_record(
        20,
        qmap::BinaryObjectType::critter,
        0x01000001
    );
    critter.prefix.inventory_count = 2;
    critter.inventory.push_back(object_record(
        21,
        qmap::BinaryObjectType::item,
        0x00000001
    ));
    map.objects.records.push_back(critter);

    const auto report = qmap::scan_weird_binary_map(map, {});

    CHECK(report.critters == 1);
    CHECK(report.objects_with_inventory == 1);
    CHECK(report.inventory_items == 1);
    REQUIRE(report.issues.size() == 1);
    CHECK(report.issues[0].category == "inventory");
    CHECK(report.issues[0].message == "object 20 inventory_count=2 but parsed inventory items=1");
}

TEST_CASE("scan_weird_binary_map reports exit grids with invalid destinations", "[map][binary][scan]")
{
    std::vector<std::byte> bytes;
    append_i32(bytes, 0);
    append_i32(bytes, 42);
    append_i32(bytes, -1);
    append_i32(bytes, 4);
    append_i32(bytes, 9);

    qmap::BinaryMap map;
    auto exit_grid = object_record(
        30,
        qmap::BinaryObjectType::misc,
        0x05000010
    );
    exit_grid.tail = qmap::Range{0, bytes.size()};
    map.objects.records.push_back(exit_grid);

    const auto report = qmap::scan_weird_binary_map(map, bytes);

    CHECK(report.exit_grids == 1);
    REQUIRE(report.issues.size() == 1);
    CHECK(report.issues[0].category == "exit-grid");
    CHECK(report.issues[0].message == "exit grid object 30 points to dest_map=42 tile=-1 elevation=4 rotation=9");
}

TEST_CASE("format_weird_map_scan_report prints stable text output", "[map][binary][scan]")
{
    qmap::WeirdMapScanReport report;
    report.top_level_objects = 1;
    report.objects_including_inventory = 2;
    report.critters = 1;
    report.objects_with_inventory = 1;
    report.inventory_items = 1;
    report.exit_grids = 0;
    report.issues.push_back({
        qmap::WeirdMapIssueSeverity::error,
        "missing-script",
        "object 10 references missing script 50331664",
        10,
    });

    const auto formatted = qmap::format_weird_map_scan_report(report, "TEST.MAP");

    CHECK(formatted.find("kind: binary map weird scan\n") != std::string::npos);
    CHECK(formatted.find("file: TEST.MAP\n") != std::string::npos);
    CHECK(formatted.find("status: issues\n") != std::string::npos);
    CHECK(formatted.find("issues: 1\n") != std::string::npos);
    CHECK(
        formatted.find(
            "issue: severity=error category=missing-script offset=10 message=\"object 10 references missing script 50331664\"\n"
        ) != std::string::npos
    );
}

TEST_CASE("format_weird_map_scan_failure prints stable parse failure output", "[map][binary][scan]")
{
    const auto formatted = qmap::format_weird_map_scan_failure(
        "MISSING.MAP",
        "input read failed: unable to open input file: MISSING.MAP",
        0
    );

    CHECK(formatted == "kind: binary map weird scan\n"
                       "file: MISSING.MAP\n"
                       "status: parse failed\n"
                       "error: input read failed: unable to open input file: MISSING.MAP at offset 0\n");
}
