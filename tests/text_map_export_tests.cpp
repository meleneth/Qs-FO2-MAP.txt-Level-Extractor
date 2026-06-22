#include <catch2/catch_test_macros.hpp>

#include "text_map_export.h"

#include <string>
#include <string_view>

namespace {

struct ParsedFixture {
    std::string text;
    qmap::ParsedTextMap map;
};

ParsedFixture parse_fixture(std::string text)
{
    auto parsed = qmap::parse_text_map(text);
    REQUIRE(parsed);
    return {std::move(text), parsed.value()};
}

qmap::ParsedTextSource source_from(const ParsedFixture& fixture)
{
    return {fixture.text, fixture.map};
}

} // namespace

TEST_CASE("export_text_map writes one selected elevation with chosen header", "[txt][export]")
{
    const auto left = parse_fixture(
        "left-header\n"
        "square_elev: 0\n\n"
        "left-zero\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n"
    );
    const auto right = parse_fixture(
        "right-header\n"
        "square_elev: 1\n\n"
        "right-one\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n"
    );
    qmap::TextMapExportPlan plan;
    plan.header_side = qmap::MapSide::right;
    plan.elevations[0] = qmap::ElevationSource{qmap::MapSide::left, 0};

    const auto exported = qmap::export_text_map(source_from(left), source_from(right), plan);

    REQUIRE(exported);
    CHECK(exported.value().starts_with("right-header\r\nsquare_elev: 0\r\n\r\nleft-zero\r\n"));
    CHECK(exported.value().find(">>>>>>>>>>: SCRIPTS <<<<<<<<<<\r\n") != std::string::npos);
    CHECK(exported.value().find(">>>>>>>>>>: OBJECTS <<<<<<<<<<\r\n") != std::string::npos);
}

TEST_CASE("export_text_map can place selected elevations in new positions", "[txt][export]")
{
    const auto left = parse_fixture(
        "left-header\r\n"
        "square_elev: 0\r\n\r\n"
        "left-zero\r\n"
        "square_elev: 2\r\n\r\n"
        "left-two\r\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\r\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\r\n"
    );
    const auto right = parse_fixture(
        "right-header\r\n"
        "square_elev: 1\r\n\r\n"
        "right-one\r\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\r\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\r\n"
    );
    qmap::TextMapExportPlan plan;
    plan.elevations[0] = qmap::ElevationSource{qmap::MapSide::right, 1};
    plan.elevations[2] = qmap::ElevationSource{qmap::MapSide::left, 0};

    const auto exported = qmap::export_text_map(source_from(left), source_from(right), plan);

    REQUIRE(exported);
    CHECK(exported.value().find("square_elev: 0\r\n\r\nright-one\r\n") != std::string::npos);
    CHECK(exported.value().find("square_elev: 1\r\n\r\n") == std::string::npos);
    CHECK(exported.value().find("square_elev: 2\r\n\r\nleft-zero\r\n") != std::string::npos);
}

TEST_CASE("export_text_map rejects absent source elevations", "[txt][export]")
{
    const auto left = parse_fixture(
        "left-header\n"
        "square_elev: 0\n\n"
        "left-zero\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n"
    );
    const auto right = parse_fixture(
        "right-header\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n"
    );
    qmap::TextMapExportPlan plan;
    plan.elevations[0] = qmap::ElevationSource{qmap::MapSide::right, 1};

    const auto exported = qmap::export_text_map(source_from(left), source_from(right), plan);

    REQUIRE_FALSE(exported);
    CHECK(exported.error().message == "selected source elevation is absent");
}

TEST_CASE("export_text_map rejects invalid source elevation indexes", "[txt][export]")
{
    const auto left = parse_fixture(
        "left-header\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n"
    );
    qmap::TextMapExportPlan plan;
    plan.elevations[0] = qmap::ElevationSource{qmap::MapSide::left, 9};

    const auto exported = qmap::export_text_map(source_from(left), source_from(left), plan);

    REQUIRE_FALSE(exported);
    CHECK(exported.error().message == "invalid source elevation");
}

