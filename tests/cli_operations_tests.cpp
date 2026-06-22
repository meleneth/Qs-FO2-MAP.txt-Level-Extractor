#include <catch2/catch_test_macros.hpp>

#include "cli_operations.h"

#include <cstddef>
#include <filesystem>
#include <stdexcept>

TEST_CASE("single_elevation_plan selects one matching source elevation", "[cli]")
{
    const auto plan = qmap::cli::single_elevation_plan(2);

    CHECK(plan.header_side == qmap::MapSide::left);
    REQUIRE(plan.elevations[2]);
    CHECK(plan.elevations[2]->side == qmap::MapSide::left);
    CHECK(plan.elevations[2]->elevation == 2);
    CHECK_FALSE(plan.elevations[0]);
    CHECK_FALSE(plan.elevations[1]);
}

TEST_CASE("single_elevation_plan rejects invalid elevations", "[cli]")
{
    CHECK_THROWS_AS(qmap::cli::single_elevation_plan(-1), std::runtime_error);
    CHECK_THROWS_AS(qmap::cli::single_elevation_plan(3), std::runtime_error);
}

TEST_CASE("split_output_path creates stable elevation filenames", "[cli]")
{
    const auto path = qmap::cli::split_output_path("out", "vault.map.txt", 1);

    CHECK(path == std::filesystem::path("out") / "vault.map_elev1.txt");
}

TEST_CASE("apply_selection maps destination elevations to either input side", "[cli]")
{
    qmap::TextMapExportPlan plan;

    qmap::cli::apply_selection(plan, "0=L:2");
    qmap::cli::apply_selection(plan, "2=r:1");

    REQUIRE(plan.elevations[0]);
    CHECK(plan.elevations[0]->side == qmap::MapSide::left);
    CHECK(plan.elevations[0]->elevation == 2);
    REQUIRE(plan.elevations[2]);
    CHECK(plan.elevations[2]->side == qmap::MapSide::right);
    CHECK(plan.elevations[2]->elevation == 1);
    CHECK_FALSE(plan.elevations[1]);
}

TEST_CASE("apply_selection rejects malformed selections", "[cli]")
{
    qmap::TextMapExportPlan plan;

    CHECK_THROWS_AS(qmap::cli::apply_selection(plan, "0=L"), std::runtime_error);
    CHECK_THROWS_AS(qmap::cli::apply_selection(plan, "0=X:1"), std::runtime_error);
    CHECK_THROWS_AS(qmap::cli::apply_selection(plan, "3=L:1"), std::runtime_error);
    CHECK_THROWS_AS(qmap::cli::apply_selection(plan, "1=L:3"), std::runtime_error);
}

TEST_CASE("lowercase_extension normalizes input paths", "[cli]")
{
    CHECK(qmap::cli::lowercase_extension("CITY.MAP.TXT") == ".txt");
    CHECK(qmap::cli::lowercase_extension("VAULT.MAP") == ".map");
}

TEST_CASE("format_binary_map_stats summarizes modern parser output", "[cli]")
{
    qmap::BinaryMapHeader header;
    header.version = 20;
    header.filename = {'V', 'A', 'U', 'L', 'T', '.', 'M', 'A', 'P'};
    header.map_id = 42;
    header.map_flags = 4;
    header.dude_start = 123;
    header.elev_start = 2;
    header.face_start = 5;

    qmap::BinaryMapVariables variables;
    variables.map_vars = {1, 2, 3};
    variables.local_vars = {4, 5};

    qmap::BinaryMapTiles tiles;
    const std::byte tile_bytes[] = {std::byte{0xAA}, std::byte{0xBB}};
    tiles.elevations[0] = tile_bytes;

    qmap::BinaryMapScripts scripts;
    scripts.by_type[1].push_back({});
    scripts.by_type[3].push_back({});
    scripts.by_type[3].push_back({});
    scripts.end_offset = 999;
    qmap::BinaryMapObjectCounts objects;
    objects.total_count = 7;
    objects.elevation_counts = {2, 3, 2};
    objects.first_counted_elevation = 1;
    objects.data_offset = 1015;
    qmap::BinaryObjectPrefix first_object;
    first_object.pid = 0x02000001;
    first_object.elevation = 1;
    first_object.script_id = 50331649;

    qmap::BinaryObjectRecord first_record;
    first_record.prefix = first_object;
    const std::byte bytes[] = {
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{10},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{11},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{12},
    };
    first_record.tail = qmap::Range{0, sizeof(bytes)};

    const auto stats = qmap::cli::format_binary_map_stats(
        header,
        variables,
        tiles,
        scripts,
        objects,
        first_object,
        first_record,
        bytes
    );

    CHECK(stats.find("kind: binary map\n") != std::string::npos);
    CHECK(stats.find("status: parsed\n") != std::string::npos);
    CHECK(stats.find("  filename: VAULT.MAP\n") != std::string::npos);
    CHECK(stats.find("  map: 3\n") != std::string::npos);
    CHECK(stats.find("  elevation 0: tile_bytes=2\n") != std::string::npos);
    CHECK(stats.find("  elevation 1: absent\n") != std::string::npos);
    CHECK(stats.find("  spatial: 1\n") != std::string::npos);
    CHECK(stats.find("  object: 2\n") != std::string::npos);
    CHECK(stats.find("  section_end: 999\n") != std::string::npos);
    CHECK(stats.find("objects:\n") != std::string::npos);
    CHECK(stats.find("  total: 7\n") != std::string::npos);
    CHECK(stats.find("  first_counted_elevation: 1\n") != std::string::npos);
    CHECK(stats.find("  first_elevation_count: 3\n") != std::string::npos);
    CHECK(stats.find("  data_offset: 1015\n") != std::string::npos);
    CHECK(stats.find("  first_object:\n") != std::string::npos);
    CHECK(stats.find("    type: scenery\n") != std::string::npos);
    CHECK(stats.find("    elevation: 1\n") != std::string::npos);
    CHECK(stats.find("    script_id: 50331649\n") != std::string::npos);
    CHECK(stats.find("    scenery_flags: 10\n") != std::string::npos);
    CHECK(stats.find("    scenery_destination: 12\n") != std::string::npos);
}
