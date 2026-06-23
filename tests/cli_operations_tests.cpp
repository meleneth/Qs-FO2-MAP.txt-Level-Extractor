#include <catch2/catch_test_macros.hpp>

#include "cli_file_io.h"
#include "cli_operations.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <vector>

TEST_CASE("single_elevation_plan selects one matching source elevation", "[cli]")
{
    const auto plan = qmap::cli::single_elevation_plan(2);

    CHECK(plan.header_side == qmap::MapSide::left);
    REQUIRE(plan.elevations[2]);
    CHECK(plan.elevations[2]->side == qmap::MapSide::left);
    CHECK(plan.elevations[2]->elevation.value == 2);
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
    CHECK(plan.elevations[0]->elevation.value == 2);
    REQUIRE(plan.elevations[2]);
    CHECK(plan.elevations[2]->side == qmap::MapSide::right);
    CHECK(plan.elevations[2]->elevation.value == 1);
    CHECK_FALSE(plan.elevations[1]);
}

TEST_CASE("apply_selection rejects malformed selections", "[cli]")
{
    qmap::TextMapExportPlan plan;

    CHECK_THROWS_AS(qmap::cli::apply_selection(plan, "0=L"), std::runtime_error);
    CHECK_THROWS_AS(qmap::cli::apply_selection(plan, "0=LL:1"), std::runtime_error);
    CHECK_THROWS_AS(qmap::cli::apply_selection(plan, "0=X:1"), std::runtime_error);
    CHECK_THROWS_AS(qmap::cli::apply_selection(plan, "3=L:1"), std::runtime_error);
    CHECK_THROWS_AS(qmap::cli::apply_selection(plan, "1=L:3"), std::runtime_error);
    CHECK_THROWS_AS(qmap::cli::apply_selection(plan, "0x=L:1"), std::runtime_error);
    CHECK_THROWS_AS(qmap::cli::apply_selection(plan, "1=L:2x"), std::runtime_error);
}

TEST_CASE("lowercase_extension normalizes input paths", "[cli]")
{
    CHECK(qmap::cli::lowercase_extension("CITY.MAP.TXT") == ".txt");
    CHECK(qmap::cli::lowercase_extension("VAULT.MAP") == ".map");
}

TEST_CASE("write_binary_output_file writes bytes and rejects accidental overwrite", "[cli]")
{
    const auto output =
        std::filesystem::temp_directory_path() / "qmap-write-binary-output-file-test.map";
    std::filesystem::remove(output);

    const std::vector<std::byte> content{
        std::byte{0x10},
        std::byte{0x20},
        std::byte{0x30},
    };

    const auto saved = qmap::cli::write_binary_output_file(output, content, false);
    REQUIRE(saved);

    {
        std::ifstream file(output, std::ios::binary);
        REQUIRE(file);
        std::vector<unsigned char> bytes;
        char ch = 0;
        while (file.get(ch)) {
            bytes.push_back(static_cast<unsigned char>(ch));
        }
        CHECK(bytes == std::vector<unsigned char>{0x10, 0x20, 0x30});
    }

    const auto refused = qmap::cli::write_binary_output_file(output, content, false);
    REQUIRE_FALSE(refused);
    CHECK(refused.error().message.find("output file already exists:") == 0);

    const std::vector<std::byte> replacement{std::byte{0x40}};
    const auto overwritten = qmap::cli::write_binary_output_file(output, replacement, true);
    REQUIRE(overwritten);

    {
        std::ifstream file(output, std::ios::binary);
        REQUIRE(file);
        std::vector<unsigned char> bytes;
        char ch = 0;
        while (file.get(ch)) {
            bytes.push_back(static_cast<unsigned char>(ch));
        }
        CHECK(bytes == std::vector<unsigned char>{0x40});
    }

    std::filesystem::remove(output);
}

TEST_CASE("read_binary_file_result reports missing input files", "[cli]")
{
    const auto missing =
        std::filesystem::temp_directory_path() / "qmap-missing-binary-input-test.map";
    std::filesystem::remove(missing);

    const auto read = qmap::cli::read_binary_file_result(missing);

    REQUIRE_FALSE(read);
    CHECK(read.error().message.find("unable to open input file:") == 0);
    CHECK(read.error().offset == 0);
}

TEST_CASE("read_binary_file_result reads binary bytes", "[cli]")
{
    const auto input =
        std::filesystem::temp_directory_path() / "qmap-read-binary-input-test.map";
    std::filesystem::remove(input);
    {
        std::ofstream file(input, std::ios::binary);
        REQUIRE(file);
        const unsigned char bytes[] = {0x10, 0x20, 0xFF};
        file.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
    }

    const auto read = qmap::cli::read_binary_file_result(input);

    REQUIRE(read);
    REQUIRE(read.value().size() == 3);
    CHECK(read.value()[0] == std::byte{0x10});
    CHECK(read.value()[1] == std::byte{0x20});
    CHECK(read.value()[2] == std::byte{0xFF});

    std::filesystem::remove(input);
}