TEST_CASE("export_text_map copies selected objects and matching scripts", "[txt][export]")
{
    const auto left = parse_fixture(
        "left-header\n"
        "square_elev: 0\n\n"
        "left-zero\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        "SCRS:\n"
        "scr_num: 0\n"
        "scr_num: 1\n"
        "scr_id: 16777216\n"
        "scr_next: 4294967295\n"
        "scr_oid: 999\n"
        "scr_num_local_vars: 0\n\n"
        "scr_udata.sp.built_tile: 100\n\n"
        "scr_udata.sp.radius: 5\n"
        "scr_num: 0\n"
        "scr_num: 1\n"
        "scr_id: 50331649\n"
        "scr_oid: 215\n"
        "scr_num_local_vars: 0\n"
        "scr_num: 0\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n"
        "[[OBJECTS BEGIN]]\n"
        "[OBJECT BEGIN]\n"
        "obj_elev: 0\n"
        "obj_sid: 50331649\n"
        "[OBJECT END]\n"
        "[OBJECT BEGIN]\n"
        "obj_elev: 1\n"
        "obj_sid: 50331650\n"
        "[OBJECT END]\n"
        "[[OBJECTS END]]\n"
    );
    const auto right = parse_fixture(
        "right-header\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n"
    );
    qmap::TextMapExportPlan plan;
    plan.elevations[2] = qmap::ElevationSource{qmap::MapSide::left, 0};

    const auto exported = qmap::export_text_map(source_from(left), source_from(right), plan);

    REQUIRE(exported);
    CHECK(exported.value().find("scr_num: 1\r\n\r\nscr_id: 16777216\r\n") != std::string::npos);
    CHECK(exported.value().find("scr_udata.sp.built_tile: 1073741924\r\n") != std::string::npos);
    CHECK(exported.value().find("scr_num: 1\r\n\r\nscr_id: 50331649\r\n") != std::string::npos);
    CHECK(exported.value().find("scr_id: 50331650\r\n") == std::string::npos);
    CHECK(exported.value().find("obj_elev: 2\r\nobj_sid: 50331649\r\n") != std::string::npos);
    CHECK(exported.value().find("obj_sid: 50331650\r\n") == std::string::npos);
}

TEST_CASE("export_text_map rewrites only real field lines", "[txt][export]")
{
    const auto left = parse_fixture(
        "left-header\n"
        "square_elev: 0\n\n"
        "left-zero\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        "SCRS:\n"
        "scr_num: 0\n"
        "scr_num: 0\n"
        "scr_num: 0\n"
        "scr_num: 1\n"
        "scr_id: 50331649\n"
        "note_scr_id: 999\n"
        "scr_oid: 215\n"
        "scr_num_local_vars: 0\n"
        "scr_num: 0\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n"
        "[[OBJECTS BEGIN]]\n"
        "[OBJECT BEGIN]\n"
        "note_obj_elev: 9\n"
        "note_obj_sid: 999\n"
        "obj_elev: 0\n"
        "obj_sid: 50331649\n"
        "[OBJECT END]\n"
        "[[OBJECTS END]]\n"
    );
    const auto right = parse_fixture(
        "right-header\n"
        "square_elev: 0\n\n"
        "right-zero\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        "SCRS:\n"
        "scr_num: 0\n"
        "scr_num: 0\n"
        "scr_num: 0\n"
        "scr_num: 1\n"
        "scr_id: 50331649\n"
        "scr_oid: 215\n"
        "scr_num_local_vars: 0\n"
        "scr_num: 0\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n"
        "[[OBJECTS BEGIN]]\n"
        "[OBJECT BEGIN]\n"
        "obj_elev: 0\n"
        "obj_sid: 50331649\n"
        "[OBJECT END]\n"
        "[[OBJECTS END]]\n"
    );
    qmap::TextMapExportPlan plan;
    plan.elevations[0] = qmap::ElevationSource{qmap::MapSide::right, 0};
    plan.elevations[2] = qmap::ElevationSource{qmap::MapSide::left, 0};

    const auto exported = qmap::export_text_map(source_from(left), source_from(right), plan);

    REQUIRE(exported);
    CHECK(exported.value().find("note_obj_elev: 9\r\n") != std::string::npos);
    CHECK(exported.value().find("note_obj_sid: 999\r\n") != std::string::npos);
    CHECK(exported.value().find("note_scr_id: 999\r\n") != std::string::npos);
    CHECK(exported.value().find("obj_elev: 2\r\n") != std::string::npos);
    CHECK(exported.value().find("obj_sid: 50331650\r\n") != std::string::npos);
    CHECK(exported.value().find("scr_id: 50331650\r\n") != std::string::npos);
}

TEST_CASE("export_text_map reassigns duplicate object script ids consistently", "[txt][export]")
{
    const auto left = parse_fixture(
        "left-header\n"
        "square_elev: 0\n\n"
        "left-zero\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        "SCRS:\n"
        "scr_num: 0\n"
        "scr_num: 0\n"
        "scr_num: 0\n"
        "scr_num: 1\n"
        "scr_id: 50331649\n"
        "scr_oid: 215\n"
        "scr_num_local_vars: 0\n"
        "scr_num: 0\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n"
        "[[OBJECTS BEGIN]]\n"
        "[OBJECT BEGIN]\n"
        "obj_elev: 0\n"
        "obj_sid: 50331649\n"
        "[OBJECT END]\n"
        "[[OBJECTS END]]\n"
    );
    const auto right = parse_fixture(
        "right-header\n"
        "square_elev: 0\n\n"
        "right-zero\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        "SCRS:\n"
        "scr_num: 0\n"
        "scr_num: 0\n"
        "scr_num: 0\n"
        "scr_num: 1\n"
        "scr_id: 50331649\n"
        "scr_oid: 215\n"
        "scr_num_local_vars: 0\n"
        "scr_num: 0\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n"
        "[[OBJECTS BEGIN]]\n"
        "[OBJECT BEGIN]\n"
        "obj_elev: 0\n"
        "obj_sid: 50331649\n"
        "[OBJECT END]\n"
        "[[OBJECTS END]]\n"
    );
    qmap::TextMapExportPlan plan;
    plan.elevations[0] = qmap::ElevationSource{qmap::MapSide::left, 0};
    plan.elevations[1] = qmap::ElevationSource{qmap::MapSide::right, 0};

    const auto exported = qmap::export_text_map(source_from(left), source_from(right), plan);

    REQUIRE(exported);
    CHECK(exported.value().find("obj_elev: 0\r\nobj_sid: 50331649\r\n") != std::string::npos);
    CHECK(exported.value().find("obj_elev: 1\r\nobj_sid: 50331650\r\n") != std::string::npos);
    CHECK(exported.value().find("scr_id: 50331649\r\n") != std::string::npos);
    CHECK(exported.value().find("scr_id: 50331650\r\n") != std::string::npos);
}

TEST_CASE("export_text_map copies critter scripts only with owning objects", "[txt][export]")
{
    const auto left = parse_fixture(
        "left-header\n"
        "square_elev: 0\n\n"
        "left-zero\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        "SCRS:\n"
        "scr_num: 0\n"
        "scr_num: 0\n"
        "scr_num: 0\n"
        "scr_num: 0\n"
        "scr_num: 1\n"
        "scr_id: 67108865\n"
        "scr_oid: 502\n"
        "scr_num_local_vars: 0\n"
        "scr_num: 0\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n"
        "[[OBJECTS BEGIN]]\n"
        "[OBJECT BEGIN]\n"
        "obj_elev: 0\n"
        "obj_sid: 67108865\n"
        "[OBJECT END]\n"
        "[OBJECT BEGIN]\n"
        "obj_elev: 1\n"
        "obj_sid: 67108866\n"
        "[OBJECT END]\n"
        "[[OBJECTS END]]\n"
    );
    const auto right = parse_fixture(
        "right-header\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n"
    );
    qmap::TextMapExportPlan plan;
    plan.elevations[2] = qmap::ElevationSource{qmap::MapSide::left, 0};

    const auto exported = qmap::export_text_map(source_from(left), source_from(right), plan);

    REQUIRE(exported);
    CHECK(exported.value().find("obj_elev: 2\r\nobj_sid: 67108865\r\n") != std::string::npos);
    CHECK(exported.value().find("scr_id: 67108865\r\n") != std::string::npos);
    CHECK(exported.value().find("obj_sid: 67108866\r\n") == std::string::npos);
    CHECK(exported.value().find("scr_id: 67108866\r\n") == std::string::npos);
}