TEST_CASE("format_text_map_stats summarizes ranges and record counts", "[cli][stats]")
{
    constexpr std::string_view text =
        "header\r\n"
        "square_elev: 0\r\n"
        "\r\n"
        "tiles\r\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\r\n"
        "[SCRIPT BEGIN]\r\n"
        "scr_id: 50331649\r\n"
        "scr_obj_id: 1\r\n"
        "[SCRIPT END]\r\n"
        "[SCRIPT BEGIN]\r\n"
        "scr_id: 16777217\r\n"
        "scr_spatial_tile: 1\r\n"
        "scr_spatial_radius: 2\r\n"
        "[SCRIPT END]\r\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\r\n"
        "[OBJECT BEGIN]\r\n"
        "obj_elev: 0\r\n"
        "obj_sid: 50331649\r\n"
        "[OBJECT END]\r\n"
        "[OBJECT BEGIN]\r\n"
        "obj_elev: 2\r\n"
        "obj_sid: 50331650\r\n"
        "[OBJECT END]\r\n";

    const auto parsed = qmap::parse_text_map(text);
    REQUIRE(parsed);

    const auto stats = qmap::cli::format_text_map_stats(text, parsed.value());

    CHECK(stats.find("kind: map txt\n") != std::string::npos);
    CHECK(stats.find("status: parsed\n") != std::string::npos);
    CHECK(stats.find("  elevation 0: offset=") != std::string::npos);
    CHECK(stats.find("  elevation 1: absent\n") != std::string::npos);
    CHECK(stats.find("text scripts:\n") != std::string::npos);
    CHECK(stats.find("  total: 2\n") != std::string::npos);
    CHECK(stats.find("  spatial: 1\n") != std::string::npos);
    CHECK(stats.find("  object: 1\n") != std::string::npos);
    CHECK(stats.find("text objects:\n") != std::string::npos);
    CHECK(stats.find("  elevation 0: 1\n") != std::string::npos);
    CHECK(stats.find("  elevation 1: 0\n") != std::string::npos);
    CHECK(stats.find("  elevation 2: 1\n") != std::string::npos);
    CHECK(stats.find("  without_elevation: 0\n") != std::string::npos);
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
    first_object.inventory_count = 2;
    first_object.inventory_size = 4;
    first_object.unknown_10 = 901;
    first_object.unknown_11 = 902;

    qmap::BinaryObjectRecord first_record;
    first_record.prefix = first_object;
    first_record.object_type = qmap::BinaryObjectType::scenery;
    first_record.inventory_quantities.push_back(3);
    qmap::BinaryObjectRecord inventory_record;
    inventory_record.object_type = qmap::BinaryObjectType::item;
    inventory_record.prefix.pid = 0x00000002;
    first_record.inventory.push_back(inventory_record);
    const std::byte bytes[] = {
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{10},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{11},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{12},
    };
    first_record.raw = qmap::Range{32, 128};
    first_record.tail = qmap::Range{0, sizeof(bytes)};

    const auto stats = qmap::cli::format_binary_map_stats(
        header,
        variables,
        tiles,
        scripts,
        objects,
        first_object,
        first_record,
        bytes,
        7,
        8
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
    CHECK(stats.find("  object_records_status: parsed\n") != std::string::npos);
    CHECK(stats.find("  object_records_parsed: 7\n") != std::string::npos);
    CHECK(stats.find("  object_records_parsed_with_inventory: 8\n") != std::string::npos);
    CHECK(stats.find("  first_object:\n") != std::string::npos);
    CHECK(stats.find("    type: scenery\n") != std::string::npos);
    CHECK(stats.find("    elevation: 1\n") != std::string::npos);
    CHECK(stats.find("    script_id: 50331649\n") != std::string::npos);
    CHECK(stats.find("    inventory_count: 2\n") != std::string::npos);
    CHECK(stats.find("    inventory_size: 4\n") != std::string::npos);
    CHECK(stats.find("    unknown_10: 901\n") != std::string::npos);
    CHECK(stats.find("    unknown_11: 902\n") != std::string::npos);
    CHECK(stats.find("    raw_range: offset=32 size=128 end=160\n") != std::string::npos);
    CHECK(stats.find("    tail_range: offset=0 size=12 end=12\n") != std::string::npos);
    CHECK(stats.find("    scenery_flags: 10\n") != std::string::npos);
    CHECK(stats.find("    scenery_destination: 12\n") != std::string::npos);
    CHECK(stats.find("    first_inventory_quantity: 3\n") != std::string::npos);
    CHECK(stats.find("    first_inventory_pid: 2\n") != std::string::npos);
    CHECK(stats.find("    first_inventory_type: item\n") != std::string::npos);
}

TEST_CASE("format_binary_map_stats reports object record diagnostics", "[cli][stats]")
{
    qmap::BinaryMapHeader header;
    header.version = 20;
    qmap::BinaryMapVariables variables;
    qmap::BinaryMapTiles tiles;
    qmap::BinaryMapScripts scripts;
    qmap::BinaryMapObjectCounts objects;

    const std::vector<qmap::Error> diagnostics{
        qmap::Error{"prototype metadata required for item PID 1", 2048},
    };

    const auto stats = qmap::cli::format_binary_map_stats(
        header,
        variables,
        tiles,
        scripts,
        objects,
        std::nullopt,
        std::nullopt,
        {},
        1,
        1,
        std::nullopt,
        diagnostics
    );

    CHECK(stats.find("  object_record_diagnostics: 1\n") != std::string::npos);
    CHECK(stats.find("  object_record_diagnostic: prototype metadata required for item PID 1 at offset 2048\n") != std::string::npos);
}

TEST_CASE("format_binary_map_stats reports incomplete object record parsing", "[cli][stats]")
{
    qmap::BinaryMapHeader header;
    header.version = 20;

    qmap::BinaryMapVariables variables;
    qmap::BinaryMapTiles tiles;
    qmap::BinaryMapScripts scripts;
    qmap::BinaryMapObjectCounts objects;
    objects.total_count = 3;

    const auto stats = qmap::cli::format_binary_map_stats(
        header,
        variables,
        tiles,
        scripts,
        objects,
        std::nullopt,
        std::nullopt,
        {},
        std::nullopt,
        std::nullopt,
        qmap::Error{"unsupported object pid type", 1234}
    );

    CHECK(stats.find("  object_records_status: incomplete\n") != std::string::npos);
    CHECK(stats.find("  object_records_error: unsupported object pid type\n") != std::string::npos);
    CHECK(stats.find("  object_records_error_offset: 1234\n") != std::string::npos);
}

TEST_CASE("format_replace_elevation_plan summarizes dry-run arguments and counts", "[cli][patch]")
{
    qmap::cli::ReplaceElevationOptions options;
    options.source = "source.map";
    options.destination = "destination.map";
    options.output = "output.map";
    options.proto_root = ".local_fallout2_data/proto";
    options.source_elevation = 0;
    options.destination_elevation = 2;
    options.dry_run = true;

    qmap::BinaryReplaceElevationPlan plan;
    plan.source_elevation = 0;
    plan.destination_elevation = 2;
    plan.destination_was_present = true;
    plan.source_tile_bytes = 40000;
    plan.destination_tile_bytes = 40000;
    plan.deleted_top_level_objects = 3;
    plan.deleted_objects_including_inventory = 4;
    plan.deleted_spatial_scripts = 1;
    plan.deleted_attached_scripts = 2;
    plan.copied_top_level_objects = 5;
    plan.copied_objects_including_inventory = 7;
    plan.copied_spatial_scripts = 2;
    plan.copied_attached_scripts = 3;
    plan.destination_total_objects_before = 8;
    plan.destination_total_objects_after = 10;
    plan.destination_object_counts_before = {2, 3, 3};
    plan.destination_object_counts_after = {2, 3, 5};
    plan.destination_script_counts_before[1] = 4;
    plan.destination_script_counts_before[3] = 6;
    plan.destination_script_counts_after[1] = 5;
    plan.destination_script_counts_after[3] = 7;
    plan.object_id_mappings.push_back({10, 100});
    plan.script_id_mappings.push_back({0x03000010, 0x03000020});
    plan.preserved_exit_grids.push_back({55, 42, 1234, 1, 3});

    const auto formatted = qmap::cli::format_replace_elevation_plan(options, plan);

    CHECK(formatted.find("kind: binary replace-elevation\n") != std::string::npos);
    CHECK(formatted.find("status: planned\n") != std::string::npos);
    CHECK(formatted.find("source: source.map\n") != std::string::npos);
    CHECK(formatted.find("destination: destination.map\n") != std::string::npos);
    CHECK(formatted.find("output: output.map\n") != std::string::npos);
    CHECK(formatted.find("source_elevation: 0\n") != std::string::npos);
    CHECK(formatted.find("destination_elevation: 2\n") != std::string::npos);
    CHECK(formatted.find("destination_was_present: true\n") != std::string::npos);
    CHECK(formatted.find("  top_level_objects: 3\n") != std::string::npos);
    CHECK(formatted.find("  objects_with_inventory: 7\n") != std::string::npos);
    CHECK(formatted.find("  object_total_before: 8\n") != std::string::npos);
    CHECK(formatted.find("  object_total_after: 10\n") != std::string::npos);
    CHECK(formatted.find("  elevation_2_objects_before: 3\n") != std::string::npos);
    CHECK(formatted.find("  elevation_2_objects_after: 5\n") != std::string::npos);
    CHECK(formatted.find("    spatial: before=4 after=5\n") != std::string::npos);
    CHECK(formatted.find("    object: before=6 after=7\n") != std::string::npos);
    CHECK(formatted.find("  object_ids: 1\n") != std::string::npos);
    CHECK(formatted.find("  script_ids: 1\n") != std::string::npos);
    CHECK(formatted.find("  object_id_mapping_preview: 1\n") != std::string::npos);
    CHECK(formatted.find("  object_id_mapping: old=10 new=100\n") != std::string::npos);
    CHECK(formatted.find("  object_id_mapping_omitted:") == std::string::npos);
    CHECK(formatted.find("  script_id_mapping_preview: 1\n") != std::string::npos);
    CHECK(formatted.find("  script_id_mapping: old=50331664 new=50331680\n") != std::string::npos);
    CHECK(formatted.find("  script_id_mapping_omitted:") == std::string::npos);
    CHECK(formatted.find("  exit_grid: object_id=55 dest_map=42 dest_tile=1234 dest_elevation=1 dest_rotation=3\n") != std::string::npos);
    CHECK(formatted.find("replace-elevation source.map destination.map output.map --source-elevation 0 --dest-elevation 2 --proto-root .local_fallout2_data/proto --dry-run\n") != std::string::npos);
}

TEST_CASE("format_replace_elevation_plan bounds id mapping previews", "[cli][patch]")
{
    qmap::cli::ReplaceElevationOptions options;
    options.source = "source.map";
    options.destination = "destination.map";
    options.output = "output.map";
    options.proto_root = ".local_fallout2_data/proto";
    options.source_elevation = 0;
    options.destination_elevation = 2;
    options.dry_run = true;

    qmap::BinaryReplaceElevationPlan plan;
    for (int index = 0; index < 12; ++index) {
        plan.object_id_mappings.push_back({index + 1, index + 100});
    }

    const auto formatted = qmap::cli::format_replace_elevation_plan(options, plan);

    CHECK(formatted.find("  object_id_mapping_preview: 10\n") != std::string::npos);
    CHECK(formatted.find("  object_id_mapping: old=1 new=100\n") != std::string::npos);
    CHECK(formatted.find("  object_id_mapping: old=10 new=109\n") != std::string::npos);
    CHECK(formatted.find("  object_id_mapping: old=11 new=110\n") == std::string::npos);
    CHECK(formatted.find("  object_id_mapping_omitted: 2\n") != std::string::npos);
}

TEST_CASE("format_replace_elevation_plan echoes force in the reproducible command", "[cli][patch]")
{
    qmap::cli::ReplaceElevationOptions options;
    options.source = "source.map";
    options.destination = "destination.map";
    options.output = "output.map";
    options.proto_root = ".local_fallout2_data/proto";
    options.source_elevation = 0;
    options.destination_elevation = 2;
    options.dry_run = true;
    options.force = true;

    qmap::BinaryReplaceElevationPlan plan;
    const auto formatted = qmap::cli::format_replace_elevation_plan(options, plan);

    CHECK(formatted.find("replace-elevation source.map destination.map output.map --source-elevation 0 --dest-elevation 2 --proto-root .local_fallout2_data/proto --dry-run --force\n") != std::string::npos);
}

TEST_CASE("format_replace_elevation_write_success reports output path and bytes", "[cli][patch]")
{
    qmap::cli::ReplaceElevationOptions options;
    options.output = "output.map";

    const auto formatted = qmap::cli::format_replace_elevation_write_success(options, 490608);

    CHECK(formatted == "kind: binary replace-elevation\n"
                       "status: written\n"
                       "output: output.map\n"
                       "bytes: 490608\n");
}

TEST_CASE("format_replace_elevation_failure reports optional offsets", "[cli][patch]")
{
    CHECK(qmap::cli::format_replace_elevation_failure("replace-elevation requires --proto-root")
        == "kind: binary replace-elevation\n"
           "status: failed\n"
           "error: replace-elevation requires --proto-root\n");

    CHECK(qmap::cli::format_replace_elevation_failure("source parse failed: bad map", 44)
        == "kind: binary replace-elevation\n"
           "status: failed\n"
           "error: source parse failed: bad map at offset 44\n");
}