TEST_CASE("export_text_map reassigns duplicate spatial script ids consistently", "[txt][export]")
{
    const auto left = parse_fixture(
        "left-header\n"
        "square_elev: 1\n\n"
        "left-one\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        "SCRS:\n"
        "scr_num: 0\n"
        "scr_num: 1\n"
        "scr_id: 16777216\n"
        "scr_num_local_vars: 0\n\n"
        "scr_udata.sp.built_tile: 536870912\n\n"
        "scr_udata.sp.radius: 5\n"
        "scr_num: 0\n"
        "scr_num: 0\n"
        "scr_num: 0\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n"
        "[[OBJECTS BEGIN]]\n"
        "[[OBJECTS END]]\n"
    );
    const auto right = parse_fixture(
        "right-header\n"
        "square_elev: 1\n\n"
        "right-one\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        "SCRS:\n"
        "scr_num: 0\n"
        "scr_num: 1\n"
        "scr_id: 16777216\n"
        "scr_num_local_vars: 0\n\n"
        "scr_udata.sp.built_tile: 536870912\n\n"
        "scr_udata.sp.radius: 7\n"
        "scr_num: 0\n"
        "scr_num: 0\n"
        "scr_num: 0\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n"
        "[[OBJECTS BEGIN]]\n"
        "[[OBJECTS END]]\n"
    );
    qmap::TextMapExportPlan plan;
    plan.elevations[0] = qmap::ElevationSource{qmap::MapSide::left, 1};
    plan.elevations[2] = qmap::ElevationSource{qmap::MapSide::right, 1};

    const auto exported = qmap::export_text_map(source_from(left), source_from(right), plan);

    REQUIRE(exported);
    CHECK(exported.value().find("scr_id: 16777216\r\n") != std::string::npos);
    CHECK(exported.value().find("scr_id: 16777217\r\n") != std::string::npos);
    CHECK(exported.value().find("scr_udata.sp.built_tile: 0\r\n") != std::string::npos);
    CHECK(exported.value().find("scr_udata.sp.built_tile: 1073741824\r\n") != std::string::npos);
}

TEST_CASE("export_text_map keeps reassigned script ids within the original type", "[txt][export]")
{
    const auto left = parse_fixture(
        "left-header\n"
        "square_elev: 1\n\n"
        "left-one\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        "SCRS:\n"
        "scr_num: 0\n"
        "scr_num: 1\n"
        "scr_id: 33554431\n"
        "scr_num_local_vars: 0\n\n"
        "scr_udata.sp.built_tile: 536870912\n\n"
        "scr_udata.sp.radius: 5\n"
        "scr_num: 0\n"
        "scr_num: 0\n"
        "scr_num: 0\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n"
        "[[OBJECTS BEGIN]]\n"
        "[[OBJECTS END]]\n"
    );
    const auto right = parse_fixture(
        "right-header\n"
        "square_elev: 1\n\n"
        "right-one\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        "SCRS:\n"
        "scr_num: 0\n"
        "scr_num: 1\n"
        "scr_id: 33554431\n"
        "scr_num_local_vars: 0\n\n"
        "scr_udata.sp.built_tile: 536870912\n\n"
        "scr_udata.sp.radius: 7\n"
        "scr_num: 0\n"
        "scr_num: 0\n"
        "scr_num: 0\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n"
        "[[OBJECTS BEGIN]]\n"
        "[[OBJECTS END]]\n"
    );
    qmap::TextMapExportPlan plan;
    plan.elevations[0] = qmap::ElevationSource{qmap::MapSide::left, 1};
    plan.elevations[2] = qmap::ElevationSource{qmap::MapSide::right, 1};

    const auto exported = qmap::export_text_map(source_from(left), source_from(right), plan);

    REQUIRE(exported);
    CHECK(exported.value().find("scr_id: 33554431\r\n") != std::string::npos);
    CHECK(exported.value().find("scr_id: 16777216\r\n") != std::string::npos);
    CHECK(exported.value().find("scr_id: 33554432\r\n") == std::string::npos);
}
